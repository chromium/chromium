// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/core/browser/logging/stub_log_manager.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/content/browser/form_meta_data.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_filling.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"

using autofill::ParsingResult;
using autofill::PasswordFormFillData;
using base::ASCIIToUTF16;
using testing::_;
using testing::ElementsAre;
using testing::NiceMock;
using testing::Return;

namespace password_manager {

namespace {

class MockLogManager : public autofill::StubLogManager {
 public:
  MOCK_METHOD(bool, IsLoggingActive, (), (const override));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  MockPasswordManagerClient(const MockPasswordManagerClient&) = delete;
  MockPasswordManagerClient& operator=(const MockPasswordManagerClient&) =
      delete;
  ~MockPasswordManagerClient() override = default;

  MOCK_METHOD(autofill::LogManager*, GetLogManager, (), (override));
  MOCK_METHOD(PasswordManager*, GetPasswordManager, (), (const override));
#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
  MOCK_METHOD(void,
              CheckSafeBrowsingReputation,
              (const GURL&, const GURL&),
              (override));
#endif
};

class FakePasswordAutofillAgent
    : public autofill::mojom::PasswordAutofillAgent {
 public:
  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<autofill::mojom::PasswordAutofillAgent>(
            std::move(handle)));
  }

  bool called_set_logging_state() { return called_set_logging_state_; }

  bool logging_state_active() { return logging_state_active_; }

  void reset_data() {
    called_set_logging_state_ = false;
    logging_state_active_ = false;
  }

  // autofill::mojom::PasswordAutofillAgent:
  MOCK_METHOD(void,
              SetPasswordFillData,
              (const PasswordFormFillData&),
              (override));
  MOCK_METHOD(void,
              FillPasswordSuggestion,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(void,
              FillPasswordSuggestionById,
              (autofill::FieldRendererId,
               autofill::FieldRendererId,
               const std::u16string&,
               const std::u16string&),
              (override));
  MOCK_METHOD(void,
              PreviewPasswordSuggestionById,
              (autofill::FieldRendererId,
               autofill::FieldRendererId,
               const std::u16string&,
               const std::u16string&),
              (override));
  MOCK_METHOD(void, InformNoSavedCredentials, (bool), (override));
  MOCK_METHOD(void,
              FillIntoFocusedField,
              (bool, const std::u16string&),
              (override));
  MOCK_METHOD(void,
              PreviewField,
              (autofill::FieldRendererId, const std::u16string&),
              (override));
  MOCK_METHOD(void,
              FillField,
              (autofill::FieldRendererId, const std::u16string&),
              (override));
#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(void, KeyboardReplacingSurfaceClosed, (bool), (override));
  MOCK_METHOD(void, TriggerFormSubmission, (), (override));
#endif
  MOCK_METHOD(void,
              AnnotateFieldsWithParsingResult,
              (const ParsingResult&),
              (override));

 private:
  void SetLoggingState(bool active) override {
    called_set_logging_state_ = true;
    logging_state_active_ = active;
  }

  // Records whether SetLoggingState() gets called.
  bool called_set_logging_state_ = false;
  // Records data received via SetLoggingState() call.
  bool logging_state_active_ = false;

  mojo::AssociatedReceiver<autofill::mojom::PasswordAutofillAgent> receiver_{
      this};
};

class MockPasswordManager : public PasswordManager {
 public:
  explicit MockPasswordManager(PasswordManagerClient* client)
      : PasswordManager(client) {}
  ~MockPasswordManager() override = default;

  MOCK_METHOD(void,
              OnPasswordFormsParsed,
              (PasswordManagerDriver * driver,
               const std::vector<autofill::FormData>&),
              (override));
  MOCK_METHOD(void,
              OnPasswordFormsRendered,
              (PasswordManagerDriver * driver,
               const std::vector<autofill::FormData>&),
              (override));
  MOCK_METHOD(void,
              OnPasswordFormSubmitted,
              (PasswordManagerDriver * driver, const autofill::FormData&),
              (override));
  MOCK_METHOD(void,
              OnPasswordFormCleared,
              (PasswordManagerDriver * driver, const autofill::FormData&),
              (override));
  MOCK_METHOD(const PasswordFormCache*,
              GetPasswordFormCache,
              (),
              (const override));
};

class MockPasswordFormCache : public PasswordFormCache {
 public:
  ~MockPasswordFormCache() override = default;

