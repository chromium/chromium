// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"
#include "components/autofill/content/browser/content_autofill_driver_test_api.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_driver_router.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace autofill {

using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Gt;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SizeIs;
using ::testing::WithArg;

namespace {

const char kAppLocale[] = "en-US";

class FakeAutofillAgent : public mojom::AutofillAgent {
 public:
  FakeAutofillAgent() = default;

  ~FakeAutofillAgent() override = default;

  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(this, mojo::PendingAssociatedReceiver<mojom::AutofillAgent>(
                             std::move(handle)));
  }

  void SetQuitLoopClosure(base::OnceClosure closure) {
    quit_closure_ = std::move(closure);
  }

  // Returns the `FormData` received via mojo interface method
  // mojom::AutofillAgent::FillOrPreviewForm().
  absl::optional<FormData> GetAutofillFillFormMessage() {
    return fill_form_form_;
  }

  // Returns the `FormData` received via mojo interface method
  // mojom::AutofillAgent::PreviewForm().
  absl::optional<FormData> GetAutofillPreviewFormMessage() {
    return preview_form_form_;
  }

  // Returns data received via mojo interface method
  // mojom::AutofillAgent::FieldTypePredictionsAvailable().
  absl::optional<std::vector<FormDataPredictions>>
  GetFieldTypePredictionsAvailable() {
    return predictions_;
  }

  // Returns whether mojo interface method mojom::AutofillAgent::ClearForm() got
  // called.
  bool GetCalledClearSection() { return called_clear_section_; }

  // Returns whether mojo interface method
  // mojom::AutofillAgent::ClearPreviewedForm() got called.
  bool GetCalledClearPreviewedForm() { return called_clear_previewed_form_; }

  // Returns data received via the mojo interface method
  // `mojom::AutofillAgent::TriggerSuggestions()`.
  absl::optional<AutofillSuggestionTriggerSource>
  GetCalledTriggerSuggestionsSource(const FieldGlobalId& field) {
    if (value_renderer_id_ != field.renderer_id) {
      return absl::nullopt;
    }
    return suggestion_trigger_source_;
  }

  // Returns data received via mojo interface method
  // mojom::AutofillAgent::ApplyFieldAction(kFill).
  absl::optional<std::u16string> GetString16FillFieldWithValue(
      const FieldGlobalId& field) {
    if (value_renderer_id_ != field.renderer_id) {
      return absl::nullopt;
    }
    return value_fill_field_;
  }

  // Returns data received via mojo interface method
  // mojom::AutofillAgent::ApplyFieldAction(kPreview).
  absl::optional<std::u16string> GetString16PreviewFieldWithValue(
      const FieldGlobalId field) {
    if (value_renderer_id_ != field.renderer_id) {
      return absl::nullopt;
    }
    return value_preview_field_;
  }

  // Returns data received via mojo interface method
  // mojom::AutofillAgent::AcceptDataListSuggestion().
  absl::optional<std::u16string> GetString16AcceptDataListSuggestion(
      FieldGlobalId field) {
    if (value_renderer_id_ != field.renderer_id) {
      return absl::nullopt;
    }
    return value_accept_data_;
  }