  MOCK_METHOD(const PasswordForm*,
              GetPasswordForm,
              (PasswordManagerDriver*, autofill::FormRendererId),
              (const override));
  MOCK_METHOD(const PasswordForm*,
              GetPasswordForm,
              (PasswordManagerDriver*, autofill::FieldRendererId),
              (const override));
};

PasswordFormFillData GetTestPasswordFormFillData() {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.url = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.scheme = PasswordForm::Scheme::kHtml;
  form_on_page.form_data.set_host_frame(autofill::LocalFrameToken(
      base::UnguessableToken::CreateForTesting(98765, 43210)));

  // Create an exact match in the database.
  PasswordForm preferred_match = form_on_page;
  preferred_match.username_element = u"username";
  preferred_match.username_value = u"test@gmail.com";
  preferred_match.password_element = u"password";
  preferred_match.password_value = u"test";
  preferred_match.match_type = PasswordForm::MatchType::kExact;

  std::vector<PasswordForm> matches;
  PasswordForm non_preferred_match = preferred_match;
  non_preferred_match.username_value = u"test1@gmail.com";
  non_preferred_match.password_value = u"test1";
  non_preferred_match.match_type = PasswordForm::MatchType::kPSL;
  matches.push_back(std::move(non_preferred_match));

  url::Origin page_origin = url::Origin::Create(GURL("https://foo.com/"));

  return CreatePasswordFormFillData(form_on_page, matches, preferred_match,
                                    page_origin, /*wait_for_username=*/true,
                                    /*suggestion_banned_fields=*/{});
}

MATCHER(WerePasswordsCleared, "Passwords not cleared") {
  if (!arg.preferred_login.password_value.empty()) {
    return false;
  }

  for (auto& credentials : arg.additional_logins) {
    if (!credentials.password_value.empty()) {
      return false;
    }
  }
  return true;
}

MATCHER_P(FormDataEqualTo, form_data, "") {
  return autofill::FormData::DeepEqual(arg, form_data);
}

}  // namespace

class ContentPasswordManagerDriverTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ON_CALL(password_manager_client_, GetLogManager())
        .WillByDefault(Return(&log_manager_));

    blink::AssociatedInterfaceProvider* remote_interfaces =
        web_contents()->GetPrimaryMainFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        autofill::mojom::PasswordAutofillAgent::Name_,
        base::BindRepeating(&FakePasswordAutofillAgent::BindPendingReceiver,
                            base::Unretained(&fake_agent_)));
  }

  bool WasLoggingActivationMessageSent(bool* activation_flag) {
    base::RunLoop().RunUntilIdle();
    if (!fake_agent_.called_set_logging_state())
      return false;

    if (activation_flag)
      *activation_flag = fake_agent_.logging_state_active();
    fake_agent_.reset_data();
    return true;
  }

 protected:
  NiceMock<MockLogManager> log_manager_;
  NiceMock<MockPasswordManagerClient> password_manager_client_;
  FakePasswordAutofillAgent fake_agent_;
};

TEST_P(ContentPasswordManagerDriverTest, SendLoggingStateInCtor) {
  const bool should_allow_logging = GetParam();
  EXPECT_CALL(log_manager_, IsLoggingActive())
      .WillRepeatedly(Return(should_allow_logging));
  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_));

  if (should_allow_logging) {
    bool logging_activated = false;
    EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_activated));
    EXPECT_TRUE(logging_activated);
  } else {
    bool logging_activated = true;
    EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_activated));
    EXPECT_FALSE(logging_activated);
  }
}

TEST_P(ContentPasswordManagerDriverTest, SendLoggingStateAfterLogManagerReady) {
  const bool should_allow_logging = GetParam();
  EXPECT_CALL(password_manager_client_, GetLogManager())
      .WillOnce(Return(nullptr));
  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_));
  // Because log manager is not ready yet, should have no logging state sent.
  EXPECT_FALSE(WasLoggingActivationMessageSent(nullptr));

  // Log manager is ready, send logging state actually.
  EXPECT_CALL(password_manager_client_, GetLogManager())
      .WillOnce(Return(&log_manager_));
  EXPECT_CALL(log_manager_, IsLoggingActive())
      .WillRepeatedly(Return(should_allow_logging));
  driver->SendLoggingAvailability();
  bool logging_activated = false;
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_activated));
  EXPECT_EQ(should_allow_logging, logging_activated);
}