  // mojom::AutofillAgent:
  MOCK_METHOD(void,
              TriggerFormExtractionWithResponse,
              (base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void,
              ExtractForm,
              (FormRendererId,
               base::OnceCallback<void(const std::optional<FormData>&)>),
              (override));
  MOCK_METHOD(void,
              GetPotentialLastFourCombinationsForStandaloneCvc,
              (base::OnceCallback<void(const std::vector<std::string>&)>),
              (override));

 private:
  void CallDone() {
    if (!quit_closure_.is_null())
      std::move(quit_closure_).Run();
  }

  // mojom::AutofillAgent:
  void TriggerFormExtraction() override {}

  void ApplyFormAction(mojom::ActionType action_type,
                       mojom::ActionPersistence action_persistence,
                       const FormData& form) override {
    switch (action_persistence) {
      case mojom::ActionPersistence::kPreview:
        preview_form_form_ = form;
        break;
      case mojom::ActionPersistence::kFill:
        fill_form_form_ = form;
        break;
    }
    CallDone();
  }

  void ApplyFieldAction(mojom::ActionPersistence action_persistence,
                        mojom::TextReplacement text_replacement,
                        FieldRendererId field,
                        const std::u16string& value) override {
    CHECK_EQ(text_replacement, mojom::TextReplacement::kReplaceAll)
        << "FakeAutofillAgent only supports kReplaceAll";
    value_renderer_id_ = field;
    switch (action_persistence) {
      case mojom::ActionPersistence::kPreview:
        value_preview_field_ = value;
        break;
      case mojom::ActionPersistence::kFill:
        value_fill_field_ = value;
        break;
    }
    CallDone();
  }

  void FieldTypePredictionsAvailable(
      const std::vector<FormDataPredictions>& forms) override {
    predictions_ = forms;
    CallDone();
  }

  void ClearSection() override {
    called_clear_section_ = true;
    CallDone();
  }

  void ClearPreviewedForm() override {
    called_clear_previewed_form_ = true;
    CallDone();
  }

  void TriggerSuggestions(
      FieldRendererId field,
      AutofillSuggestionTriggerSource trigger_source) override {
    value_renderer_id_ = field;
    suggestion_trigger_source_ = trigger_source;
    CallDone();
  }

  void SetSuggestionAvailability(
      FieldRendererId field,
      mojom::AutofillSuggestionAvailability suggestion_availability) override {
    value_renderer_id_ = field;
    if (suggestion_availability ==
        mojom::AutofillSuggestionAvailability::kAutofillAvailable) {
      suggestions_available_ = true;
    } else if (suggestion_availability ==
               mojom::AutofillSuggestionAvailability::kNoSuggestions) {
      suggestions_available_ = false;
    }
    CallDone();
  }

  void AcceptDataListSuggestion(FieldRendererId field,
                                const std::u16string& value) override {
    value_renderer_id_ = field;
    value_accept_data_ = value;
    CallDone();
  }

  void EnableHeavyFormDataScraping() override {}

  void PreviewPasswordSuggestion(const std::u16string& username,
                                 const std::u16string& password) override {}

  void PreviewPasswordGenerationSuggestion(
      const std::u16string& password) override {}

  void SetUserGestureRequired(bool required) override {}

  void SetSecureContextRequired(bool required) override {}

  void SetFocusRequiresScroll(bool require) override {}

  void SetQueryPasswordSuggestion(bool query) override {}

  void SetFieldsEligibleForManualFilling(
      const std::vector<FieldRendererId>& fields) override {}

  mojo::AssociatedReceiverSet<mojom::AutofillAgent> receivers_;

  base::OnceClosure quit_closure_;

  // Records data received from FillOrPreviewForm() call.
  absl::optional<FormData> fill_form_form_;
  absl::optional<FormData> preview_form_form_;
  // Records data received from FieldTypePredictionsAvailable() call.
  absl::optional<std::vector<FormDataPredictions>> predictions_;
  // Records whether ClearSection() got called.
  bool called_clear_section_ = false;
  // Records whether ClearPreviewedForm() got called.
  bool called_clear_previewed_form_ = false;
  // Records the trigger source received from a TriggerSuggestions() call.
  absl::optional<AutofillSuggestionTriggerSource> suggestion_trigger_source_;
  // Records the ID received from ApplyFieldAction(),
  // SetSuggestionAvailability(), or AcceptDataListSuggestion().
  absl::optional<FieldRendererId> value_renderer_id_;
  // Records string received from ApplyFieldAction() call.
  absl::optional<std::u16string> value_fill_field_;
  absl::optional<std::u16string> value_preview_field_;
  // Records string received from AcceptDataListSuggestion() call.
  absl::optional<std::u16string> value_accept_data_;
  // Records bool received from SetSuggestionAvailability() call.
  bool suggestions_available_;
};

}  // namespace

class MockBrowserAutofillManager : public BrowserAutofillManager {
 public:
  MockBrowserAutofillManager(AutofillDriver* driver, AutofillClient* client)
      : BrowserAutofillManager(driver, client, kAppLocale) {}
  ~MockBrowserAutofillManager() override = default;

  MOCK_METHOD(void, Reset, (), (override));
  MOCK_METHOD(bool, ShouldParseForms, (), ());
  MOCK_METHOD(void,
              OnFormsSeen,
              (const std::vector<FormData>& updated_forms,
               const std::vector<FormGlobalId>& removed_forms),
              ());
};

class ContentAutofillDriverWithFakeAutofillAgent
    : public ContentAutofillDriver {
 public:
  ContentAutofillDriverWithFakeAutofillAgent(
      content::RenderFrameHost* render_frame_host,
      ContentAutofillDriverFactory* owner)
      : ContentAutofillDriver(render_frame_host, owner) {
    blink::AssociatedInterfaceProvider* remote_interfaces =
        render_frame_host->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::AutofillAgent::Name_,
        base::BindRepeating(&FakeAutofillAgent::BindPendingReceiver,
                            base::Unretained(&agent_)));
  }

  FakeAutofillAgent& agent() { return agent_; }

 private:
  FakeAutofillAgent agent_;
};

class ContentAutofillDriverTest : public content::RenderViewHostTestHarness {
 public:
  enum class NavigationType {
    kNormal,
    kSameDocument,
    kServedFromBackForwardCache,
    kPrerenderedPageActivation,
  };

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    // This needed to keep the WebContentsObserverConsistencyChecker checks
    // happy for when AppendChild is called.
    NavigateAndCommit(GURL("https://foo.com/"));
  }

  void Navigate(NavigationType type) {
    content::MockNavigationHandle navigation_handle(GURL(), main_rfh());
    navigation_handle.set_has_committed(true);
    switch (type) {
      case NavigationType::kNormal:
        break;
      case NavigationType::kSameDocument:
        navigation_handle.set_is_same_document(true);
        break;
      case NavigationType::kServedFromBackForwardCache:
        navigation_handle.set_is_served_from_bfcache(true);
        break;
      case NavigationType::kPrerenderedPageActivation:
        navigation_handle.set_is_prerendered_page_activation(true);
        break;
    }
    factory().DidFinishNavigation(&navigation_handle);
  }

 protected:
  TestContentAutofillClient* client() {
    return autofill_client_injector_[web_contents()];
  }

  ContentAutofillDriverFactory& factory() {
    return *client()->GetAutofillDriverFactory();
  }

  AutofillDriverRouter& router() { return factory().router(); }

  ContentAutofillDriver& driver(content::RenderFrameHost* rfh = nullptr) {
    return *autofill_driver_injector_[rfh ? rfh : main_frame()];
  }

  FakeAutofillAgent& agent(content::RenderFrameHost* rfh = nullptr) {
    return autofill_driver_injector_[rfh ? rfh : main_frame()]->agent();
  }

  MockBrowserAutofillManager& manager(content::RenderFrameHost* rfh = nullptr) {
    return *autofill_manager_injector_[rfh ? rfh : main_frame()];
  }

  LocalFrameToken frame_token(content::RenderFrameHost* rfh = nullptr) {
    return LocalFrameToken((rfh ? rfh : main_frame())->GetFrameToken().value());
  }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  // `source_rfh` represents the render frame the form comes from.
  // `target_rfh` is the frame the form is routed to by AutofillDriverRouter,
  // i.e., it's either `source_rfh` itself (or `nullptr` as a shorthand) or an
  // ancester of `source_rfh`.
  FormData SeeForm(content::RenderFrameHost* source_rfh,
                   FormData form,
                   content::RenderFrameHost* target_rfh = nullptr) {
    if (!target_rfh) {
      target_rfh = source_rfh;
    }
    std::vector<FormData> augmented_forms;
    EXPECT_CALL(manager(target_rfh), OnFormsSeen(_, _))
        .WillOnce(DoAll(SaveArg<0>(&augmented_forms)));
    driver(source_rfh)
        .renderer_events()
        .FormsSeen(/*updated_forms=*/{std::move(form)},
                   /*removed_forms=*/{});
    // The augmented form has its metadata set
    // (ContentAutofillDriver::SetFrameAndFormMetaData().
    // It may be flattened.
    return augmented_forms.front();
  }

  test::AutofillUnitTestEnvironment autofill_test_environment_;

  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<ContentAutofillDriverWithFakeAutofillAgent>
      autofill_driver_injector_;
  TestAutofillManagerInjector<MockBrowserAutofillManager>
      autofill_manager_injector_;
};

class ContentAutofillDriverTestWithAddressForm
    : public ContentAutofillDriverTest {
 public:
  void SetUp() override {
    ContentAutofillDriverTest::SetUp();
    address_form_ = SeeForm(main_frame(), test::CreateTestAddressFormData());
  }

  FormData& address_form() { return address_form_; }

 private:
  FormData address_form_;
};