TEST_F(ContentPasswordManagerDriverTest, ClearPasswordsOnAutofill) {
  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_));

  PasswordFormFillData fill_data = GetTestPasswordFormFillData();
  fill_data.wait_for_username = true;
  EXPECT_CALL(fake_agent_, SetPasswordFillData(WerePasswordsCleared()));
  driver->SetPasswordFillData(fill_data);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentPasswordManagerDriverTest, SetFrameAndFormMetaDataOfForm) {
  NavigateAndCommit(GURL("https://username:password@hostname/path?query#hash"));

  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_));
  autofill::FormData form;
  autofill::FormData form2 = GetFormWithFrameAndFormMetaData(main_rfh(), form);

  EXPECT_EQ(
      form2.host_frame(),
      autofill::LocalFrameToken(
          web_contents()->GetPrimaryMainFrame()->GetFrameToken().value()));
  EXPECT_EQ(form2.url(), GURL("https://hostname/path"));
  EXPECT_EQ(form2.full_url(), GURL("https://hostname/path?query#hash"));
  EXPECT_EQ(form2.main_frame_origin(),
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_EQ(form2.main_frame_origin(),
            url::Origin::CreateFromNormalizedTuple("https", "hostname", 443));
}

TEST_P(ContentPasswordManagerDriverTest, LogFilledFieldTypeMetric) {
  base::HistogramTester histogram_tester;
  MockPasswordManager password_manager_{&password_manager_client_};
  MockPasswordFormCache password_form_cache_;
  PasswordForm form;
  bool field_part_of_password_form = GetParam();

  ON_CALL(password_manager_client_, GetPasswordManager())
      .WillByDefault(Return(&password_manager_));
  ON_CALL(password_manager_, GetPasswordFormCache())
      .WillByDefault(Return(&password_form_cache_));
  ON_CALL(password_form_cache_, GetPasswordForm(_, autofill::FieldRendererId()))
      .WillByDefault(Return(field_part_of_password_form ? &form : nullptr));

  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_));

  driver->FillField(u"password");
  histogram_tester.ExpectUniqueSample("Autofill.FilledFieldType.Password",
                                      field_part_of_password_form, 1);

  driver->FillSuggestion(u"username", u"password");
  histogram_tester.ExpectUniqueSample("Autofill.FilledFieldType.Password",
                                      field_part_of_password_form, 2);

  driver->FillSuggestionById(autofill::FieldRendererId(),
                             autofill::FieldRendererId(), u"username",
                             u"password");
  histogram_tester.ExpectUniqueSample("Autofill.FilledFieldType.Password",
                                      field_part_of_password_form, 3);

  driver->FillIntoFocusedField(true, u"password");
  histogram_tester.ExpectUniqueSample("Autofill.FilledFieldType.Password",
                                      field_part_of_password_form, 4);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ContentPasswordManagerDriverTest,
                         testing::Bool());

class ContentPasswordManagerDriverURLTest
    : public ContentPasswordManagerDriverTest {
 public:
  void SetUp() override {
    ContentPasswordManagerDriverTest::SetUp();
    ON_CALL(password_manager_client_, GetPasswordManager())
        .WillByDefault(Return(&password_manager_));
    driver_ = std::make_unique<ContentPasswordManagerDriver>(
        main_rfh(), &password_manager_client_);
    NavigateAndCommit(
        GURL("https://username:password@hostname/path?query#hash"));
  }

  void TearDown() override {
    driver_.reset();
    ContentPasswordManagerDriverTest::TearDown();
  }

  autofill::FormData ExpectedFormData() {
    autofill::FormData expected_form;
    expected_form.set_url(GURL("https://hostname/path"));
    expected_form.set_full_url(GURL("https://hostname/path?query#hash"));
    expected_form.set_main_frame_origin(
        url::Origin::CreateFromNormalizedTuple("https", "hostname", 443));
    expected_form.set_host_frame(autofill::LocalFrameToken(
        web_contents()->GetPrimaryMainFrame()->GetFrameToken().value()));
    return expected_form;
  }

  autofill::mojom::PasswordManagerDriver* driver() {
    return static_cast<autofill::mojom::PasswordManagerDriver*>(driver_.get());
  }

 protected:
  MockPasswordManager password_manager_{&password_manager_client_};
  std::unique_ptr<ContentPasswordManagerDriver> driver_;
};

TEST_F(ContentPasswordManagerDriverURLTest, PasswordFormsParsed) {
  autofill::FormData form;
  form.set_url(GURL("http://evil.com"));
  form.set_full_url(GURL("http://evil.com/path"));

  EXPECT_CALL(password_manager_,
              OnPasswordFormsParsed(
                  _, ElementsAre(FormDataEqualTo(ExpectedFormData()))));

  driver()->PasswordFormsParsed({form});
}

TEST_F(ContentPasswordManagerDriverURLTest, PasswordFormsRendered) {
  autofill::FormData form;
  form.set_url(GURL("http://evil.com"));
  form.set_full_url(GURL("http://evil.com/path"));

  EXPECT_CALL(password_manager_,
              OnPasswordFormsRendered(
                  _, ElementsAre(FormDataEqualTo(ExpectedFormData()))));

  driver()->PasswordFormsRendered({form});
}