class ContentAutofillDriverWithMultiFrameCreditCardForm
    : public ContentAutofillDriverTest {
 public:
  static constexpr size_t kName = 0;
  static constexpr size_t kNumber = 1;
  static constexpr size_t kExp = 2;
  static constexpr size_t kCvc = 3;

  void SetUp() override {
    ContentAutofillDriverTest::SetUp();

    rfhs_[kName] = CreateChild("name");
    rfhs_[kNumber] = CreateChild("number");
    rfhs_[kExp] = CreateChild("exp");
    rfhs_[kCvc] = CreateChild("cvc");

    // We see the subframes before the ancestor frame, so the forms are not
    // flattened and not routed to an ancestor frame.
    forms_[kName] = SeeFormWithField(rfh(kName), "name");
    forms_[kNumber] = SeeFormWithField(rfh(kNumber), "number");
    forms_[kExp] = SeeFormWithField(rfh(kExp), "exp");
    forms_[kCvc] = SeeFormWithField(rfh(kCvc), "csc");

    FormData main_form;
    main_form.child_frames.resize(4);
    main_form.child_frames[kName].token = form(kName).host_frame;
    main_form.child_frames[kNumber].token = form(kNumber).host_frame;
    main_form.child_frames[kExp].token = form(kExp).host_frame;
    main_form.child_frames[kCvc].token = form(kCvc).host_frame;
    SeeForm(main_frame(), main_form);
  }

  void TearDown() override {
    rfhs_[kName] = nullptr;
    rfhs_[kNumber] = nullptr;
    rfhs_[kExp] = nullptr;
    rfhs_[kCvc] = nullptr;
    ContentAutofillDriverTest::TearDown();
  }

  content::RenderFrameHost* rfh(size_t i) { return rfhs_[i]; }
  const FormData& form(size_t i) { return forms_[i]; }
  FormGlobalId form_id(size_t i) { return form(i).global_id(); }
  FieldGlobalId field_id(size_t i) {
    return form(i).fields.front().global_id();
  }

 private:
  content::RenderFrameHost* CreateChild(std::string_view name) {
    content::RenderFrameHost* rfh =
        content::NavigationSimulator::NavigateAndCommitFromDocument(
            GURL(base::StrCat({"https://foo.com/", name})),
            content::RenderFrameHostTester::For(main_rfh())
                ->AppendChild(std::string(name)));
    // Make sure the driver (and the manager) is created as there is an early
    // return in `ContentAutofillDriverFactory::DidFinishNavigation` before
    // `DriverForFrame()` call.
    // In non-test setup this method is called during mojom bindings, see
    // `ContentAutofillDriverFactory::BindAutofillDriver`.
    factory().DriverForFrame(rfh);
    return rfh;
  }

  FormData SeeFormWithField(content::RenderFrameHost* source_rfh,
                            std::string_view name,
                            content::RenderFrameHost* target_rfh = nullptr) {
    FormData form;
    form.fields.push_back(test::CreateTestFormField(
        /*label=*/name, /*name=*/name, /*value=*/"",
        FormControlType::kInputText,
        /*autocomplete=*/base::StrCat({"cc-", name})));
    form = SeeForm(source_rfh, std::move(form), target_rfh);
    return form;
  }

  std::array<FormData, 4> forms_;
  std::array<raw_ptr<content::RenderFrameHost>, 4> rfhs_;
};

TEST_F(ContentAutofillDriverTest, NavigatedMainFrameDifferentDocument) {
  EXPECT_CALL(manager(), Reset());
  Navigate(NavigationType::kNormal);
}

TEST_F(ContentAutofillDriverTest, NavigatedMainFrameSameDocument) {
  EXPECT_CALL(manager(), Reset()).Times(0);
  Navigate(NavigationType::kSameDocument);
}

TEST_F(ContentAutofillDriverTest, NavigatedMainFrameFromBackForwardCache) {
  EXPECT_CALL(manager(), Reset()).Times(0);
  Navigate(NavigationType::kServedFromBackForwardCache);
}

TEST_F(ContentAutofillDriverTest, NavigatedMainFramePrerenderedPageActivation) {
  EXPECT_CALL(manager(), Reset()).Times(0);
  Navigate(NavigationType::kPrerenderedPageActivation);
}

TEST_F(ContentAutofillDriverTest, SetFrameAndFormMetaDataOfForm) {
  NavigateAndCommit(GURL("https://username:password@hostname/path?query#hash"));
  FormData form;
  form.fields.emplace_back();
  FormData form2 = test_api(driver()).GetFormWithFrameAndFormMetaData(form);
  test_api(driver()).SetFrameAndFormMetaData(form, nullptr);

  EXPECT_EQ(form.host_frame, frame_token());
  EXPECT_EQ(form.url, GURL("https://hostname/path"));
  EXPECT_EQ(form.full_url, GURL());
  EXPECT_EQ(form.main_frame_origin,
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_EQ(form.main_frame_origin,
            url::Origin::CreateFromNormalizedTuple("https", "hostname", 443));
  ASSERT_EQ(form.fields.size(), 1u);
  EXPECT_EQ(form.fields.front().host_frame, frame_token());

  EXPECT_EQ(form2.host_frame, form.host_frame);
  EXPECT_EQ(form2.url, form.url);
  EXPECT_EQ(form2.full_url, form.full_url);
  EXPECT_EQ(form2.main_frame_origin, form.main_frame_origin);
  ASSERT_EQ(form2.fields.size(), 1u);
  EXPECT_EQ(form2.fields.front().host_frame, form2.fields.front().host_frame);
}

// Test that forms in "about:" without parents have an empty FormData::url.
TEST_F(ContentAutofillDriverTest, SetFrameAndFormMetaDataOfForm_AboutScheme) {
  NavigateAndCommit(GURL("about:blank"));
  ASSERT_TRUE(main_rfh()->GetLastCommittedURL().IsAboutBlank());

  FormData form;
  test_api(driver()).SetFrameAndFormMetaData(form, nullptr);

  EXPECT_TRUE(form.url.is_empty());
}

// Tests that the FormData::version of forms passed to AutofillManager
// increases.
TEST_F(ContentAutofillDriverTest, SetFrameAndFormMetaDataOfForm_Version) {
  FormData form = test::CreateTestAddressFormData();
  std::vector<FormData> augmented_forms;
  EXPECT_CALL(manager(), OnFormsSeen)
      .WillOnce(DoAll(SaveArg<0>(&augmented_forms)));
  driver().renderer_events().FormsSeen(/*updated_forms=*/{form},
                                       /*removed_forms=*/{});
  ASSERT_EQ(augmented_forms.size(), 1u);
  FormVersion previous_version = augmented_forms[0].version;
  EXPECT_CALL(manager(), OnFormsSeen(ElementsAre(Field("FormData::version",
                                                       &FormData::version,
                                                       Gt(previous_version))),
                                     _));
  driver().renderer_events().FormsSeen(/*updated_forms=*/{form},
                                       /*removed_forms=*/{});
}

// Test that forms in "about:" subframes inherit the URL of their next
// non-"about:" ancestor in FormData::url.
TEST_F(ContentAutofillDriverTest,
       SetFrameAndFormMetaDataOfForm_AboutSchemeInheritsFromGrandParent) {
  NavigateAndCommit(GURL("https://username:password@hostname/path?query#hash"));
  content::RenderFrameHost* child_rfh =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("about:blank"), content::RenderFrameHostTester::For(main_rfh())
                                   ->AppendChild("child"));
  content::RenderFrameHost* grandchild_rfh =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("about:blank"),
          content::RenderFrameHostTester::For(child_rfh)->AppendChild(
              "grandchild"));
  // Unlike `autofill_driver_injector_[grandchild_rfh]`, GetForRenderFrameHost()
  // creates a driver (if none exists).
  ContentAutofillDriver* grandchild_driver =
      ContentAutofillDriver::GetForRenderFrameHost(grandchild_rfh);
  autofill_driver_injector_[grandchild_rfh];
  ASSERT_TRUE(child_rfh->GetLastCommittedURL().IsAboutBlank());
  ASSERT_TRUE(grandchild_rfh->GetLastCommittedURL().IsAboutBlank());

  FormData form;
  test_api(*grandchild_driver).SetFrameAndFormMetaData(form, nullptr);

  EXPECT_EQ(form.url, GURL("https://hostname"));
}

TEST_F(ContentAutofillDriverTest, SetFrameAndFormMetaDataOfField) {
  NavigateAndCommit(GURL("https://username:password@hostname/path?query#hash"));
  // We test that `SetFrameAndFormMetaData(form, &field) sets the meta data not
  // just of |form|'s fields but also of an additional individual |field|.
  FormData form;
  form.fields.emplace_back();
  FormFieldData field = form.fields.back();
  FormSignature signature_without_meta_data = CalculateFormSignature(form);
  test_api(driver()).SetFrameAndFormMetaData(form, &field);

  EXPECT_NE(signature_without_meta_data, CalculateFormSignature(form));
  EXPECT_EQ(field.host_frame, frame_token());
  EXPECT_EQ(field.host_form_id, form.unique_renderer_id);
  EXPECT_EQ(field.host_form_signature, CalculateFormSignature(form));

  EXPECT_EQ(field.host_frame, form.fields.front().host_frame);
  EXPECT_EQ(field.host_form_id, form.fields.front().host_form_id);
  EXPECT_EQ(field.host_form_signature, form.fields.front().host_form_signature);
}