TEST_F(ContentPasswordManagerDriverURLTest, PasswordFormSubmitted) {
  autofill::FormData form;
  form.set_url(GURL("http://evil.com"));
  form.set_full_url(GURL("http://evil.com/path"));

  EXPECT_CALL(password_manager_,
              OnPasswordFormSubmitted(_, FormDataEqualTo(ExpectedFormData())));

  driver()->PasswordFormSubmitted(form);
}

TEST_F(ContentPasswordManagerDriverURLTest, PasswordFormCleared) {
  autofill::FormData form;
  form.set_url(GURL("http://evil.com"));
  form.set_full_url(GURL("http://evil.com/path"));

  EXPECT_CALL(password_manager_,
              OnPasswordFormCleared(_, FormDataEqualTo(ExpectedFormData())));

  driver()->PasswordFormCleared(form);
}

class ContentPasswordManagerDriverFencedFramesTest
    : public ContentPasswordManagerDriverTest {
 public:
  ContentPasswordManagerDriverFencedFramesTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~ContentPasswordManagerDriverFencedFramesTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ContentPasswordManagerDriverFencedFramesTest,
       SetFrameAndFormMetaDataOfForm) {
  NavigateAndCommit(GURL("https://test.org"));

  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_));

  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();

  // Navigate a fenced frame.
  GURL fenced_frame_url = GURL("https://hostname/path?query#hash");
  std::unique_ptr<content::NavigationSimulator> navigation_simulator =
      content::NavigationSimulator::CreateRendererInitiated(fenced_frame_url,
                                                            fenced_frame_root);
  navigation_simulator->Commit();
  fenced_frame_root = navigation_simulator->GetFinalRenderFrameHost();

  autofill::FormData initial_form;
  autofill::FormData form_in_fenced_frame =
      GetFormWithFrameAndFormMetaData(fenced_frame_root, initial_form);

  // Verify all form data that are filled from a fenced frame's render frame
  // host, not from the primary main frame.
  EXPECT_EQ(
      form_in_fenced_frame.host_frame(),
      autofill::LocalFrameToken(fenced_frame_root->GetFrameToken().value()));
  EXPECT_EQ(form_in_fenced_frame.url(), GURL("https://hostname/path"));
  EXPECT_EQ(form_in_fenced_frame.full_url(),
            GURL("https://hostname/path?query#hash"));

  EXPECT_EQ(form_in_fenced_frame.main_frame_origin(),
            fenced_frame_root->GetLastCommittedOrigin());
  EXPECT_NE(form_in_fenced_frame.main_frame_origin(),
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_EQ(form_in_fenced_frame.main_frame_origin(),
            url::Origin::CreateFromNormalizedTuple("https", "hostname", 443));
}

TEST_F(ContentPasswordManagerDriverTest,
       PasswordAutofillDisabledOnCredentiallessIframe) {
  NavigateAndCommit(GURL("https://test.org"));

  content::RenderFrameHost* credentialless_iframe_root =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendCredentiallessChild("credentialless_iframe");

  // Navigate a credentialless iframe.
  GURL credentialless_iframe_url = GURL("https://hostname/path?query#hash");
  std::unique_ptr<content::NavigationSimulator> navigation_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          credentialless_iframe_url, credentialless_iframe_root);
  navigation_simulator->Commit();
  content::RenderFrameHost* credentialless_rfh_1 =
      navigation_simulator->GetFinalRenderFrameHost();

  // Install a the PasswordAutofillAgent mock. Verify it do not receive commands
  // from the browser side.
  FakePasswordAutofillAgent credentialless_fake_agent;
  EXPECT_CALL(credentialless_fake_agent, SetPasswordFillData(_)).Times(0);
  credentialless_rfh_1->GetRemoteAssociatedInterfaces()
      ->OverrideBinderForTesting(
          autofill::mojom::PasswordAutofillAgent::Name_,
          base::BindRepeating(&FakePasswordAutofillAgent::BindPendingReceiver,
                              base::Unretained(&credentialless_fake_agent)));

  autofill::FormData initial_form;
  autofill::FormData form_in_credentialless_iframe =
      GetFormWithFrameAndFormMetaData(credentialless_rfh_1, initial_form);

  // Verify autofill can not be triggered by browser side.
  std::unique_ptr<ContentPasswordManagerDriver> driver(
      std::make_unique<ContentPasswordManagerDriver>(
          credentialless_rfh_1, &password_manager_client_));
  driver->SetPasswordFillData(GetTestPasswordFormFillData());
  base::RunLoop().RunUntilIdle();
}

}  // namespace password_manager