// Tests that FormsSeen() for an updated form arrives in the AutofillManager.
// Does not test multiple frames.
TEST_F(ContentAutofillDriverTest, FormsSeen_UpdatedForm) {
  FormData form = test::CreateTestAddressFormData();
  EXPECT_CALL(manager(),
              OnFormsSeen(ElementsAre(AllOf(
                              // The received form has some frame-specific meta
                              // data set, which we don't test here.
                              Field("FormData::frame_token",
                                    &FormData::host_frame, frame_token()),
                              Field("FormData::unique_renderer_id",
                                    &FormData::unique_renderer_id,
                                    form.unique_renderer_id),
                              Field("FormData::fields", &FormData::fields,
                                    SizeIs(form.fields.size())))),
                          IsEmpty()));
  driver().renderer_events().FormsSeen(/*updated_forms=*/{form},
                                       /*removed_forms=*/{});
}

// Tests that FormsSeen() for a removed form arrives in the AutofillManager.
// Does not test multiple frames.
TEST_F(ContentAutofillDriverTest, FormsSeen_RemovedForm) {
  FormRendererId form_renderer_id = test::MakeFormRendererId();
  EXPECT_CALL(manager(),
              OnFormsSeen(IsEmpty(), ElementsAre(FormGlobalId(
                                         frame_token(), form_renderer_id))));
  driver().renderer_events().FormsSeen(/*updated_forms=*/{},
                                       /*removed_forms=*/{form_renderer_id});
}

// Tests that FormsSeen() for one updated and one removed form arrives in the
// AutofillManager.
// Does not test multiple frames.
TEST_F(ContentAutofillDriverTest, FormsSeen_UpdatedAndRemovedForm) {
  FormData form = test::CreateTestAddressFormData();
  FormRendererId other_form_renderer_id = test::MakeFormRendererId();
  EXPECT_CALL(
      manager(),
      OnFormsSeen(
          ElementsAre(AllOf(
              // The received form has some frame-specific meta data set, which
              // we don't test here.
              Field("FormData::frame_token", &FormData::host_frame,
                    frame_token()),
              Field("FormData::unique_renderer_id",
                    &FormData::unique_renderer_id, form.unique_renderer_id),
              Field("FormData::fields", &FormData::fields,
                    SizeIs(form.fields.size())))),
          ElementsAre(FormGlobalId(frame_token(), other_form_renderer_id))));
  driver().renderer_events().FormsSeen(
      /*updated_forms=*/{form},
      /*removed_forms=*/{other_form_renderer_id});
}

TEST_F(ContentAutofillDriverTestWithAddressForm,
       FormDataSentToRenderer_FillForm) {
  url::Origin triggered_origin;
  for (FormFieldData& field : address_form().fields) {
    field.origin = triggered_origin;
    field.value = u"dummy_value";
  }
  base::RunLoop run_loop;
  agent().SetQuitLoopClosure(run_loop.QuitClosure());
  driver().browser_events().ApplyFormAction(
      mojom::ActionType::kFill, mojom::ActionPersistence::kFill, address_form(),
      triggered_origin, {});

  run_loop.RunUntilIdle();

  EXPECT_FALSE(agent().GetAutofillPreviewFormMessage());
  absl::optional<FormData> output_form_data =
      agent().GetAutofillFillFormMessage();
  ASSERT_TRUE(output_form_data.has_value());
  EXPECT_TRUE(test::WithoutUnserializedData(address_form())
                  .SameFormAs(*output_form_data));
}

TEST_F(ContentAutofillDriverTestWithAddressForm,
       FormDataSentToRenderer_PreviewForm) {
  url::Origin triggered_origin;
  for (FormFieldData& field : address_form().fields) {
    field.origin = triggered_origin;
    field.value = u"dummy_value";
  }
  ASSERT_TRUE(base::ranges::all_of(address_form().fields,
                                   base::not_fn(&std::u16string::empty),
                                   &FormFieldData::value));
  base::RunLoop run_loop;
  agent().SetQuitLoopClosure(run_loop.QuitClosure());
  driver().browser_events().ApplyFormAction(
      mojom::ActionType::kFill, mojom::ActionPersistence::kPreview,
      address_form(), triggered_origin, {});

  run_loop.RunUntilIdle();

  EXPECT_FALSE(agent().GetAutofillFillFormMessage());
  absl::optional<FormData> output_form_data =
      agent().GetAutofillPreviewFormMessage();
  ASSERT_TRUE(output_form_data);
  EXPECT_TRUE(test::WithoutUnserializedData(address_form())
                  .SameFormAs(*output_form_data));
}

TEST_F(ContentAutofillDriverTest, TypePredictionsSentToRendererWhenEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kShowAutofillTypePredictions);

  FormData form = test::CreateTestAddressFormData();
  std::vector<FormData> augmented_forms;
  EXPECT_CALL(manager(), OnFormsSeen)
      .WillOnce(DoAll(SaveArg<0>(&augmented_forms)));
  driver().renderer_events().FormsSeen(/*updated_forms=*/{form},
                                       /*removed_forms=*/{});

  test_api(driver()).SetFrameAndFormMetaData(form, nullptr);
  ASSERT_EQ(augmented_forms.size(), 1u);
  EXPECT_TRUE(augmented_forms.front().SameFormAs(form));

  FormStructure form_structure(form);
  std::vector<FormStructure*> form_structures(1, &form_structure);
  std::vector<FormDataPredictions> expected_type_predictions =
      FormStructure::GetFieldTypePredictions(form_structures);

  base::RunLoop run_loop;
  agent().SetQuitLoopClosure(run_loop.QuitClosure());
  driver().browser_events().SendAutofillTypePredictionsToRenderer(
      form_structures);
  run_loop.RunUntilIdle();

  EXPECT_EQ(expected_type_predictions,
            agent().GetFieldTypePredictionsAvailable());
}

TEST_F(ContentAutofillDriverTestWithAddressForm, AcceptDataListSuggestion) {
  FieldGlobalId field = address_form().fields.front().global_id();
  std::u16string input_value(u"barfoo");

  base::RunLoop run_loop;
  agent().SetQuitLoopClosure(run_loop.QuitClosure());
  driver().browser_events().RendererShouldAcceptDataListSuggestion(field,
                                                                   input_value);
  run_loop.RunUntilIdle();

  EXPECT_EQ(input_value, agent().GetString16AcceptDataListSuggestion(field));
}

TEST_F(ContentAutofillDriverTestWithAddressForm,
       ClearFilledSectionSentToRenderer) {
  base::RunLoop run_loop;
  agent().SetQuitLoopClosure(run_loop.QuitClosure());
  driver().browser_events().RendererShouldClearFilledSection();
  run_loop.RunUntilIdle();

  EXPECT_TRUE(agent().GetCalledClearSection());
}

TEST_F(ContentAutofillDriverTestWithAddressForm,
       ClearPreviewedFormSentToRenderer) {
  base::RunLoop run_loop;
  agent().SetQuitLoopClosure(run_loop.QuitClosure());
  driver().browser_events().RendererShouldClearPreviewedForm();
  run_loop.RunUntilIdle();

  EXPECT_TRUE(agent().GetCalledClearPreviewedForm());
}

// Tests that `AutofillDriver::RendererShouldTriggerSuggestions()` calls make
// it to AutofillAgent.
TEST_F(ContentAutofillDriverTestWithAddressForm, TriggerSuggestions) {
  const FieldGlobalId field = address_form().fields.front().global_id();
  const auto input_source =
      AutofillSuggestionTriggerSource::kFormControlElementClicked;

  base::RunLoop run_loop;
  agent().SetQuitLoopClosure(run_loop.QuitClosure());
  driver().browser_events().RendererShouldTriggerSuggestions(field,
                                                             input_source);
  run_loop.RunUntilIdle();

  EXPECT_EQ(input_source, agent().GetCalledTriggerSuggestionsSource(field));
}

TEST_F(ContentAutofillDriverTestWithAddressForm, ApplyFieldAction_Fill) {
  FieldGlobalId field = address_form().fields.front().global_id();
  std::u16string input_value(u"barqux");

  base::RunLoop run_loop;
  agent().SetQuitLoopClosure(run_loop.QuitClosure());
  driver().browser_events().ApplyFieldAction(
      mojom::ActionPersistence::kFill, mojom::TextReplacement::kReplaceAll,
      field, input_value);
  run_loop.RunUntilIdle();

  EXPECT_EQ(input_value, agent().GetString16FillFieldWithValue(field));
}

TEST_F(ContentAutofillDriverTestWithAddressForm, ApplyFieldAction_Preview) {
  FieldGlobalId field = address_form().fields.front().global_id();
  std::u16string input_value(u"barqux");

  base::RunLoop run_loop;
  agent().SetQuitLoopClosure(run_loop.QuitClosure());
  driver().browser_events().ApplyFieldAction(
      mojom::ActionPersistence::kPreview, mojom::TextReplacement::kReplaceAll,
      field, input_value);
  run_loop.RunUntilIdle();

  EXPECT_EQ(input_value, agent().GetString16PreviewFieldWithValue(field));
}

TEST_F(ContentAutofillDriverTest, TriggerFormExtractionInAllFrames) {
  base::RunLoop run_loop;
  agent().SetQuitLoopClosure(run_loop.QuitClosure());
  base::OnceCallback<void(bool)> form_extraction_finished_callback;

  EXPECT_CALL(agent(), TriggerFormExtractionWithResponse)
      .WillOnce(MoveArg<0>(&form_extraction_finished_callback));
  driver().browser_events().TriggerFormExtractionInAllFrames(base::BindOnce(
      [](base::RunLoop* run_loop, bool success) { run_loop->Quit(); },
      &run_loop));
  run_loop.RunUntilIdle();

  EXPECT_FALSE(form_extraction_finished_callback.is_null());
  std::move(form_extraction_finished_callback).Run(true);
}

TEST_F(ContentAutofillDriverWithMultiFrameCreditCardForm,
       ExtractForm_NotFound) {
  using RendererResponseHandler =
      base::OnceCallback<void(const std::optional<FormData>&)>;
  using BrowserResponseHandler = AutofillDriver::BrowserFormHandler;
  EXPECT_CALL(agent(), ExtractForm)
      .WillRepeatedly(
          [](FormRendererId form_id, RendererResponseHandler callback) {
            std::move(callback).Run(std::nullopt);
          });
  base::MockCallback<BrowserResponseHandler> cb;
  EXPECT_CALL(cb, Run(IsNull(), Eq(std::nullopt)));
  driver().browser_events().ExtractForm(test::MakeFormGlobalId(), cb.Get());
}

TEST_F(ContentAutofillDriverWithMultiFrameCreditCardForm, ExtractForm_Found) {
  using RendererResponseHandler =
      base::OnceCallback<void(const std::optional<FormData>&)>;
  using BrowserResponseHandler = AutofillDriver::BrowserFormHandler;
  EXPECT_CALL(agent(rfh(kNumber)), ExtractForm)
      .WillRepeatedly(
          [this](FormRendererId form_id, RendererResponseHandler callback) {
            std::move(callback).Run(form(kNumber));
          });
  base::MockCallback<BrowserResponseHandler> cb;
  EXPECT_CALL(
      cb, Run(&driver(main_frame()),
              Optional(Field(
                  "FormData::fields", &FormData::fields,
                  ElementsAre(
                      Property("FormFieldData::global_id",
                               &FormFieldData::global_id, field_id(kName)),
                      Property("FormFieldData::global_id",
                               &FormFieldData::global_id, field_id(kNumber)),
                      Property("FormFieldData::global_id",
                               &FormFieldData::global_id, field_id(kExp)),
                      Property("FormFieldData::global_id",
                               &FormFieldData::global_id, field_id(kCvc)))))));
  driver(main_frame()).browser_events().ExtractForm(form_id(kNumber), cb.Get());
  task_environment()->RunUntilIdle();
}

TEST_F(ContentAutofillDriverTest, GetFourDigitCombinationsFromDOM_NoMatches) {
  base::RunLoop run_loop;
  auto cb =
      [](base::OnceCallback<void(const std::vector<std::string>&)> callback) {
        std::vector<std::string> matches;
        std::move(callback).Run(matches);
      };
  EXPECT_CALL(agent(), GetPotentialLastFourCombinationsForStandaloneCvc)
      .WillOnce(WithArg<0>(Invoke(cb)));

  std::vector<std::string> matches = {"dummy data"};
  driver().browser_events().GetFourDigitCombinationsFromDOM(
      base::BindLambdaForTesting([&](const std::vector<std::string>& result) {
        matches = result;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_TRUE(matches.empty());
}

TEST_F(ContentAutofillDriverTest,
       GetFourDigitCombinationsFromDOM_SuccessfulMatches) {
  base::RunLoop run_loop;
  auto cb =
      [](base::OnceCallback<void(const std::vector<std::string>&)> callback) {
        std::vector<std::string> matches = {"1234"};
        std::move(callback).Run(matches);
      };
  EXPECT_CALL(agent(), GetPotentialLastFourCombinationsForStandaloneCvc)
      .WillOnce(WithArg<0>(Invoke(cb)));
  std::vector<std::string> matches;
  driver().browser_events().GetFourDigitCombinationsFromDOM(
      base::BindLambdaForTesting([&](const std::vector<std::string>& result) {
        matches = result;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_THAT(matches, ElementsAre("1234"));
}

}  // namespace autofill
