// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/collect_user_data_action.h"

#include <algorithm>
#include <codecvt>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/cud_condition.pb.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/field_formatter.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"
#include "components/autofill_assistant/browser/public/password_change/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "components/autofill_assistant/browser/ukm_test_util.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/value_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {
namespace {

const char kFakeUrl[] = "https://www.example.com";
const char kFakeUsername[] = "user@example.com";
const char kFakePassword[] = "example_password";

const char kMemoryLocation[] = "address";

// AutofillClock override that guarantees increasing time.
class ScopedAutofillClockOverride : public autofill::TestAutofillClock {
 public:
  ScopedAutofillClockOverride()
      : autofill::TestAutofillClock(std::make_unique<IncreasingClock>()) {}

 private:
  class IncreasingClock : public base::SimpleTestClock {
   public:
    IncreasingClock() { SetNow(base::Time::Now()); }

    IncreasingClock(const IncreasingClock&) = delete;
    IncreasingClock& operator=(const IncreasingClock&) = delete;

    base::Time Now() const override {
      return base::SimpleTestClock::Now() + base::Milliseconds(delta_++);
    }

   private:
    mutable int delta_ = 0;
  };
};

MATCHER_P(MatchingAutofillVariant, guid, "") {
  if (absl::holds_alternative<const autofill::AutofillProfile*>(arg)) {
    return absl::get<const autofill::AutofillProfile*>(arg)->guid() == guid;
  }
  if (absl::holds_alternative<const autofill::CreditCard*>(arg)) {
    return absl::get<const autofill::CreditCard*>(arg)->guid() == guid;
  }
  return false;
}

MATCHER_P(MatchesProfile, profile, "") {
  return arg.guid() == profile.guid() && arg.Compare(profile) == 0;
}

MATCHER_P(MatchesContact, profile, "") {
  return arg.profile->guid() == profile.guid() &&
         arg.profile->Compare(profile) == 0;
}

MATCHER_P(MatchesAddress, profile, "") {
  return arg.profile->guid() == profile.guid() &&
         arg.profile->Compare(profile) == 0;
}

MATCHER_P(MatchesCard, card, "") {
  return arg.guid() == card.guid() && arg.Compare(card) == 0;
}

RequiredDataPiece MakeRequiredDataPiece(autofill::ServerFieldType field) {
  RequiredDataPiece required_data_piece;
  required_data_piece.mutable_condition()->set_key(static_cast<int>(field));
  required_data_piece.mutable_condition()->mutable_not_empty();
  return required_data_piece;
}

AutofillEntryProto MakeAutofillEntry(const std::string& value,
                                     bool raw = false) {
  AutofillEntryProto entry;
  entry.set_value(value);
  entry.set_raw(raw);
  return entry;
}

void AddCompleteAddressEntriesToMap(
    const std::string& name,
    google::protobuf::Map<int32_t, AutofillEntryProto>* values) {
  (*values)[7] = MakeAutofillEntry(name);
  (*values)[30] = MakeAutofillEntry("Brandschenkestrasse 110");
  (*values)[35] = MakeAutofillEntry("8002");
  (*values)[33] = MakeAutofillEntry("Zurich");
  (*values)[36] = MakeAutofillEntry("CH");
}

void AddCompleteCardEntriesToMap(
    const std::string& name,
    google::protobuf::Map<int32_t, AutofillEntryProto>* values) {
  (*values)[51] = MakeAutofillEntry(name);
  (*values)[53] = MakeAutofillEntry("8");
  (*values)[55] = MakeAutofillEntry("2050");
}

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::Key;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

}  // namespace

class CollectUserDataActionTest : public testing::Test {
 public:
  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);
    content::WebContentsTester::For(web_contents_.get())
        ->SetLastCommittedURL(GURL(kFakeUrl));
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents_.get());
    source_id_ = web_contents_->GetPrimaryMainFrame()->GetPageUkmSourceId();

    if (!base::TimeTicks::IsHighResolution()) {
      // AutofillClock is used to initialize |use_date| for user data created in
      // tests. |use_date| is expected to be different in each case as clock
      // time is running. On machines with low resolution clock, we need to
      // provide a custom autofill clock that ensures always increasing time.
      autofill_clock_override_ =
          std::make_unique<ScopedAutofillClockOverride>();
    }

    ON_CALL(mock_action_delegate_, GetPersonalDataManager)
        .WillByDefault(Return(&mock_personal_data_manager_));
    ON_CALL(mock_action_delegate_, GetWebsiteLoginManager)
        .WillByDefault(Return(&mock_website_login_manager_));
    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
    ON_CALL(mock_action_delegate_, GetUserModel)
        .WillByDefault(Return(&user_model_));
    ON_CALL(mock_action_delegate_, WriteUserData(_))
        .WillByDefault(Invoke(
            [this](base::OnceCallback<void(UserData*, UserDataFieldChange*)>
                       write_callback) {
              UserDataFieldChange field_change = UserDataFieldChange::NONE;
              std::move(write_callback).Run(&user_data_, &field_change);
            }));
    ON_CALL(mock_action_delegate_, CollectUserData(_))
        .WillByDefault(
            Invoke([this](CollectUserDataOptions* collect_user_data_options) {
              std::move(collect_user_data_options->confirm_callback)
                  .Run(&user_data_, &user_model_);
            }));
    ON_CALL(mock_website_login_manager_, GetLoginsForUrl(_, _))
        .WillByDefault(
            RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{
                WebsiteLoginManager::Login(GURL(kFakeUrl), kFakeUsername)}));
    ON_CALL(mock_website_login_manager_, GetPasswordForLogin(_, _))
        .WillByDefault(RunOnceCallback<1>(true, kFakePassword));
    ON_CALL(mock_action_delegate_, GetWebContents())
        .WillByDefault(Return(web_contents_.get()));
    ON_CALL(mock_action_delegate_, GetUkmRecorder())
        .WillByDefault(Return(&ukm_recorder_));
    ON_CALL(mock_action_delegate_, MustUseBackendData())
        .WillByDefault(Return(false));
  }

  void ExpectSelectedProfileMatches(const std::string& profile_name,
                                    const autofill::AutofillProfile* profile) {
    if (profile == nullptr) {
      EXPECT_EQ(user_data_.selected_address(profile_name), nullptr);
      EXPECT_EQ(user_model_.GetSelectedAutofillProfile(profile_name), nullptr);
      return;
    }

    EXPECT_THAT(user_data_.selected_address(profile_name),
                Pointee(MatchesProfile(*profile)));
    EXPECT_THAT(user_model_.GetSelectedAutofillProfile(profile_name),
                Pointee(MatchesProfile(*profile)));
  }

  void ExpectSelectedCardMatches(const autofill::CreditCard* card) {
    if (card == nullptr) {
      EXPECT_EQ(user_data_.selected_card(), nullptr);
      EXPECT_EQ(user_model_.GetSelectedCreditCard(), nullptr);
      return;
    }

    EXPECT_THAT(user_data_.selected_card(), Pointee(MatchesCard(*card)));
    EXPECT_THAT(user_model_.GetSelectedCreditCard(),
                Pointee(MatchesCard(*card)));
  }

  void AddWaitTime(CollectUserDataAction* action, base::TimeDelta delta) {
    action->action_stopwatch_.TransferToWaitTime(delta);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<ScopedAutofillClockOverride> autofill_clock_override_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  NiceMock<MockPersonalDataManager> mock_personal_data_manager_;
  NiceMock<MockWebsiteLoginManager> mock_website_login_manager_;
  NiceMock<MockActionDelegate> mock_action_delegate_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  ukm::SourceId source_id_;
  UserData user_data_;
  UserModel user_model_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CollectUserDataActionTest, FailsForMissingPrivacyText) {
  ActionProto action_proto;
  action_proto.mutable_collect_user_data();

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SucceedsForPrivacyTextPresent) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(false);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.terms_and_conditions_ = ACCEPTED;
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(
                  &CollectUserDataResultProto::is_terms_and_conditions_accepted,
                  true))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, FailsForMissingTermsAcceptTextIfRequired) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_terms_require_review_text("terms review");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, FailsForMissingTermsReviewTextIfRequired) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(false);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SucceedsForCheckboxIfReviewTextMissing) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(true);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.terms_and_conditions_ = ACCEPTED;
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(
                  &CollectUserDataResultProto::is_terms_and_conditions_accepted,
                  true))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SucceedsForAllTermsTextPresent) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(false);
  collect_user_data_proto->set_terms_require_review_text("terms review");

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.terms_and_conditions_ = ACCEPTED;
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(
                  &CollectUserDataResultProto::is_terms_and_conditions_accepted,
                  true))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, InfoSectionText) {
  const char kInfoSection[] = "Info section.";

  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_info_section_text(kInfoSection);
  collect_user_data_proto->set_request_terms_and_conditions(false);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));
  EXPECT_CALL(
      mock_action_delegate_,
      CollectUserData(Pointee(Field(&CollectUserDataOptions::info_section_text,
                                    StrEq(kInfoSection)))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, PromptIsShown) {
  const char kPrompt[] = "Some message.";

  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  collect_user_data_proto->set_prompt(kPrompt);

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(kPrompt));
  EXPECT_CALL(callback_, Run(_));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SelectLoginWithTag) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data_proto->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_tag("tag");

  // Action should fetch the logins, but not the passwords.
  EXPECT_CALL(mock_website_login_manager_, GetLoginsForUrl(GURL(kFakeUrl), _))
      .Times(1);
  EXPECT_CALL(mock_website_login_manager_, GetPasswordForLogin(_, _)).Times(0);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_model_.SetSelectedLoginChoice(
                std::make_unique<LoginChoice>(
                    collect_user_data_options->login_choices[0]),
                &user_data_);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  ProcessedActionProto captured_action;
  EXPECT_CALL(callback_, Run(_))
      .WillOnce(testing::SaveArgPointee<0>(&captured_action));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(ACTION_APPLIED, captured_action.status());
  EXPECT_EQ("", captured_action.collect_user_data_result().login_payload());
  EXPECT_EQ("tag", captured_action.collect_user_data_result().login_tag());
  EXPECT_TRUE(captured_action.collect_user_data_result().shown_to_user());
}

TEST_F(CollectUserDataActionTest, SelectLoginWithPayload) {
  // This test concentrate on the backward-compatibility case where login is
  // reported using a payload instead of a case. Other aspects of login
  // selection are covered by SelectLogin.

  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data_proto->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("payload");

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_model_.SetSelectedLoginChoice(
                std::make_unique<LoginChoice>(
                    collect_user_data_options->login_choices[0]),
                &user_data_);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  ProcessedActionProto captured_action;
  EXPECT_CALL(callback_, Run(_))
      .WillOnce(testing::SaveArgPointee<0>(&captured_action));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ("payload",
            captured_action.collect_user_data_result().login_payload());
  EXPECT_EQ("", captured_action.collect_user_data_result().login_tag());
}

TEST_F(CollectUserDataActionTest, SelectLoginMissingUsername) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data_proto->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("payload");

  ON_CALL(mock_website_login_manager_, GetLoginsForUrl(_, _))
      .WillByDefault(RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{
          WebsiteLoginManager::Login(GURL(kFakeUrl), /*username=*/"")}));
  // Action should fetch the logins, but not the passwords.
  EXPECT_CALL(mock_website_login_manager_, GetLoginsForUrl(GURL(kFakeUrl), _))
      .Times(1);
  EXPECT_CALL(mock_website_login_manager_, GetPasswordForLogin(_, _)).Times(0);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_model_.SetSelectedLoginChoice(
                std::make_unique<LoginChoice>(
                    collect_user_data_options->login_choices[0]),
                &user_data_);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(&CollectUserDataResultProto::login_payload, "payload")),
          Property(&ProcessedActionProto::collect_user_data_result,
                   Property(&CollectUserDataResultProto::shown_to_user, true)),
          Property(&ProcessedActionProto::collect_user_data_result,
                   Property(&CollectUserDataResultProto::login_missing_username,
                            true))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, LoginChoiceAutomaticIfNoOtherOptions) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data_proto->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_custom()->set_label("Guest Checkout");
  login_option->set_payload("guest");
  login_option->set_choose_automatically_if_no_stored_login(true);
  login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("password_manager");

  ON_CALL(mock_website_login_manager_, GetLoginsForUrl(_, _))
      .WillByDefault(
          RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{}));

  EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(0);
  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::login_payload,
                                    "guest")),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::shown_to_user,
                                    false))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest,
       LoginChoiceAutomaticIfNoPasswordManagerLogins) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data_proto->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_custom()->set_label("Guest Checkout");
  login_option->set_payload("guest");
  login_option->set_choose_automatically_if_no_stored_login(true);

  login_option = login_details->add_login_options();
  login_option->mutable_custom()->set_label("Manual");
  login_option->set_payload("manual");
  login_option->set_choose_automatically_if_no_stored_login(false);

  login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("password_manager");

  ON_CALL(mock_website_login_manager_, GetLoginsForUrl(_, _))
      .WillByDefault(
          RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{}));

  EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(0);
  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::login_payload,
                                    "guest")),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::shown_to_user,
                                    false))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SortLoginChoices) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data_proto->mutable_login_details();

  auto* login_option = login_details->add_login_options();
  login_option->mutable_custom()->set_label("Guest Checkout");
  login_option->set_preselection_priority(2);

  login_option = login_details->add_login_options();
  login_option->mutable_custom()->set_label("Default priority");

  login_option = login_details->add_login_options();
  login_option->mutable_custom()->set_label("VIP Checkout");
  login_option->set_preselection_priority(0);

  login_option = login_details->add_login_options();
  login_option->mutable_custom()->set_label("Full Checkout");
  login_option->set_preselection_priority(1);

  EXPECT_CALL(mock_action_delegate_, CollectUserData(_))
      .WillOnce(Invoke([=](CollectUserDataOptions* collect_user_data_options) {
        EXPECT_THAT(
            collect_user_data_options->login_choices,
            ElementsAre(AllOf(Field(&LoginChoice::label, "Default priority"),
                              Field(&LoginChoice::preselect_priority, -1)),
                        AllOf(Field(&LoginChoice::label, "VIP Checkout"),
                              Field(&LoginChoice::preselect_priority, 0)),
                        AllOf(Field(&LoginChoice::label, "Full Checkout"),
                              Field(&LoginChoice::preselect_priority, 1)),
                        AllOf(Field(&LoginChoice::label, "Guest Checkout"),
                              Field(&LoginChoice::preselect_priority, 2))));
      }));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ChooseHighestPriorityLoginChoice) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data_proto->mutable_login_details();

  auto* login_option = login_details->add_login_options();
  login_option->mutable_custom()->set_label("Guest Checkout");
  login_option->set_preselection_priority(2);

  login_option = login_details->add_login_options();
  login_option->mutable_custom()->set_label("Default priority");

  login_option = login_details->add_login_options();
  login_option->mutable_custom()->set_label("VIP Checkout");
  login_option->set_preselection_priority(0);

  EXPECT_CALL(mock_action_delegate_, CollectUserData(_))
      .WillOnce(Invoke([=](CollectUserDataOptions* collect_user_data_options) {
        EXPECT_THAT(user_data_.selected_login_choice(),
                    AllOf(Field(&LoginChoice::label, "Default priority"),
                          Field(&LoginChoice::preselect_priority, -1)));
      }));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, EarlyActionReturnIfOnlyLoginRequested) {
  ON_CALL(mock_action_delegate_, CollectUserData(_)).WillByDefault(Return());

  ActionProto action_proto;
  auto* proto = action_proto.mutable_collect_user_data();
  auto* login_details = proto->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_custom()->set_label("Guest Checkout");
  login_option->set_payload("guest");
  login_option->set_choose_automatically_if_no_stored_login(true);

  login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("password_manager");

  ON_CALL(mock_website_login_manager_, GetLoginsForUrl(_, _))
      .WillByDefault(
          RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{}));

  // Terms requested, no early return.
  proto->set_request_terms_and_conditions(true);
  proto->set_accept_terms_and_conditions_text("I accept");
  proto->set_show_terms_as_checkbox(true);
  {
    EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(1);
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  // Contact info requested, no early return.
  proto->set_request_terms_and_conditions(false);
  proto->mutable_contact_details()->set_request_payer_name(true);
  proto->mutable_contact_details()->set_contact_details_name("name");
  {
    EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(1);
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  // Shipping info requested, no early return.
  proto->clear_contact_details();
  proto->set_shipping_address_name("shipping");
  {
    EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(1);
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  // Payment info requested, no early return.
  proto->clear_shipping_address_name();
  proto->set_request_payment_method(true);
  proto->set_billing_address_name("billing");
  {
    EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(1);
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  // Generic UI model identifier set, no early return.
  proto->clear_request_payment_method();
  proto->clear_billing_address_name();
  proto->set_additional_model_identifier_to_check("identifier");
  {
    EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(1);
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  // Additional prepended input section, no early return.
  proto->clear_additional_model_identifier_to_check();
  auto* section = proto->add_additional_prepended_sections();
  section->set_title("Title");
  auto* input_field = section->mutable_text_input_section()->add_input_fields();
  input_field->set_hint("hint");
  input_field->set_input_type(TextInputProto::INPUT_TEXT);
  input_field->set_client_memory_key("key");
  {
    EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(1);
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  // Additional appended input section, no early return.
  proto->clear_additional_prepended_sections();
  section = proto->add_additional_appended_sections();
  section->set_title("title");
  auto* popup = section->mutable_popup_list_section();
  popup->set_additional_value_key("key");
  popup->add_item_names("item");
  popup->set_no_selection_error_message("error");
  {
    EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(1);
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  // Additional static section is allowed, expect early return.
  proto->clear_additional_appended_sections();
  section = proto->add_additional_appended_sections();
  section->set_title("title");
  section->mutable_static_text_section()->set_text("text");
  {
    EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(0);

    ProcessedActionProto captured_action;
    EXPECT_CALL(callback_, Run(_))
        .WillOnce(testing::SaveArgPointee<0>(&captured_action));

    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());

    EXPECT_EQ(ACTION_APPLIED, captured_action.status());
    EXPECT_EQ("guest",
              captured_action.collect_user_data_result().login_payload());
    EXPECT_FALSE(captured_action.collect_user_data_result().shown_to_user());
  }
}

TEST_F(CollectUserDataActionTest, SelectLoginFailsIfNoOptionAvailable) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data_proto->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("password_manager");

  ON_CALL(mock_website_login_manager_, GetLoginsForUrl(_, _))
      .WillByDefault(
          RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{}));

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              COLLECT_USER_DATA_ERROR))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SelectContactDetails) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* contact_details_proto =
      collect_user_data_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name(kMemoryLocation);
  contact_details_proto->set_request_payer_name(true);
  contact_details_proto->set_request_payer_email(true);
  contact_details_proto->set_request_payer_phone(true);

  autofill::AutofillProfile contact_profile;
  contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL,
                             u"Marion Mitchell Morrison");
  contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_FIRST, u"Marion");
  contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_MIDDLE,
                             u"Mitchell");
  contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_LAST, u"Morrison");
  contact_profile.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                             u"marion@me.xyz");
  contact_profile.SetRawInfo(autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
                             u"16505678910");

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_model_.SetSelectedAutofillProfile(
                kMemoryLocation,
                std::make_unique<autofill::AutofillProfile>(contact_profile),
                &user_data_);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(mock_personal_data_manager_, RecordUseOf(_)).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::collect_user_data_result,
                   AllOf(Property(&CollectUserDataResultProto::payer_email,
                                  "marion@me.xyz")))))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(user_data_.has_selected_address(kMemoryLocation), true);
  auto* profile = user_data_.selected_address(kMemoryLocation);
  EXPECT_EQ(profile->GetRawInfo(autofill::NAME_FULL),
            u"Marion Mitchell Morrison");
  EXPECT_EQ(profile->GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER),
            u"16505678910");
  EXPECT_EQ(profile->GetRawInfo(autofill::EMAIL_ADDRESS), u"marion@me.xyz");
}

TEST_F(CollectUserDataActionTest,
       ContactDetailsDescriptionFieldsEnumConversion) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* contact_details_proto =
      collect_user_data_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name(kMemoryLocation);
  contact_details_proto->set_request_payer_name(true);
  contact_details_proto->set_request_payer_email(true);
  contact_details_proto->set_request_payer_phone(true);
  contact_details_proto->add_summary_fields(ContactDetailsProto::EMAIL_ADDRESS);
  contact_details_proto->add_summary_fields(
      ContactDetailsProto::PHONE_HOME_WHOLE_NUMBER);
  contact_details_proto->set_max_number_summary_lines(2);
  contact_details_proto->add_full_fields(ContactDetailsProto::NAME_FULL);
  contact_details_proto->add_full_fields(ContactDetailsProto::EMAIL_ADDRESS);
  contact_details_proto->add_full_fields(
      ContactDetailsProto::PHONE_HOME_WHOLE_NUMBER);
  contact_details_proto->set_max_number_full_lines(3);

  EXPECT_CALL(mock_action_delegate_, CollectUserData(_))
      .WillOnce(Invoke([=](CollectUserDataOptions* collect_user_data_options) {
        EXPECT_EQ(collect_user_data_options->contact_summary_max_lines, 2);
        EXPECT_EQ(collect_user_data_options->contact_full_max_lines, 3);
        EXPECT_THAT(collect_user_data_options->contact_summary_fields,
                    ElementsAre(EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER));
        EXPECT_THAT(
            collect_user_data_options->contact_full_fields,
            ElementsAre(NAME_FULL, EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER));
      }));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest,
       ContactDetailsDescriptionDefaultsIfNotSpecified) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* contact_details_proto =
      collect_user_data_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name(kMemoryLocation);
  contact_details_proto->set_request_payer_name(true);
  contact_details_proto->set_request_payer_email(true);
  contact_details_proto->set_request_payer_phone(true);

  EXPECT_CALL(mock_action_delegate_, CollectUserData(_))
      .WillOnce(Invoke([=](CollectUserDataOptions* collect_user_data_options) {
        EXPECT_EQ(collect_user_data_options->contact_summary_max_lines, 1);
        EXPECT_EQ(collect_user_data_options->contact_full_max_lines, 2);
        EXPECT_THAT(collect_user_data_options->contact_summary_fields,
                    ElementsAre(EMAIL_ADDRESS, NAME_FULL));
        EXPECT_THAT(collect_user_data_options->contact_full_fields,
                    ElementsAre(NAME_FULL, EMAIL_ADDRESS));
      }));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SelectPaymentMethod) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  collect_user_data_proto->set_request_payment_method(true);
  collect_user_data_proto->set_billing_address_name("billing_address");

  autofill::AutofillProfile billing_profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&billing_profile, "Marion", "Mitchell",
                                 "Morrison", "marion@me.xyz", "Fox",
                                 "123 Zoo St.", "unit 5", "Hollywood", "CA",
                                 "91601", "US", "16505678910");

  autofill::CreditCard credit_card(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card, "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2050",
                                    billing_profile.guid());

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_model_.SetSelectedCreditCard(
                std::make_unique<autofill::CreditCard>(credit_card),
                &user_data_);
            user_model_.SetSelectedAutofillProfile(
                "billing_address",
                std::make_unique<autofill::AutofillProfile>(billing_profile),
                &user_data_);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(mock_personal_data_manager_, RecordUseOf(_)).Times(2);
  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           AllOf(Property(
                               &CollectUserDataResultProto::card_issuer_network,
                               "visa")))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  ExpectSelectedCardMatches(&credit_card);
}

TEST_F(CollectUserDataActionTest, SelectShippingAddress) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  collect_user_data_proto->set_shipping_address_name(kMemoryLocation);

  autofill::AutofillProfile shipping_address(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&shipping_address, "Marion", "Mitchell",
                                 "Morrison", "marion@me.xyz", "Fox",
                                 "123 Zoo St.", "unit 5", "Hollywood", "CA",
                                 "91601", "US", "16505678910");

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_model_.SetSelectedAutofillProfile(
                kMemoryLocation,
                std::make_unique<autofill::AutofillProfile>(shipping_address),
                &user_data_);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(mock_personal_data_manager_, RecordUseOf(_)).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_TRUE(user_data_.has_selected_address(kMemoryLocation));
  EXPECT_THAT(user_data_.selected_address(kMemoryLocation),
              Pointee(MatchesProfile(shipping_address)));
}

TEST_F(CollectUserDataActionTest, ContactDetailsCanHandleUtf8) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* contact_details_proto =
      collect_user_data_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name(kMemoryLocation);
  contact_details_proto->set_request_payer_name(true);
  contact_details_proto->set_request_payer_email(true);

  autofill::AutofillProfile contact_profile;
  contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL, u"艾丽森");
  contact_profile.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                             u"艾丽森@example.com");

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_model_.SetSelectedAutofillProfile(
                kMemoryLocation,
                std::make_unique<autofill::AutofillProfile>(contact_profile),
                &user_data_);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::payer_email,
                                    "艾丽森@example.com"))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(user_data_.has_selected_address(kMemoryLocation), true);
  auto* profile = user_data_.selected_address(kMemoryLocation);
  EXPECT_EQ(profile->GetRawInfo(autofill::NAME_FULL), u"艾丽森");
  EXPECT_EQ(profile->GetRawInfo(autofill::EMAIL_ADDRESS),
            u"艾丽森@example.com");
}

TEST_F(CollectUserDataActionTest, UserDataCompleteContact) {
  UserData user_data;
  CollectUserDataOptions options;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  options.contact_details_name = "profile";
  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  user_model_.SetSelectedAutofillProfile(
      "profile", std::make_unique<autofill::AutofillProfile>(profile),
      &user_data);

  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::EMAIL_ADDRESS));
  options.request_payer_email = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  profile.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                     u"joedoe@example.com");
  user_model_.SetSelectedAutofillProfile(
      "profile", std::make_unique<autofill::AutofillProfile>(profile),
      &user_data);
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FULL));
  options.request_payer_name = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL, u"Joe Doe");
  user_model_.SetSelectedAutofillProfile(
      "profile", std::make_unique<autofill::AutofillProfile>(profile),
      &user_data);
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  options.required_contact_data_pieces.push_back(MakeRequiredDataPiece(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER));
  options.request_payer_phone = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  profile.SetRawInfo(autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
                     u"+1 23 456 789 01");
  user_model_.SetSelectedAutofillProfile(
      "profile", std::make_unique<autofill::AutofillProfile>(profile),
      &user_data);
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));
}

TEST_F(CollectUserDataActionTest, UserDataCompletePhoneNumber) {
  UserData user_data;
  CollectUserDataOptions options;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  user_data.SetSelectedPhoneNumber(
      std::make_unique<autofill::AutofillProfile>(profile));

  options.required_phone_number_data_pieces.push_back(MakeRequiredDataPiece(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER));
  options.request_phone_number_separately = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  profile.SetRawInfo(autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
                     u"+1 23 456 789 01");
  user_data.SetSelectedPhoneNumber(
      std::make_unique<autofill::AutofillProfile>(profile));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));
}

TEST_F(CollectUserDataActionTest, UserDataCompletePayment) {
  UserData user_data;
  CollectUserDataOptions options;

  options.request_payment_method = true;
  options.billing_address_name = "billing_address";
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  // Valid credit card, but no billing address.
  autofill::CreditCard card(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&card, "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2050",
                                    /* billing_address_id = */ "");
  user_model_.SetSelectedCreditCard(
      std::make_unique<autofill::CreditCard>(card), &user_data);
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  // Incomplete billing address.
  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  user_model_.SetSelectedAutofillProfile(
      "billing_address", std::make_unique<autofill::AutofillProfile>(profile),
      &user_data);

  autofill::test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                                 "marion@me.xyz", "Fox", "123 Zoo St.",
                                 "unit 5", "Hollywood", "CA",
                                 /* zipcode = */ "", "US", "16505678910");

  user_model_.SetSelectedAutofillProfile(
      "billing_address", std::make_unique<autofill::AutofillProfile>(profile),
      &user_data);
  card.set_billing_address_id(
      user_data.selected_address("billing_address")->guid());
  user_model_.SetSelectedCreditCard(
      std::make_unique<autofill::CreditCard>(card), &user_data);
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  profile.SetRawInfo(autofill::ADDRESS_HOME_ZIP, u"91601");
  user_model_.SetSelectedAutofillProfile(
      "billing_address", std::make_unique<autofill::AutofillProfile>(profile),
      &user_data);
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  // Zip code is optional in Argentinian address.
  profile.SetRawInfo(autofill::ADDRESS_HOME_ZIP, u"");
  profile.SetRawInfo(autofill::ADDRESS_HOME_COUNTRY, u"AR");
  user_model_.SetSelectedAutofillProfile(
      "billing_address", std::make_unique<autofill::AutofillProfile>(profile),
      &user_data);
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  options.required_billing_address_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::ADDRESS_HOME_ZIP));
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  profile.SetRawInfo(autofill::ADDRESS_HOME_ZIP, u"B1675");
  user_model_.SetSelectedAutofillProfile(
      "billing_address", std::make_unique<autofill::AutofillProfile>(profile),
      &user_data);
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  // Expired credit card.
  card.SetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2019");
  user_model_.SetSelectedCreditCard(
      std::make_unique<autofill::CreditCard>(card), &user_data);
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));
}

TEST_F(CollectUserDataActionTest, UserDataCompleteTerms) {
  UserData user_data;
  CollectUserDataOptions options;

  options.accept_terms_and_conditions_text.assign("Accept T&C");
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  user_data.terms_and_conditions_ = REQUIRES_REVIEW;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  user_data.terms_and_conditions_ = ACCEPTED;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));
}

TEST_F(CollectUserDataActionTest, UserDataCompleteLogin) {
  UserData user_data;
  CollectUserDataOptions options;

  options.request_login_choice = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  user_model_.SetSelectedLoginChoice(std::make_unique<LoginChoice>(),
                                     &user_data);
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));
}

TEST_F(CollectUserDataActionTest, UserDataCompleteShippingAddress) {
  UserData user_data;
  CollectUserDataOptions options;
  options.request_shipping = true;
  options.shipping_address_name = "shipping_address";
  options.required_shipping_address_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::EMAIL_ADDRESS));
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  // Incomplete address.
  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  user_model_.SetSelectedAutofillProfile(
      "shipping_address", std::make_unique<autofill::AutofillProfile>(profile),
      &user_data);
  autofill::test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison", "",
                                 "Fox", "123 Zoo St.", "unit 5", "Hollywood",
                                 "CA",
                                 /* zipcode = */ "", "US", "16505678910");
  user_model_.SetSelectedAutofillProfile(
      "shipping_address", std::make_unique<autofill::AutofillProfile>(profile),
      &user_data);
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  // Complete for Assistant but not for AddressEditor.
  profile.SetRawInfo(autofill::EMAIL_ADDRESS, u"marion@me.xyz");
  user_model_.SetSelectedAutofillProfile(
      "shipping_address", std::make_unique<autofill::AutofillProfile>(profile),
      &user_data);
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  // Complete.
  profile.SetRawInfo(autofill::ADDRESS_HOME_ZIP, u"91601");
  user_model_.SetSelectedAutofillProfile(
      "shipping_address", std::make_unique<autofill::AutofillProfile>(profile),
      &user_data);
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));
}

TEST_F(CollectUserDataActionTest, UserDataCompleteChecksGenericUiCompleteness) {
  UserData user_data;
  CollectUserDataOptions options;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  options.additional_model_identifier_to_check = "generic_ui_valid";
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  ValueProto invalid;
  invalid.mutable_booleans()->add_values(false);
  user_model_.SetValue("generic_ui_valid", invalid);
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  ValueProto valid;
  valid.mutable_booleans()->add_values(true);
  user_model_.SetValue("generic_ui_valid", valid);
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));
}

TEST_F(CollectUserDataActionTest, StaticSectionValid) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  auto* static_section =
      collect_user_data_proto->add_additional_prepended_sections();
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  static_section->set_title("Static section");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  static_section->mutable_static_text_section()->set_text("Lorem ipsum.");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
    action.ProcessAction(callback_.Get());
  }

  static_section->mutable_static_text_section()->set_client_memory_key("key");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  static_section->mutable_static_text_section()->set_client_memory_key("key");
  user_model_.SetValue("key", SimpleValue(std::string("Hello World")));
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
    action.ProcessAction(callback_.Get());
  }

  static_section->mutable_static_text_section()->set_client_memory_key("key");
  user_data_.SetAdditionalValue("key", SimpleValue(std::string("Hello World")));
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
    action.ProcessAction(callback_.Get());
  }
}

TEST_F(CollectUserDataActionTest, TextInputSectionValid) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  auto* text_input_section =
      collect_user_data_proto->add_additional_prepended_sections();
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  text_input_section->set_title("Text input section");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  auto* input_field =
      text_input_section->mutable_text_input_section()->add_input_fields();
  input_field->set_value("12345");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  input_field->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  input_field->set_client_memory_key("code");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
    action.ProcessAction(callback_.Get());
  }

  // Duplicate input field fails due to duplicate memory key.
  *text_input_section->mutable_text_input_section()->add_input_fields() =
      *input_field;
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  text_input_section->mutable_text_input_section()
      ->mutable_input_fields(1)
      ->set_client_memory_key("something else");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
    action.ProcessAction(callback_.Get());
  }
}

TEST_F(CollectUserDataActionTest, PopupListSectionValid) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  auto* popup_list_section =
      collect_user_data_proto->add_additional_prepended_sections();
  popup_list_section->set_title("Popup list section");
  popup_list_section->mutable_popup_list_section()->set_selection_mandatory(
      false);
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }
  popup_list_section->mutable_popup_list_section()->add_item_names("item1");
  popup_list_section->mutable_popup_list_section()->add_item_names("item2");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
    action.ProcessAction(callback_.Get());
  }

  // Having multiple initial selections fails if multiselect is not allowed
  popup_list_section->mutable_popup_list_section()->add_initial_selection(0);
  popup_list_section->mutable_popup_list_section()->add_initial_selection(1);
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  popup_list_section->mutable_popup_list_section()->set_allow_multiselect(true);
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
    action.ProcessAction(callback_.Get());
  }

  // If an initial selection is out of bonds of the list of items there is an
  // error.
  popup_list_section->mutable_popup_list_section()->add_initial_selection(2);
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  popup_list_section->mutable_popup_list_section()->add_item_names("item3");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
    action.ProcessAction(callback_.Get());
  }
}

TEST_F(CollectUserDataActionTest, TextInputSectionWritesToClientMemory) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            ValueProto value;
            value.mutable_strings()->add_values("modified");
            user_data_.SetAdditionalValue("key2", value);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  auto* text_input_section =
      collect_user_data_proto->add_additional_prepended_sections();
  text_input_section->set_title("Text input section");

  auto* input_field_1 =
      text_input_section->mutable_text_input_section()->add_input_fields();
  input_field_1->set_value("initial");
  input_field_1->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  input_field_1->set_client_memory_key("key1");

  auto* input_field_2 =
      text_input_section->mutable_text_input_section()->add_input_fields();
  input_field_2->set_value("initial");
  input_field_2->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  input_field_2->set_client_memory_key("key2");

  auto* input_field_3 =
      text_input_section->mutable_text_input_section()->add_input_fields();
  input_field_3->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  input_field_3->set_client_memory_key("key3");

  EXPECT_FALSE(user_data_.HasAdditionalValue("key1"));
  EXPECT_FALSE(user_data_.HasAdditionalValue("key2"));
  EXPECT_FALSE(user_data_.HasAdditionalValue("key3"));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(&CollectUserDataResultProto::set_text_input_memory_keys,
                       UnorderedElementsAre("key1", "key2", "key3")))))));
  action.ProcessAction(callback_.Get());

  ValueProto value1;
  value1.mutable_strings()->add_values("initial");
  ValueProto value2;
  value2.mutable_strings()->add_values("modified");
  ValueProto value3;
  value3.mutable_strings()->add_values("");
  EXPECT_EQ(*user_data_.GetAdditionalValue("key1"), value1);
  EXPECT_EQ(*user_data_.GetAdditionalValue("key2"), value2);
  EXPECT_EQ(*user_data_.GetAdditionalValue("key3"), value3);
}

TEST_F(CollectUserDataActionTest, AllowedBasicCardNetworks) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  collect_user_data_proto->set_billing_address_name("billing_address");

  std::string kSupportedBasicCardNetworks[] = {"amex", "diners",   "discover",
                                               "elo",  "jcb",      "mastercard",
                                               "mir",  "unionpay", "visa"};

  for (const auto& network : kSupportedBasicCardNetworks) {
    *collect_user_data_proto->add_supported_basic_card_networks() = network;
  }

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
            autofill::test::SetProfileInfo(&profile, "Marion", "Mitchell",
                                           "Morrison", "marion@me.xyz", "Fox",
                                           "123 Zoo St.", "unit 5", "Hollywood",
                                           "CA", "96043", "US", "16505678910");
            user_model_.SetSelectedAutofillProfile(
                "billing_address",
                std::make_unique<autofill::AutofillProfile>(profile),
                &user_data_);
            autofill::CreditCard card(base::GenerateGUID(), kFakeUrl);
            autofill::test::SetCreditCardInfo(
                &card, "Marion Mitchell", "4111 1111 1111 1111", "01", "2050",
                user_data_.selected_address("billing_address")->guid());
            user_model_.SetSelectedCreditCard(
                std::make_unique<autofill::CreditCard>(card), &user_data_);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, InvalidBasicCardNetworks) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);

  *collect_user_data_proto->add_supported_basic_card_networks() = "visa";
  *collect_user_data_proto->add_supported_basic_card_networks() =
      "unknown_network";

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, OverwriteExistingUserData) {
  // Set previous user data state.
  user_data_.terms_and_conditions_ = ACCEPTED;
  ValueProto value1;
  value1.mutable_strings()->add_values("val1");
  ValueProto value2;
  value2.mutable_strings()->add_values("val2");
  ValueProto value3;
  value3.mutable_strings()->add_values("val3");
  user_data_.SetAdditionalValue("key1", value1);
  user_data_.SetAdditionalValue("key2", value2);
  user_data_.SetAdditionalValue("key3", value3);

  // Set options.
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* prepended_section =
      collect_user_data_proto->add_additional_prepended_sections();
  prepended_section->set_title("Text input section");

  auto* input_field_1 =
      prepended_section->mutable_text_input_section()->add_input_fields();
  input_field_1->set_value("initial");
  input_field_1->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  input_field_1->set_client_memory_key("key1");

  auto* appended_section =
      collect_user_data_proto->add_additional_appended_sections();
  appended_section->set_title("Text input section 2");
  auto* input_field_2 =
      appended_section->mutable_text_input_section()->add_input_fields();
  input_field_2->set_value("initial");
  input_field_2->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  input_field_2->set_client_memory_key("key2");

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([](CollectUserDataOptions* collect_user_data_options) {
            // do not call confirm_callback since we are only looking to test
            // OnShowToUser.
            // Calling confirm_callback then calls OnGetUserData which changes
            // the user_data_.
          }));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(user_data_.terms_and_conditions_, NOT_SELECTED);
  EXPECT_EQ(user_data_.GetAdditionalValue("key1")->strings().values(0),
            "initial");
  EXPECT_EQ(user_data_.GetAdditionalValue("key2")->strings().values(0),
            "initial");
  EXPECT_EQ(user_data_.GetAdditionalValue("key3")->strings().values(0), "val3");
}

TEST_F(CollectUserDataActionTest, AttachesProfilesToContactsAndAddresses) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(&profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Main St. 18", "",
                                 "abc", "New York", "NY", "10001", "US", "");
  autofill::AutofillProfile incomplete;
  autofill::test::SetProfileInfo(&incomplete, "Berta", "West", "", "", "", "",
                                 "", "", "", "", "", "", "");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(Return(
          std::vector<autofill::AutofillProfile*>({&profile, &incomplete})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_model_.SetSelectedAutofillProfile(
                "contact", std::make_unique<autofill::AutofillProfile>(profile),
                &user_data_);
            user_model_.SetSelectedAutofillProfile(
                "shipping-address",
                std::make_unique<autofill::AutofillProfile>(profile),
                &user_data_);

            EXPECT_THAT(
                user_data_.available_contacts_,
                UnorderedElementsAre(Pointee(MatchesContact(profile)),
                                     Pointee(MatchesContact(incomplete))));
            EXPECT_THAT(
                user_data_.available_addresses_,
                UnorderedElementsAre(Pointee(MatchesAddress(profile)),
                                     Pointee(MatchesAddress(incomplete))));

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  auto* contact_details = user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_contact_details_name("contact");
  user_data->set_shipping_address_name("shipping-address");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, InitialSelectsProfileAndShippingAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(&profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Main St. 18", "",
                                 "abc", "New York", "NY", "10001", "us", "");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(
          Return(std::vector<autofill::AutofillProfile*>({&profile})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            ExpectSelectedProfileMatches("profile", &profile);
            ExpectSelectedProfileMatches("shipping-address", &profile);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->set_shipping_address_name("shipping-address");
  auto* contact_details = collect_user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_contact_details_name("profile");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ShippingAddressSectionCustomTitle) {
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            // do not call confirm_callback since we are only looking to test
            // |collect_user_data_options|.
            EXPECT_EQ(collect_user_data_options->shipping_address_section_title,
                      "custom title");
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_shipping_address_section_title("custom title");

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ShippingAddressSectionDefaultTitle) {
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            // do not call confirm_callback since we are only looking to test
            // |collect_user_data_options|.
            EXPECT_EQ(
                collect_user_data_options->shipping_address_section_title,
                l10n_util::GetStringUTF8(IDS_PAYMENTS_SHIPPING_ADDRESS_LABEL));
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_shipping_address_section_title("");

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ContactDetailsSectionCustomTitle) {
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            // do not call confirm_callback since we are only looking to test
            // |collect_user_data_options|.
            EXPECT_EQ(collect_user_data_options->contact_details_section_title,
                      "custom title");
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->mutable_contact_details()
      ->set_contact_details_section_title("custom title");

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ContactDetailsSectionDefaultTitle) {
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            // do not call confirm_callback since we are only looking to test
            // |collect_user_data_options|.
            EXPECT_EQ(
                collect_user_data_options->shipping_address_section_title,
                l10n_util::GetStringUTF8(IDS_PAYMENTS_CONTACT_DETAILS_LABEL));
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->mutable_contact_details()
      ->set_contact_details_section_title("");

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, InitialSelectsProfileFromDefaultEmail) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile_a;
  autofill::test::SetProfileInfo(&profile_a, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  autofill::AutofillProfile profile_b;
  autofill::test::SetProfileInfo(&profile_b, "Berta", "", "West",
                                 "berta.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(Return(
          std::vector<autofill::AutofillProfile*>({&profile_a, &profile_b})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            ExpectSelectedProfileMatches("profile", &profile_b);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  auto* contact_details = user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_request_payer_email(true);
  contact_details->set_contact_details_name("profile");

  ON_CALL(mock_action_delegate_, GetEmailAddressForAccessTokenAccount())
      .WillByDefault(Return("berta.west@gmail.com"));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, KeepsSelectedProfileAndShippingAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(&profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Main St. 18", "",
                                 "abc", "New York", "NY", "10001", "us", "");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(
          Return(std::vector<autofill::AutofillProfile*>({&profile})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            ExpectSelectedProfileMatches("profile", &profile);
            ExpectSelectedProfileMatches("shipping_address", &profile);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->set_shipping_address_name("shipping_address");
  auto* contact_details = collect_user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_contact_details_name("profile");

  // Set previous user data.
  user_model_.SetSelectedAutofillProfile(
      "profile", std::make_unique<autofill::AutofillProfile>(profile),
      &user_data_);
  user_model_.SetSelectedAutofillProfile(
      "shipping_address", std::make_unique<autofill::AutofillProfile>(profile),
      &user_data_);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ResetsContactAndShippingIfNoLongerInList) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(&profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(
          Return(std::vector<autofill::AutofillProfile*>({&profile})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            // Default selected to newly sent profile.
            ExpectSelectedProfileMatches("profile", &profile);
            ExpectSelectedProfileMatches("shipping_address", &profile);

            // Do not call the callback. We're only interested in the state.
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->set_shipping_address_name("shipping_address");
  auto* contact_details = collect_user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_contact_details_name("profile");

  // Set previous user data.
  autofill::AutofillProfile selected_profile;
  autofill::test::SetProfileInfo(&selected_profile, "Berta", "", "West",
                                 "berta.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  user_model_.SetSelectedAutofillProfile(
      "profile", std::make_unique<autofill::AutofillProfile>(selected_profile),
      &user_data_);
  user_model_.SetSelectedAutofillProfile(
      "shipping_address",
      std::make_unique<autofill::AutofillProfile>(selected_profile),
      &user_data_);

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ResetsMatchingButDifferentContact) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(&profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(
          Return(std::vector<autofill::AutofillProfile*>({&profile})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            // Default selected to newly sent profile.
            ExpectSelectedProfileMatches("profile", &profile);

            // Do not call the callback. We're only interested in the state.
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  auto* contact_details = collect_user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_contact_details_name("profile");

  // The selected profile is identical with the current one, but it has a
  // different GUID.
  autofill::AutofillProfile selected_profile;
  autofill::test::SetProfileInfo(&selected_profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  user_model_.SetSelectedAutofillProfile(
      "profile", std::make_unique<autofill::AutofillProfile>(selected_profile),
      &user_data_);

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, AttachesCreditCardsWithAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::AutofillProfile billing_address;
  autofill::test::SetProfileInfo(&billing_address, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK", "+44");

  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("GUID"))
      .WillByDefault(Return(&billing_address));

  autofill::CreditCard card_with_address;
  autofill::test::SetCreditCardInfo(&card_with_address, "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "GUID");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&card_with_address})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_THAT(user_data_.available_payment_instruments_, SizeIs(1));
            EXPECT_THAT(user_data_.available_payment_instruments_[0]->card,
                        Pointee(MatchesCard(card_with_address)));
            EXPECT_THAT(
                user_data_.available_payment_instruments_[0]->billing_address,
                Pointee(MatchesProfile(billing_address)));

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  user_data->set_request_payment_method(true);
  user_data->set_billing_address_name("BILLING");
  user_data->add_supported_basic_card_networks("visa");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, AttachesCreditCardsWithoutAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::CreditCard card_without_address;
  autofill::test::SetCreditCardInfo(&card_without_address, "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&card_without_address})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_THAT(user_data_.available_payment_instruments_, SizeIs(1));
            EXPECT_THAT(user_data_.available_payment_instruments_[0]->card,
                        Pointee(MatchesCard(card_without_address)));
            EXPECT_EQ(user_data_.available_payment_instruments_[0]
                          ->billing_address.get(),
                      nullptr);
            // Do not run callback, the data is not complete.
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_payment_method(true);
  user_data->set_billing_address_name("BILLING");
  user_data->set_request_terms_and_conditions(false);
  user_data->add_supported_basic_card_networks("visa");

  EXPECT_CALL(mock_personal_data_manager_, GetProfileByGUID(_)).Times(0);
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, AttachesCreditCardsForEmptyNetworksList) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::CreditCard card;
  autofill::test::SetCreditCardInfo(&card, "Adam West", "4111111111111111", "1",
                                    "2050",
                                    /* billing_address_id= */ "");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(Return(std::vector<autofill::CreditCard*>({&card})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_THAT(user_data_.available_payment_instruments_, SizeIs(1));
            EXPECT_THAT(user_data_.available_payment_instruments_[0]->card,
                        Pointee(MatchesCard(card)));
            // Don't run callback, the data is not complete.
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  user_data->set_request_payment_method(true);
  user_data->set_billing_address_name("BILLING");

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, InitialSelectsCardAndAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::AutofillProfile billing_address;
  autofill::test::SetProfileInfo(&billing_address, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK", "+44");

  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("GUID"))
      .WillByDefault(Return(&billing_address));

  autofill::CreditCard card_with_address;
  autofill::test::SetCreditCardInfo(&card_with_address, "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "GUID");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&card_with_address})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            ExpectSelectedCardMatches(&card_with_address);
            ExpectSelectedProfileMatches("billing_address", &billing_address);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->add_supported_basic_card_networks("visa");
  collect_user_data->set_request_payment_method(true);
  collect_user_data->set_billing_address_name("billing_address");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, KeepsSelectedCardAndAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::AutofillProfile billing_address;
  autofill::test::SetProfileInfo(&billing_address, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK", "+44");

  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("GUID"))
      .WillByDefault(Return(&billing_address));

  autofill::CreditCard card_with_address;
  autofill::test::SetCreditCardInfo(&card_with_address, "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "GUID");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&card_with_address})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            ExpectSelectedCardMatches(&card_with_address);
            ExpectSelectedProfileMatches("billing_address", &billing_address);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->add_supported_basic_card_networks("visa");
  collect_user_data->set_billing_address_name("billing_address");

  // Set previous user data.
  user_model_.SetSelectedCreditCard(
      std::make_unique<autofill::CreditCard>(card_with_address), &user_data_);

  user_model_.SetSelectedAutofillProfile(
      "billing_address",
      std::make_unique<autofill::AutofillProfile>(billing_address),
      &user_data_);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ResetsCardAndAddressIfNoLongerInList) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  // Do not return any cards, one will be default selected.
  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(Return(std::vector<autofill::CreditCard*>({})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            ExpectSelectedCardMatches(nullptr);
            ExpectSelectedProfileMatches("BILLING", nullptr);

            // Do not call the callback. We're only interested in the state.
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->set_request_payment_method(true);
  collect_user_data->set_billing_address_name("BILLING");
  collect_user_data->add_supported_basic_card_networks("visa");

  // Set previous user data.
  autofill::CreditCard selected_card;
  autofill::test::SetCreditCardInfo(&selected_card, "Berta West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  autofill::AutofillProfile selected_address;
  autofill::test::SetProfileInfo(
      &selected_address, "Berta", "", "West", "berta.west@gmail.com", "",
      "Baker Street 221b", "", "London", "", "WC2N 5DU", "UK", "+44");

  user_model_.SetSelectedCreditCard(
      std::make_unique<autofill::CreditCard>(selected_card), &user_data_);
  user_model_.SetSelectedAutofillProfile(
      "billing_address",
      std::make_unique<autofill::AutofillProfile>(selected_address),
      &user_data_);

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, GenericUiModelWritesToProtoResult) {
  ModelProto::ModelValue value_1;
  value_1.set_identifier("value_1_key");
  value_1.mutable_value()->mutable_strings()->add_values(
      "value_1_initial_value");
  ModelProto::ModelValue value_1_modified;
  value_1_modified.set_identifier("value_1_key");
  ModelProto::ModelValue value_2;
  value_2.set_identifier("value_2_key");
  value_2.mutable_value()->mutable_strings()->add_values(
      "value_2_initial_value");
  ModelProto::ModelValue value_2_modified;
  value_2_modified.set_identifier("value_2_key");
  value_2_modified.mutable_value()->mutable_strings()->add_values(
      "value_2_modified");
  ModelProto::ModelValue value_3;
  value_3.set_identifier("value_3_key");
  value_3.mutable_value()->mutable_strings()->add_values(
      "value_3_initial_value");
  ModelProto::ModelValue value_3_modified;
  value_3_modified.set_identifier("value_3_key");
  value_3_modified.mutable_value()->mutable_strings()->add_values(
      "value_3_modified");
  ModelProto::ModelValue value_4;
  value_4.set_identifier("value_4_key");
  value_4.mutable_value()->mutable_strings()->add_values(
      "value_4_initial_value");

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_model_.SetValue("value_1_key", value_1_modified.value());
            user_model_.SetValue("value_2_key", value_2_modified.value());
            user_model_.SetValue("value_3_key", value_3_modified.value());
            // Leave value_4 at initial value.

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  auto* proto_model_prepended =
      collect_user_data->mutable_generic_user_interface_prepended()
          ->mutable_model();
  *proto_model_prepended->add_values() = value_1;
  *proto_model_prepended->add_values() = value_2;
  auto* proto_model_appended =
      collect_user_data->mutable_generic_user_interface_appended()
          ->mutable_model();
  *proto_model_appended->add_values() = value_3;
  *proto_model_appended->add_values() = value_4;

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::collect_user_data_result,
                   Property(&CollectUserDataResultProto::model,
                            Property(&ModelProto::values,
                                     UnorderedElementsAre(
                                         value_1_modified, value_2_modified,
                                         value_3_modified, value_4))))))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ClearUserDataIfRequested) {
  base::Time current = base::Time::Now();

  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::AutofillProfile address_new;
  autofill::test::SetProfileInfo(&address_new, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK", "+44");
  address_new.set_use_date(current);

  autofill::AutofillProfile address_old;
  autofill::test::SetProfileInfo(
      &address_old, "Berta", "", "West", "berta.west@gmail.com", "",
      "Baker Street 221b", "", "London", "", "WC2N 5DU", "UK", "+44");
  address_old.set_use_date(current - base::Days(2));

  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("card_new"))
      .WillByDefault(Return(&address_new));
  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("card_old"))
      .WillByDefault(Return(&address_old));

  autofill::CreditCard card_new;
  autofill::test::SetCreditCardInfo(&card_new, "Adam West", "4111111111111111",
                                    "1", "2050",
                                    /* billing_address_id= */ "card_new");
  card_new.set_use_date(current);

  autofill::CreditCard card_old;
  autofill::test::SetCreditCardInfo(&card_old, "Berta West", "4111111111111111",
                                    "1", "2050",
                                    /* billing_address_id= */ "card_old");
  card_old.set_use_date(current - base::Days(2));

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&card_new, &card_old})));

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(Return(std::vector<autofill::AutofillProfile*>(
          {&address_new, &address_old})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            ExpectSelectedCardMatches(&card_new);
            ExpectSelectedProfileMatches("billing", &address_new);
            ExpectSelectedProfileMatches("contact", &address_new);
            ExpectSelectedProfileMatches("shipping", &address_new);
            EXPECT_EQ(user_data_.selected_login_, absl::nullopt);

            // Do not call the callback. We're only interested in the state.
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_payment_method(true);
  collect_user_data->set_billing_address_name("billing");
  collect_user_data->set_shipping_address_name("shipping");
  collect_user_data->mutable_contact_details()->set_request_payer_name(true);
  collect_user_data->mutable_contact_details()->set_contact_details_name(
      "contact");
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->add_supported_basic_card_networks("visa");
  collect_user_data->set_clear_previous_credit_card_selection(true);
  collect_user_data->set_clear_previous_login_selection(true);
  collect_user_data->add_clear_previous_profile_selection("billing");
  collect_user_data->add_clear_previous_profile_selection("contact");
  collect_user_data->add_clear_previous_profile_selection("shipping");

  // Set previous user data to the second card/profile. If clear works
  // correctly, the action should default to the first card/profile.
  user_model_.SetSelectedCreditCard(
      std::make_unique<autofill::CreditCard>(card_old), &user_data_);
  user_model_.SetSelectedAutofillProfile(
      "billing", std::make_unique<autofill::AutofillProfile>(address_old),
      &user_data_);
  user_model_.SetSelectedAutofillProfile(
      "contact", std::make_unique<autofill::AutofillProfile>(address_old),
      &user_data_);
  user_model_.SetSelectedAutofillProfile(
      "shipping", std::make_unique<autofill::AutofillProfile>(address_old),
      &user_data_);
  user_data_.selected_login_ =
      WebsiteLoginManager::Login(GURL("http://www.example.com"), "username");

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, LinkClickWritesPartialUserData) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(true);
  auto* contact_details_proto =
      collect_user_data_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name(kMemoryLocation);
  contact_details_proto->set_request_payer_name(true);
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            ValueProto value;
            value.mutable_strings()->add_values("modified");
            user_data_.SetAdditionalValue("key1", value);
            std::move(collect_user_data_options->terms_link_callback)
                .Run(1, &user_data_, &user_model_);
          }));

  auto* text_input_section =
      collect_user_data_proto->add_additional_prepended_sections();
  text_input_section->set_title("Text input section");

  auto* input_field_1 =
      text_input_section->mutable_text_input_section()->add_input_fields();
  input_field_1->set_value("initial");
  input_field_1->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  input_field_1->set_client_memory_key("key1");

  CollectUserDataAction action(&mock_action_delegate_, action_proto);

  EXPECT_CALL(mock_personal_data_manager_, RecordUseOf(_)).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(&CollectUserDataResultProto::set_text_input_memory_keys,
                       UnorderedElementsAre("key1")))))));
  action.ProcessAction(callback_.Get());
  EXPECT_THAT(
      GetUkmCollectUserDataResult(ukm_recorder_),
      ElementsAreArray({ToHumanReadableEntry(
          source_id_, kResult,
          static_cast<int64_t>(Metrics::CollectUserDataResult::
                                   TERMS_AND_CONDITIONS_LINK_CLICKED))}));
}

TEST_F(CollectUserDataActionTest, ConfirmButtonChip) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* confirm_chip = collect_user_data_proto->mutable_confirm_chip();
  confirm_chip->set_text("Custom text");
  confirm_chip->set_icon(ICON_CLEAR);
  confirm_chip->set_sticky(true);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_EQ(collect_user_data_options->confirm_action.chip().text(),
                      "Custom text");
            EXPECT_EQ(collect_user_data_options->confirm_action.chip().icon(),
                      ICON_CLEAR);
            EXPECT_EQ(collect_user_data_options->confirm_action.chip().sticky(),
                      true);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ConfirmButtonFallbackText) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_EQ(collect_user_data_options->confirm_action.chip().text(),
                      l10n_util::GetStringUTF8(
                          IDS_AUTOFILL_ASSISTANT_PAYMENT_INFO_CONFIRM));
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, FailsForWebLayerRunsWithoutBackendData) {
  ON_CALL(mock_action_delegate_, MustUseBackendData)
      .WillByDefault(Return(true));
  EXPECT_CALL(mock_action_delegate_, CollectUserData).Times(0);
  EXPECT_CALL(mock_action_delegate_, RequestUserData).Times(0);

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->set_request_payment_method(true);
  collect_user_data->set_billing_address_name("billing");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, FailsForDataOriginNoticeWithoutBackendData) {
  EXPECT_CALL(mock_action_delegate_, CollectUserData).Times(0);
  EXPECT_CALL(mock_action_delegate_, RequestUserData).Times(0);

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->set_request_payment_method(true);
  collect_user_data->set_billing_address_name("billing");
  collect_user_data->mutable_data_origin_notice()->set_link_text("Link");
  collect_user_data->mutable_data_origin_notice()->set_dialog_title("Title");
  collect_user_data->mutable_data_origin_notice()->set_dialog_text("Text");
  collect_user_data->mutable_data_origin_notice()->set_dialog_button_text(
      "Button");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SucceedsWithDataOriginNoticeAndBackendData) {
  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .WillOnce(RunOnceCallback<1>(true, GetUserDataResponseProto()));
  ON_CALL(mock_action_delegate_, CollectUserData)
      .WillByDefault([&](CollectUserDataOptions* collect_user_data_options) {
        EXPECT_TRUE(collect_user_data_options->data_origin_notice);
        // Do not finish the action.
      });

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->set_request_payment_method(true);
  collect_user_data->set_billing_address_name("billing");
  collect_user_data->mutable_data_origin_notice()->set_link_text("Link");
  collect_user_data->mutable_data_origin_notice()->set_dialog_title("Title");
  collect_user_data->mutable_data_origin_notice()->set_dialog_text("Text");
  collect_user_data->mutable_data_origin_notice()->set_dialog_button_text(
      "Button");
  collect_user_data->mutable_data_source();

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ContactDataFromProto) {
  ON_CALL(mock_action_delegate_, GetPersonalDataManager)
      .WillByDefault(Return(nullptr));
  ON_CALL(mock_action_delegate_, MustUseBackendData)
      .WillByDefault(Return(true));
  ON_CALL(mock_action_delegate_, CollectUserData)
      .WillByDefault([&](CollectUserDataOptions* collect_user_data_options) {
        EXPECT_FALSE(collect_user_data_options->should_store_data_changes);
        ASSERT_EQ(user_data_.available_contacts_.size(), 1u);
        EXPECT_THAT(user_data_.available_contacts_[0]->profile->guid(),
                    Not(IsEmpty()));
        EXPECT_FALSE(user_data_.available_contacts_[0]->can_edit);
        auto mappings = field_formatter::CreateAutofillMappings(
            *user_data_.available_contacts_[0]->profile, "en-US");
        EXPECT_THAT(mappings,
                    IsSupersetOf(
                        {Pair(field_formatter::Key(3), "John"),
                         Pair(field_formatter::Key(5), "Doe"),
                         Pair(field_formatter::Key(7), "John Doe"),
                         Pair(field_formatter::Key(9), "johndoe@google.com")}));

        std::move(collect_user_data_options->confirm_callback)
            .Run(&user_data_, &user_model_);
      });

  GetUserDataResponseProto user_data_response;
  user_data_response.set_locale("en-US");
  auto* profile = user_data_response.add_available_contacts();
  (*profile->mutable_values())[7] = MakeAutofillEntry("John Doe");
  (*profile->mutable_values())[9] = MakeAutofillEntry("johndoe@google.com");
  auto* incomplete = user_data_response.add_available_contacts();
  (*incomplete->mutable_values())[7] = MakeAutofillEntry("Jane Doe");
  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .WillOnce(RunOnceCallback<1>(true, user_data_response));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->mutable_contact_details()->set_request_payer_name(true);
  collect_user_data->mutable_contact_details()->set_request_payer_email(true);
  *collect_user_data->mutable_contact_details()->add_required_data_piece() =
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FULL);
  *collect_user_data->mutable_contact_details()->add_required_data_piece() =
      MakeRequiredDataPiece(autofill::ServerFieldType::EMAIL_ADDRESS);
  collect_user_data->mutable_contact_details()->set_contact_details_name(
      kMemoryLocation);
  collect_user_data->mutable_data_source();

  EXPECT_CALL(mock_personal_data_manager_, RecordUseOf).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, PhoneNumberFromProto) {
  ON_CALL(mock_action_delegate_, GetPersonalDataManager)
      .WillByDefault(Return(nullptr));
  ON_CALL(mock_action_delegate_, MustUseBackendData)
      .WillByDefault(Return(true));
  ON_CALL(mock_action_delegate_, CollectUserData)
      .WillByDefault([&](CollectUserDataOptions* collect_user_data_options) {
        EXPECT_FALSE(collect_user_data_options->should_store_data_changes);
        ASSERT_EQ(user_data_.available_contacts_.size(), 1u);
        EXPECT_THAT(user_data_.available_contacts_[0]->profile->guid(),
                    Not(IsEmpty()));
        EXPECT_FALSE(user_data_.available_contacts_[0]->can_edit);
        auto contact_mappings = field_formatter::CreateAutofillMappings(
            *user_data_.available_contacts_[0]->profile, "en-US");
        // Initially the contact contains the backend data.
        EXPECT_THAT(
            contact_mappings,
            IsSupersetOf({Pair(field_formatter::Key(3), "John"),
                          Pair(field_formatter::Key(5), "Doe"),
                          Pair(field_formatter::Key(7), "John Doe"),
                          Pair(field_formatter::Key(10), "1234567890"),
                          Pair(field_formatter::Key(12), "1"),
                          Pair(field_formatter::Key(14), "+11234567890")}));

        ASSERT_EQ(user_data_.available_phone_numbers_.size(), 1u);
        EXPECT_THAT(user_data_.available_phone_numbers_[0]->profile->guid(),
                    Not(IsEmpty()));
        auto phone_number_mappings = field_formatter::CreateAutofillMappings(
            *user_data_.available_phone_numbers_[0]->profile, "en-US");
        EXPECT_THAT(
            phone_number_mappings,
            IsSupersetOf({Pair(field_formatter::Key(10), "1876543210"),
                          Pair(field_formatter::Key(12), "1"),
                          Pair(field_formatter::Key(14), "+11876543210")}));

        std::move(collect_user_data_options->confirm_callback)
            .Run(&user_data_, &user_model_);
      });

  GetUserDataResponseProto user_data_response;
  user_data_response.set_locale("en-US");
  auto* profile = user_data_response.add_available_contacts();
  (*profile->mutable_values())[7] = MakeAutofillEntry("John Doe");
  (*profile->mutable_values())[14] = MakeAutofillEntry("+1 123-456-7890");
  *user_data_response.add_available_phone_numbers()->mutable_value() =
      MakeAutofillEntry("+1 187-654-3210");
  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .WillOnce(RunOnceCallback<1>(true, user_data_response));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->mutable_contact_details()->set_request_payer_name(true);
  *collect_user_data->mutable_contact_details()->add_required_data_piece() =
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FULL);
  collect_user_data->mutable_contact_details()->set_contact_details_name(
      kMemoryLocation);
  collect_user_data->mutable_contact_details()
      ->set_separate_phone_number_section(true);
  *collect_user_data->mutable_contact_details()
       ->add_phone_number_required_data_piece() =
      MakeRequiredDataPiece(autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER);
  collect_user_data->mutable_contact_details()->set_phone_number_section_title(
      "Phone number");
  collect_user_data->mutable_data_source();

  EXPECT_CALL(mock_personal_data_manager_, RecordUseOf).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
  // The selected phone number is merged into the selected contact.
  auto mappings = field_formatter::CreateAutofillMappings(
      *user_data_.selected_address(kMemoryLocation), "en-US");
  EXPECT_THAT(mappings,
              IsSupersetOf({Pair(field_formatter::Key(3), "John"),
                            Pair(field_formatter::Key(5), "Doe"),
                            Pair(field_formatter::Key(7), "John Doe"),
                            Pair(field_formatter::Key(10), "1876543210"),
                            Pair(field_formatter::Key(12), "1"),
                            Pair(field_formatter::Key(14), "+11876543210")}));
  EXPECT_EQ(user_data_.available_contacts_.size(), 1u);
}

TEST_F(CollectUserDataActionTest, PaymentDataFromProto) {
  autofill::CountryNames::SetLocaleString("en-US");
  ON_CALL(mock_action_delegate_, GetPersonalDataManager)
      .WillByDefault(Return(nullptr));
  ON_CALL(mock_action_delegate_, MustUseBackendData)
      .WillByDefault(Return(true));
  ON_CALL(mock_action_delegate_, CollectUserData)
      .WillByDefault([&](CollectUserDataOptions* collect_user_data_options) {
        EXPECT_FALSE(collect_user_data_options->should_store_data_changes);
        EXPECT_THAT(user_data_.available_payment_instruments_[0]->card->guid(),
                    Not(IsEmpty()));
        EXPECT_THAT(user_data_.available_payment_instruments_[0]
                        ->billing_address->guid(),
                    Not(IsEmpty()));
        auto card_mappings = field_formatter::CreateAutofillMappings(
            *user_data_.available_payment_instruments_[0]->card, "en-US");
        EXPECT_THAT(card_mappings,
                    IsSupersetOf({Pair(field_formatter::Key(51), "John Doe"),
                                  Pair(field_formatter::Key(53), "08"),
                                  Pair(field_formatter::Key(-7), "8"),
                                  Pair(field_formatter::Key(55), "2050"),
                                  Pair(field_formatter::Key(54), "50"),
                                  Pair(field_formatter::Key(56), "08/50"),
                                  Pair(field_formatter::Key(57), "08/2050"),
                                  Pair(field_formatter::Key(58), "Visa"),
                                  Pair(field_formatter::Key(-2), "visa"),
                                  Pair(field_formatter::Key(-5), "Visa"),
                                  Pair(field_formatter::Key(-4), "1111")}));
        EXPECT_EQ(
            user_data_.available_payment_instruments_[0]->card->instrument_id(),
            123456);
        // Used for card summary in UI.
        EXPECT_EQ(user_data_.available_payment_instruments_[0]
                      ->card->NetworkForDisplay(),
                  u"Visa");
        EXPECT_EQ(user_data_.available_payment_instruments_[0]
                      ->card->LastFourDigits(),
                  u"1111");

        auto address_mappings = field_formatter::CreateAutofillMappings(
            *user_data_.available_payment_instruments_[0]->billing_address,
            "en-US");
        EXPECT_THAT(address_mappings,
                    IsSupersetOf({Pair(field_formatter::Key(3), "John"),
                                  Pair(field_formatter::Key(5), "Doe"),
                                  Pair(field_formatter::Key(7), "John Doe"),
                                  Pair(field_formatter::Key(30),
                                       "Brandschenkestrasse 110"),
                                  Pair(field_formatter::Key(35), "8002"),
                                  Pair(field_formatter::Key(33), "Zurich"),
                                  Pair(field_formatter::Key(36), "Switzerland"),
                                  Pair(field_formatter::Key(-8), "CH")}));

        std::move(collect_user_data_options->confirm_callback)
            .Run(&user_data_, &user_model_);
      });

  GetUserDataResponseProto user_data_response;
  user_data_response.set_locale("en-US");
  auto* payment_instrument =
      user_data_response.add_available_payment_instruments();
  AddCompleteCardEntriesToMap("John Doe",
                              payment_instrument->mutable_card_values());
  payment_instrument->set_instrument_id(123456);
  payment_instrument->set_network("visaCC");
  payment_instrument->set_last_four_digits("1111");
  AddCompleteAddressEntriesToMap("John Doe",
                                 payment_instrument->mutable_address_values());
  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .WillOnce(RunOnceCallback<1>(true, user_data_response));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->set_request_payment_method(true);
  collect_user_data->set_billing_address_name("billing");
  collect_user_data->mutable_data_source();

  EXPECT_CALL(mock_personal_data_manager_, RecordUseOf).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ShippingDataFromProto) {
  autofill::CountryNames::SetLocaleString("en-US");
  ON_CALL(mock_action_delegate_, GetPersonalDataManager)
      .WillByDefault(Return(nullptr));
  ON_CALL(mock_action_delegate_, MustUseBackendData)
      .WillByDefault(Return(true));
  ON_CALL(mock_action_delegate_, CollectUserData)
      .WillByDefault([&](CollectUserDataOptions* collect_user_data_options) {
        EXPECT_FALSE(collect_user_data_options->should_store_data_changes);
        EXPECT_THAT(user_data_.available_addresses_[0]->profile->guid(),
                    Not(IsEmpty()));
        auto mappings = field_formatter::CreateAutofillMappings(
            *user_data_.available_addresses_[0]->profile, "en-US");
        EXPECT_THAT(mappings,
                    IsSupersetOf({Pair(field_formatter::Key(3), "John"),
                                  Pair(field_formatter::Key(5), "Doe"),
                                  Pair(field_formatter::Key(7), "John Doe"),
                                  Pair(field_formatter::Key(30),
                                       "Brandschenkestrasse 110"),
                                  Pair(field_formatter::Key(35), "8002"),
                                  Pair(field_formatter::Key(33), "Zurich"),
                                  Pair(field_formatter::Key(36), "Switzerland"),
                                  Pair(field_formatter::Key(-8), "CH")}));

        std::move(collect_user_data_options->confirm_callback)
            .Run(&user_data_, &user_model_);
      });

  GetUserDataResponseProto user_data_response;
  user_data_response.set_locale("en-US");
  auto* address = user_data_response.add_available_addresses();
  AddCompleteAddressEntriesToMap("John Doe", address->mutable_values());
  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .WillOnce(RunOnceCallback<1>(true, user_data_response));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->set_shipping_address_name("shipping");
  collect_user_data->mutable_data_source();

  EXPECT_CALL(mock_personal_data_manager_, RecordUseOf).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, RawDataFromProtoDoesNotGetFormatted) {
  ON_CALL(mock_action_delegate_, GetPersonalDataManager)
      .WillByDefault(Return(nullptr));
  ON_CALL(mock_action_delegate_, MustUseBackendData)
      .WillByDefault(Return(true));
  ON_CALL(mock_action_delegate_, CollectUserData)
      .WillByDefault([&](CollectUserDataOptions* collect_user_data_options) {
        EXPECT_FALSE(collect_user_data_options->should_store_data_changes);
        EXPECT_THAT(user_data_.available_contacts_[0]->profile->guid(),
                    Not(IsEmpty()));
        auto mappings = field_formatter::CreateAutofillMappings(
            *user_data_.available_contacts_[0]->profile, "en-US");
        // Note: Phone number is still getting formatted on extraction, even if
        // it was added with |raw|.
        EXPECT_THAT(
            mappings,
            IsSupersetOf({Pair(field_formatter::Key(7), "John Doe"),
                          Pair(field_formatter::Key(14), "+11234567890")}));

        std::move(collect_user_data_options->confirm_callback)
            .Run(&user_data_, &user_model_);
      });

  GetUserDataResponseProto user_data_response;
  user_data_response.set_locale("en-US");
  auto* profile = user_data_response.add_available_contacts();
  (*profile->mutable_values())[7] =
      MakeAutofillEntry("John Doe", /* raw= */ true);
  (*profile->mutable_values())[14] =
      MakeAutofillEntry("+1 123-456-7890", /* raw= */ true);
  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .WillOnce(RunOnceCallback<1>(true, user_data_response));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->mutable_contact_details()->set_request_payer_name(true);
  collect_user_data->mutable_contact_details()->set_contact_details_name(
      kMemoryLocation);
  collect_user_data->mutable_data_source();

  EXPECT_CALL(mock_personal_data_manager_, RecordUseOf).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SelectEntriesFromProtoFromIdentifiers) {
  autofill::CountryNames::SetLocaleString("en-US");
  ON_CALL(mock_action_delegate_, GetPersonalDataManager)
      .WillByDefault(Return(nullptr));
  ON_CALL(mock_action_delegate_, MustUseBackendData)
      .WillByDefault(Return(true));
  ON_CALL(mock_action_delegate_, CollectUserData)
      .WillByDefault([&](CollectUserDataOptions* collect_user_data_options) {
        ASSERT_TRUE(user_data_.has_selected_address("contact"));
        EXPECT_EQ(user_data_.selected_address("contact")->GetRawInfo(
                      autofill::ServerFieldType::NAME_FULL),
                  u"John Doe");
        ASSERT_TRUE(user_data_.has_selected_address("shipping"));
        EXPECT_EQ(user_data_.selected_address("shipping")
                      ->GetRawInfo(autofill::ServerFieldType::NAME_FULL),
                  u"John Doe");
        ASSERT_TRUE(user_data_.has_selected_address("billing"));
        EXPECT_EQ(user_data_.selected_address("billing")->GetRawInfo(
                      autofill::ServerFieldType::NAME_FULL),
                  u"John Doe");
        ASSERT_TRUE(user_data_.selected_card());
        EXPECT_EQ(user_data_.selected_card()->GetRawInfo(
                      autofill::ServerFieldType::CREDIT_CARD_NAME_FULL),
                  u"John Doe");

        std::move(collect_user_data_options->confirm_callback)
            .Run(&user_data_, &user_model_);
      });

  // The default selection would select Jane Doe, as she is created later (see
  // |DefaultSelectEntriesFromProtoWithoutIdentifiers|), select John Doe by
  // identifier instead.
  GetUserDataResponseProto user_data_response;
  user_data_response.set_locale("en-US");
  user_data_response.set_selected_contact_identifier("selected-contact");
  auto* contact_1 = user_data_response.add_available_contacts();
  contact_1->set_identifier("selected-contact");
  (*contact_1->mutable_values())[7] = MakeAutofillEntry("John Doe");
  auto* contact_2 = user_data_response.add_available_contacts();
  contact_2->set_identifier("not-selected");
  (*contact_2->mutable_values())[7] = MakeAutofillEntry("Jane Doe");

  user_data_response.set_selected_shipping_address_identifier(
      "selected-address");
  auto* address_1 = user_data_response.add_available_addresses();
  address_1->set_identifier("selected-address");
  AddCompleteAddressEntriesToMap("John Doe", address_1->mutable_values());
  auto* address_2 = user_data_response.add_available_addresses();
  address_2->set_identifier("not-selected");
  AddCompleteAddressEntriesToMap("Jane Doe", address_2->mutable_values());

  user_data_response.set_selected_payment_instrument_identifier(
      "selected-instrument");
  auto* payment_instrument_1 =
      user_data_response.add_available_payment_instruments();
  payment_instrument_1->set_identifier("selected-instrument");
  AddCompleteCardEntriesToMap("John Doe",
                              payment_instrument_1->mutable_card_values());
  AddCompleteAddressEntriesToMap(
      "John Doe", payment_instrument_1->mutable_address_values());
  auto* payment_instrument_2 =
      user_data_response.add_available_payment_instruments();
  payment_instrument_2->set_identifier("not-selected");
  AddCompleteCardEntriesToMap("Jane Doe",
                              payment_instrument_2->mutable_card_values());
  AddCompleteAddressEntriesToMap(
      "Jane Doe", payment_instrument_2->mutable_address_values());
  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .WillOnce(RunOnceCallback<1>(true, user_data_response));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->mutable_contact_details()->set_request_payer_name(true);
  collect_user_data->mutable_contact_details()->set_contact_details_name(
      "contact");
  collect_user_data->set_shipping_address_name("shipping");
  collect_user_data->set_request_payment_method(true);
  collect_user_data->set_billing_address_name("billing");
  collect_user_data->mutable_data_source();

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest,
       DefaultSelectEntriesFromProtoWithoutIdentifiers) {
  autofill::CountryNames::SetLocaleString("en-US");
  ON_CALL(mock_action_delegate_, GetPersonalDataManager)
      .WillByDefault(Return(nullptr));
  ON_CALL(mock_action_delegate_, MustUseBackendData)
      .WillByDefault(Return(true));
  ON_CALL(mock_action_delegate_, CollectUserData)
      .WillByDefault([&](CollectUserDataOptions* collect_user_data_options) {
        ASSERT_TRUE(user_data_.has_selected_address("contact"));
        EXPECT_EQ(user_data_.selected_address("contact")->GetRawInfo(
                      autofill::ServerFieldType::NAME_FULL),
                  u"Jane Doe");
        ASSERT_TRUE(user_data_.has_selected_address("shipping"));
        EXPECT_EQ(user_data_.selected_address("shipping")
                      ->GetRawInfo(autofill::ServerFieldType::NAME_FULL),
                  u"Jane Doe");
        ASSERT_TRUE(user_data_.has_selected_address("billing"));
        EXPECT_EQ(user_data_.selected_address("billing")->GetRawInfo(
                      autofill::ServerFieldType::NAME_FULL),
                  u"Jane Doe");
        ASSERT_TRUE(user_data_.selected_card());
        EXPECT_EQ(user_data_.selected_card()->GetRawInfo(
                      autofill::ServerFieldType::CREDIT_CARD_NAME_FULL),
                  u"Jane Doe");

        std::move(collect_user_data_options->confirm_callback)
            .Run(&user_data_, &user_model_);
      });

  // The default selection will select Jane Doe, as she is created later.
  GetUserDataResponseProto user_data_response;
  user_data_response.set_locale("en-US");
  auto* contact_1 = user_data_response.add_available_contacts();
  (*contact_1->mutable_values())[7] = MakeAutofillEntry("John Doe");
  auto* contact_2 = user_data_response.add_available_contacts();
  (*contact_2->mutable_values())[7] = MakeAutofillEntry("Jane Doe");

  auto* address_1 = user_data_response.add_available_addresses();
  AddCompleteAddressEntriesToMap("John Doe", address_1->mutable_values());
  auto* address_2 = user_data_response.add_available_addresses();
  AddCompleteAddressEntriesToMap("Jane Doe", address_2->mutable_values());

  auto* payment_instrument_1 =
      user_data_response.add_available_payment_instruments();
  AddCompleteCardEntriesToMap("John Doe",
                              payment_instrument_1->mutable_card_values());
  AddCompleteAddressEntriesToMap(
      "John Doe", payment_instrument_1->mutable_address_values());
  auto* payment_instrument_2 =
      user_data_response.add_available_payment_instruments();
  AddCompleteCardEntriesToMap("Jane Doe",
                              payment_instrument_2->mutable_card_values());
  AddCompleteAddressEntriesToMap(
      "Jane Doe", payment_instrument_2->mutable_address_values());
  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .WillOnce(RunOnceCallback<1>(true, user_data_response));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->mutable_contact_details()->set_request_payer_name(true);
  collect_user_data->mutable_contact_details()->set_contact_details_name(
      "contact");
  collect_user_data->set_shipping_address_name("shipping");
  collect_user_data->set_request_payment_method(true);
  collect_user_data->set_billing_address_name("billing");
  collect_user_data->mutable_data_source();

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, RecordAddressUseOnlyOnce) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  auto* contact_details_proto =
      collect_user_data_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name("contact");
  contact_details_proto->set_request_payer_name(true);
  contact_details_proto->set_request_payer_email(true);
  contact_details_proto->set_request_payer_phone(true);
  collect_user_data_proto->set_request_terms_and_conditions(false);
  collect_user_data_proto->set_request_payment_method(true);
  collect_user_data_proto->set_billing_address_name("billing_address");
  collect_user_data_proto->set_shipping_address_name("shipping_address");

  std::string address_guid = base::GenerateGUID();
  autofill::AutofillProfile address(address_guid, kFakeUrl);
  autofill::test::SetProfileInfo(
      &address, "Marion", "Mitchell", "Morrison", "marion@me.xyz", "Fox",
      "123 Zoo St.", "unit 5", "Hollywood", "CA", "91601", "US", "16505678910");

  std::string card_guid = base::GenerateGUID();
  autofill::CreditCard credit_card(card_guid, kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card, "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2050",
                                    address.guid());

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_model_.SetSelectedAutofillProfile(
                "contact", std::make_unique<autofill::AutofillProfile>(address),
                &user_data_);
            user_model_.SetSelectedCreditCard(
                std::make_unique<autofill::CreditCard>(credit_card),
                &user_data_);
            user_model_.SetSelectedAutofillProfile(
                "billing_address",
                std::make_unique<autofill::AutofillProfile>(address),
                &user_data_);
            user_model_.SetSelectedAutofillProfile(
                "shipping_address",
                std::make_unique<autofill::AutofillProfile>(address),
                &user_data_);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(mock_personal_data_manager_,
              RecordUseOf(MatchingAutofillVariant(address_guid)))
      .Times(1);
  EXPECT_CALL(mock_personal_data_manager_,
              RecordUseOf(MatchingAutofillVariant(card_guid)))
      .Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, LogsUMAPrefilledSuccess) {
  base::HistogramTester histogram_tester;

  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  auto* contact_details_proto =
      collect_user_data_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name("contact");
  contact_details_proto->set_request_payer_name(true);
  contact_details_proto->set_request_payer_email(true);
  contact_details_proto->set_request_payer_phone(true);
  collect_user_data_proto->set_request_terms_and_conditions(false);
  collect_user_data_proto->set_request_payment_method(true);
  collect_user_data_proto->set_billing_address_name("billing_address");
  collect_user_data_proto->set_shipping_address_name("shipping_address");

  std::string address_guid = base::GenerateGUID();
  autofill::AutofillProfile address(address_guid, kFakeUrl);
  autofill::test::SetProfileInfo(
      &address, "Marion", "Mitchell", "Morrison", "marion@me.xyz", "Fox",
      "123 Zoo St.", "unit 5", "Hollywood", "CA", "91601", "US", "16505678910");

  autofill::CreditCard credit_card(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card, "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2050",
                                    address.guid());

  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(
          Return(std::vector<autofill::AutofillProfile*>({&address})));
  ON_CALL(mock_personal_data_manager_, GetCreditCards)
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&credit_card})));
  ON_CALL(mock_personal_data_manager_, GetProfileByGUID(address_guid))
      .WillByDefault(Return(&address));
  EXPECT_CALL(mock_action_delegate_, CollectUserData(_))
      .WillOnce(Invoke([=](CollectUserDataOptions* collect_user_data_options) {
        // Do not select anything here, the data is expected to be complete
        // through default selection.
        std::move(collect_user_data_options->confirm_callback)
            .Run(&user_data_, &user_model_);
      }));

  std::unique_ptr<CollectUserDataAction> action =
      std::make_unique<CollectUserDataAction>(&mock_action_delegate_,
                                              action_proto);
  action->ProcessAction(callback_.Get());
  // We can't wait for the callback_ to be called and destroy the action there,
  // it will trigger a "heap use after free" error.
  action.reset();  // Destroy to write histogram entries.
  histogram_tester.ExpectUniqueSample(
      "Android.AutofillAssistant.PaymentRequest.Prefilled",
      Metrics::PaymentRequestPrefilled::PREFILLED_SUCCESS, 1u);
}

TEST_F(CollectUserDataActionTest, LogsUMANotPrefilledSuccess) {
  base::HistogramTester histogram_tester;

  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* contact_details_proto =
      collect_user_data_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name("contact");
  contact_details_proto->set_request_payer_name(true);
  contact_details_proto->set_request_payer_email(true);
  contact_details_proto->set_request_payer_phone(true);

  EXPECT_CALL(mock_action_delegate_, CollectUserData(_))
      .WillOnce(Invoke([=](CollectUserDataOptions* collect_user_data_options) {
        std::move(collect_user_data_options->confirm_callback)
            .Run(&user_data_, &user_model_);
      }));

  std::unique_ptr<CollectUserDataAction> action =
      std::make_unique<CollectUserDataAction>(&mock_action_delegate_,
                                              action_proto);
  action->ProcessAction(callback_.Get());
  // We can't wait for the callback_ to be called and destroy the action there,
  // it will trigger a "heap use after free" error.
  action.reset();  // Destroy to write histogram entries.
  histogram_tester.ExpectUniqueSample(
      "Android.AutofillAssistant.PaymentRequest.Prefilled",
      Metrics::PaymentRequestPrefilled::NOTPREFILLED_SUCCESS, 1u);
}

TEST_F(CollectUserDataActionTest, LogsUMAAutofillChangedSuccess) {
  base::HistogramTester histogram_tester;

  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);

  auto* contact_details_proto =
      collect_user_data_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name("contact");
  contact_details_proto->set_request_payer_name(true);
  contact_details_proto->set_request_payer_email(true);
  contact_details_proto->set_request_payer_phone(true);

  EXPECT_CALL(mock_action_delegate_, CollectUserData(_))
      .WillOnce(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            // Notify before EndAction() is called.
            mock_personal_data_manager_.NotifyPersonalDataObserver();
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  std::unique_ptr<CollectUserDataAction> action =
      std::make_unique<CollectUserDataAction>(&mock_action_delegate_,
                                              action_proto);

  action->ProcessAction(callback_.Get());
  // We can't wait for the callback_ to be called and destroy the action there,
  // it will trigger a "heap use after free" error.
  action.reset();  // Destroy to write histogram entries.
  histogram_tester.ExpectUniqueSample(
      "Android.AutofillAssistant.PaymentRequest.AutofillChanged",
      Metrics::PaymentRequestAutofillInfoChanged::CHANGED_SUCCESS, 1u);
}

TEST_F(CollectUserDataActionTest, LogsUMAAutofillNotChangedSuccess) {
  base::HistogramTester histogram_tester;

  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);

  auto* contact_details_proto =
      collect_user_data_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name("contact");
  contact_details_proto->set_request_payer_name(true);
  contact_details_proto->set_request_payer_email(true);
  contact_details_proto->set_request_payer_phone(true);

  EXPECT_CALL(mock_action_delegate_, CollectUserData(_))
      .WillOnce(Invoke([=](CollectUserDataOptions* collect_user_data_options) {
        // No PDM change.
        std::move(collect_user_data_options->confirm_callback)
            .Run(&user_data_, &user_model_);
      }));

  std::unique_ptr<CollectUserDataAction> action =
      std::make_unique<CollectUserDataAction>(&mock_action_delegate_,
                                              action_proto);

  action->ProcessAction(callback_.Get());
  // We can't wait for the callback_ to be called and destroy the action there,
  // it will trigger a "heap use after free" error.
  action.reset();  // Destroy to write histogram entries.
  histogram_tester.ExpectUniqueSample(
      "Android.AutofillAssistant.PaymentRequest.AutofillChanged",
      Metrics::PaymentRequestAutofillInfoChanged::NOTCHANGED_SUCCESS, 1u);
}

TEST_F(CollectUserDataActionTest, LogsUkmSelectionStateUpdated) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  collect_user_data_proto->mutable_contact_details()->set_request_payer_name(
      true);
  collect_user_data_proto->mutable_contact_details()->set_contact_details_name(
      "name");
  collect_user_data_proto->set_request_payment_method(true);
  collect_user_data_proto->set_billing_address_name("billing_address");
  collect_user_data_proto->set_shipping_address_name("shipping");

  autofill::AutofillProfile billing_profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&billing_profile, "Marion", "Mitchell",
                                 "Morrison", "marion@me.xyz", "Fox",
                                 "123 Zoo St.", "unit 5", "Hollywood", "CA",
                                 "91601", "US", "16505678910");

  autofill::CreditCard credit_card(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card, "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2050",
                                    billing_profile.guid());

  autofill::AutofillProfile shipping_address(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&shipping_address, "Marion", "Mitchell",
                                 "Morrison", "marion@me.xyz", "Fox",
                                 "123 Zoo St.", "unit 5", "Hollywood", "CA",
                                 "91601", "US", "16505678910");

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault([&](CollectUserDataOptions* collect_user_data_options) {
        user_data_.terms_and_conditions_ = ACCEPTED;
        collect_user_data_options->selected_user_data_changed_callback.Run(
            UserDataEventField::CONTACT_EVENT, SELECTION_CHANGED);
        collect_user_data_options->selected_user_data_changed_callback.Run(
            UserDataEventField::CREDIT_CARD_EVENT, ENTRY_CREATED);
        collect_user_data_options->selected_user_data_changed_callback.Run(
            UserDataEventField::SHIPPING_EVENT, ENTRY_EDITED);
        user_model_.SetSelectedCreditCard(
            std::make_unique<autofill::CreditCard>(credit_card), &user_data_);
        user_model_.SetSelectedAutofillProfile(
            "billing_address",
            std::make_unique<autofill::AutofillProfile>(billing_profile),
            &user_data_);
        user_model_.SetSelectedAutofillProfile(
            "shipping",
            std::make_unique<autofill::AutofillProfile>(shipping_address),
            &user_data_);
        std::move(collect_user_data_options->confirm_callback)
            .Run(&user_data_, &user_model_);
      });

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_THAT(
      GetUkmContactModified(ukm_recorder_),
      ElementsAreArray({ToHumanReadableEntry(
          source_id_, kContactModified,
          static_cast<int64_t>(
              Metrics::UserDataSelectionState::SELECTED_DIFFERENT_ENTRY))}));
  EXPECT_THAT(
      GetUkmCreditCardModified(ukm_recorder_),
      ElementsAreArray({ToHumanReadableEntry(
          source_id_, kCreditCardModified,
          static_cast<int64_t>(Metrics::UserDataSelectionState::NEW_ENTRY))}));
  EXPECT_THAT(GetUkmShippingModified(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kShippingModified,
                  static_cast<int64_t>(
                      Metrics::UserDataSelectionState::EDIT_PRESELECTED))}));
}

TEST_F(CollectUserDataActionTest,
       SelectionStateNotLoggedIfNoUserDataRequested) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(false);
  collect_user_data_proto->set_terms_require_review_text("terms review");

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault([&](CollectUserDataOptions* collect_user_data_options) {
        user_data_.terms_and_conditions_ = ACCEPTED;

        std::move(collect_user_data_options->confirm_callback)
            .Run(&user_data_, &user_model_);
      });

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_THAT(GetUkmContactModified(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmCreditCardModified(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmShippingModified(ukm_recorder_), IsEmpty());
}

TEST_F(CollectUserDataActionTest, LogsUkmProfilesCount) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  // Note that each profile has a different last name to avoid deduplication
  // This profile is a complete contact and shipping address.
  autofill::AutofillProfile complete;
  autofill::test::SetProfileInfo(&complete, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Main St. 18", "",
                                 "abc", "New York", "NY", "10001", "US", "");
  // This profile is a complete contact but incomplete as a shipping address.
  autofill::AutofillProfile incomplete_for_shipping;
  autofill::test::SetProfileInfo(&incomplete_for_shipping, "Berta", "West", "",
                                 "", "", "", "", "", "", "", "", "", "");
  // This profile is incomplete both as a contact and as a shipping address.
  autofill::AutofillProfile incomplete;
  autofill::test::SetProfileInfo(&incomplete, "", "", "Bird", "", "", "", "",
                                 "", "", "", "", "", "");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(Return(std::vector<autofill::AutofillProfile*>(
          {&complete, &incomplete_for_shipping, &incomplete})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  auto* contact_details = user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_request_payer_email(true);
  contact_details->set_contact_details_name("contact");
  user_data->set_shipping_address_name("shipping-address");
  *contact_details->add_required_data_piece() =
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FIRST);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_THAT(GetUkmCompleteContactProfilesCount(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kCompleteContactProfilesCount, 2)}));
  EXPECT_THAT(GetUkmIncompleteContactProfilesCount(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kIncompleteContactProfilesCount, 1)}));
  EXPECT_THAT(GetUkmCompleteShippingProfilesCount(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kCompleteShippingProfilesCount, 1)}));
  EXPECT_THAT(GetUkmIncompleteShippingProfilesCount(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kIncompleteShippingProfilesCount, 2)}));
}

TEST_F(CollectUserDataActionTest, LogsUkmCreditCardsCount) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::AutofillProfile complete_billing_address;
  autofill::test::SetProfileInfo(&complete_billing_address, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK", "+44");
  autofill::AutofillProfile incomplete_billing_address;
  autofill::test::SetProfileInfo(&incomplete_billing_address, "", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK", "+44");

  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("GUID"))
      .WillByDefault(Return(&complete_billing_address));
  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("INCOMPLETE_GUID"))
      .WillByDefault(Return(&incomplete_billing_address));

  autofill::CreditCard card_with_complete_address;
  autofill::test::SetCreditCardInfo(&card_with_complete_address, "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "GUID");
  autofill::CreditCard card_with_incomplete_address;
  autofill::test::SetCreditCardInfo(
      &card_with_incomplete_address, "Berta West", "4111111111111111", "1",
      "2050",
      /* billing_address_id= */ "INCOMPLETE_GUID");
  autofill::CreditCard incomplete_card;
  autofill::test::SetCreditCardInfo(&incomplete_card, "Jim West",
                                    "4111111111111111", "1",
                                    /* expiration_year= */ "",
                                    /* billing_address_id= */ "GUID");

  // Cards with unsupported networks are filtered out early and not shown as
  // option to the user in the UI, so they are not counted as neither complete
  // or incomplete.
  autofill::CreditCard unsupported_network_card;
  autofill::test::SetCreditCardInfo(&unsupported_network_card, "Jim West",
                                    "378282246310005", "1", "",
                                    /* billing_address_id= */ "GUID");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(Return(std::vector<autofill::CreditCard*>(
          {&card_with_complete_address, &card_with_incomplete_address,
           &incomplete_card, &unsupported_network_card})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_model_.SetSelectedCreditCard(
                std::make_unique<autofill::CreditCard>(
                    card_with_complete_address),
                &user_data_);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  user_data->set_request_payment_method(true);
  user_data->set_billing_address_name("billing");
  user_data->add_supported_basic_card_networks("visa");
  *user_data->add_required_billing_address_data_piece() =
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FIRST);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_THAT(GetUkmCompleteCreditCardsCount(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kCompleteCreditCardsCount, 1)}));
  EXPECT_THAT(GetUkmIncompleteCreditCardsCount(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kIncompleteCreditCardsCount, 2)}));
}

TEST_F(CollectUserDataActionTest, ProfilesNotDeduplicatedWithDisabledFlag) {
  base::HistogramTester histogram_tester;
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillAssistantCudFilterProfiles);
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(&profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Main St. 18", "",
                                 "abc", "New York", "NY", "10001", "US", "");
  // We return the 6 profiles, to verify that the count is correctly
  // set in the |MORE_THAN_FIVE| bucket.
  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(Return(std::vector<autofill::AutofillProfile*>(
          {&profile, &profile, &profile, &profile, &profile, &profile})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  auto* contact_details = user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_request_payer_email(true);
  contact_details->set_contact_details_name("contact");
  user_data->set_shipping_address_name("shipping-address");
  *contact_details->add_required_data_piece() =
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FIRST);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_THAT(
      GetUkmCompleteContactProfilesCount(ukm_recorder_),
      ElementsAreArray({ToHumanReadableEntry(
          source_id_, kCompleteContactProfilesCount,
          static_cast<int64_t>(Metrics::UserDataEntryCount::FIVE_OR_MORE))}));
  EXPECT_THAT(
      GetUkmCompleteShippingProfilesCount(ukm_recorder_),
      ElementsAreArray({ToHumanReadableEntry(
          source_id_, kCompleteShippingProfilesCount,
          static_cast<int64_t>(Metrics::UserDataEntryCount::FIVE_OR_MORE))}));

  histogram_tester.ExpectTotalCount(
      "Android.AutofillAssistant.Cud.AutofillProfileDeduplicatedAddress",
      1u);
  histogram_tester.ExpectTotalCount(
      "Android.AutofillAssistant.Cud.AutofillProfileDeduplicatedContact",
      1u);
}

TEST_F(CollectUserDataActionTest, ProfilesDeduplicatedWithEnabledFlag) {
  base::HistogramTester histogram_tester;
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillAssistantCudFilterProfiles);
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(&profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Main St. 18", "",
                                 "abc", "New York", "NY", "10001", "US", "");
  // We return 6 duplicate profiles, to verify that they are deduplicated
  // correctly and the count is correctly set in the |ONE| bucket.
  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(Return(std::vector<autofill::AutofillProfile*>(
          {&profile, &profile, &profile, &profile, &profile, &profile})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  auto* contact_details = user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_request_payer_email(true);
  contact_details->set_contact_details_name("contact");
  user_data->set_shipping_address_name("shipping-address");
  *contact_details->add_required_data_piece() =
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FIRST);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_THAT(GetUkmCompleteContactProfilesCount(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kCompleteContactProfilesCount,
                  static_cast<int64_t>(Metrics::UserDataEntryCount::ONE))}));
  EXPECT_THAT(GetUkmCompleteShippingProfilesCount(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kCompleteShippingProfilesCount,
                  static_cast<int64_t>(Metrics::UserDataEntryCount::ONE))}));

  histogram_tester.ExpectTotalCount(
      "Android.AutofillAssistant.Cud.AutofillProfileDeduplicatedAddress",
      1u);
  histogram_tester.ExpectTotalCount(
      "Android.AutofillAssistant.Cud.AutofillProfileDeduplicatedContact",
      1u);
}

TEST_F(CollectUserDataActionTest, LogsUkmMoreThanFiveProfilesCount) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  std::vector<autofill::AutofillProfile*> profile_pointers;
  for (int i = 0; i < 6; i++) {
    // We are changing the last name of each profile to make sure they don't get
    // deduplicated
    std::unique_ptr<autofill::AutofillProfile> profile =
        std::make_unique<autofill::AutofillProfile>();
    std::string last_name = "West" + base::NumberToString(i);
    autofill::test::SetProfileInfo(profile.get(), "Adam", "", last_name.data(),
                                   "adam.west@gmail.com", "", "Main St. 18", "",
                                   "abc", "New York", "NY", "10001", "US", "");
    profile_pointers.push_back(profile.get());
    profiles.push_back(std::move(profile));
  }

  // We return the 6 profiles, to verify that the count is correctly
  // set in the |MORE_THAN_FIVE| bucket.
  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(Return(profile_pointers));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  auto* contact_details = user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_request_payer_email(true);
  contact_details->set_contact_details_name("contact");
  user_data->set_shipping_address_name("shipping-address");
  *contact_details->add_required_data_piece() =
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FIRST);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_THAT(
      GetUkmCompleteContactProfilesCount(ukm_recorder_),
      ElementsAreArray({ToHumanReadableEntry(
          source_id_, kCompleteContactProfilesCount,
          static_cast<int64_t>(Metrics::UserDataEntryCount::FIVE_OR_MORE))}));
  EXPECT_THAT(
      GetUkmCompleteShippingProfilesCount(ukm_recorder_),
      ElementsAreArray({ToHumanReadableEntry(
          source_id_, kCompleteShippingProfilesCount,
          static_cast<int64_t>(Metrics::UserDataEntryCount::FIVE_OR_MORE))}));
}

TEST_F(CollectUserDataActionTest, LogUkmSuccess) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(false);
  collect_user_data_proto->set_terms_require_review_text("terms review");

  base::OnceCallback<void(UserData*, const UserModel*)> confirm_callback;
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault([&](CollectUserDataOptions* collect_user_data_options) {
        user_data_.terms_and_conditions_ = ACCEPTED;

        confirm_callback =
            std::move(collect_user_data_options->confirm_callback);
      });

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);

  AddWaitTime(&action, base::Milliseconds(4000));
  action.ProcessAction(callback_.Get());

  ASSERT_TRUE(confirm_callback);
  std::move(confirm_callback).Run(&user_data_, &user_model_);

  EXPECT_THAT(
      GetUkmCollectUserDataResult(ukm_recorder_),
      ElementsAreArray({ToHumanReadableEntry(
          source_id_, kResult,
          static_cast<int64_t>(Metrics::CollectUserDataResult::SUCCESS))}));
  EXPECT_THAT(
      GetUkmTimeTakenMs(ukm_recorder_),
      ElementsAreArray({ToHumanReadableEntry(source_id_, kTimeTakenMs, 4000)}));
  EXPECT_THAT(
      GetUkmUserDataSource(ukm_recorder_),
      ElementsAreArray({ToHumanReadableEntry(
          source_id_, kUserDataSource,
          static_cast<int64_t>(Metrics::UserDataSource::CHROME_AUTOFILL))}));
}

TEST_F(CollectUserDataActionTest, LogUkmAdditionalActionSelected) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(false);
  collect_user_data_proto->set_terms_require_review_text("terms review");
  collect_user_data_proto->add_additional_actions();

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault([&](CollectUserDataOptions* collect_user_data_options) {
        std::move(collect_user_data_options->additional_actions_callback)
            .Run(/* index= */ 0, &user_data_, &user_model_);
      });

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_THAT(
      GetUkmCollectUserDataResult(ukm_recorder_),
      ElementsAreArray({ToHumanReadableEntry(
          source_id_, kResult,
          static_cast<int64_t>(
              Metrics::CollectUserDataResult::ADDITIONAL_ACTION_SELECTED))}));
}

TEST_F(CollectUserDataActionTest, LogUkmFailure) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(false);
  collect_user_data_proto->set_terms_require_review_text("terms review");

  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    ON_CALL(mock_action_delegate_, CollectUserData(_))
        .WillByDefault([&](CollectUserDataOptions* collect_user_data_options) {
          // The continue button is never pressed.
        });
    AddWaitTime(&action, base::Milliseconds(3000));
    action.ProcessAction(callback_.Get());

    // The CollectUserDataAction destructor is called, this simulates the user
    // closing the bottom sheet or the tab.
  }
  EXPECT_THAT(
      GetUkmCollectUserDataResult(ukm_recorder_),
      ElementsAreArray({ToHumanReadableEntry(
          source_id_, kResult,
          static_cast<int64_t>(Metrics::CollectUserDataResult::FAILURE))}));
  EXPECT_THAT(
      GetUkmTimeTakenMs(ukm_recorder_),
      ElementsAreArray({ToHumanReadableEntry(source_id_, kTimeTakenMs, 3000)}));
}

TEST_F(CollectUserDataActionTest, LogUkmDataFromBackend) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(false);
  collect_user_data_proto->set_terms_require_review_text("terms review");
  collect_user_data_proto->mutable_data_source();

  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .WillRepeatedly(RunOnceCallback<1>(true, GetUserDataResponseProto()));
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault([&](CollectUserDataOptions* collect_user_data_options) {
        user_data_.terms_and_conditions_ = ACCEPTED;

        std::move(collect_user_data_options->confirm_callback)
            .Run(&user_data_, &user_model_);
      });

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_THAT(
      GetUkmCollectUserDataResult(ukm_recorder_),
      ElementsAreArray({ToHumanReadableEntry(
          source_id_, kResult,
          static_cast<int64_t>(Metrics::CollectUserDataResult::SUCCESS))}));
  EXPECT_THAT(GetUkmUserDataSource(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kUserDataSource,
                  static_cast<int64_t>(Metrics::UserDataSource::BACKEND))}));
}

TEST_F(CollectUserDataActionTest, LogUkmDataFallbackBackendData) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(false);
  collect_user_data_proto->set_terms_require_review_text("terms review");
  collect_user_data_proto->mutable_data_source()
      ->set_allow_fallback_on_missing_data(true);

  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_action_delegate_, MustUseBackendData)
      .WillByDefault(Return(false));

  GetUserDataResponseProto user_data_response;
  user_data_response.set_locale("en-US");
  auto* profile = user_data_response.add_available_contacts();
  (*profile->mutable_values())[7] = MakeAutofillEntry("John Doe");
  (*profile->mutable_values())[14] = MakeAutofillEntry("+1 123-456-7890");
  *user_data_response.add_available_phone_numbers()->mutable_value() =
      MakeAutofillEntry("+1 187-654-3210");
  auto* address_1 = user_data_response.add_available_addresses();
  AddCompleteAddressEntriesToMap("John Doe", address_1->mutable_values());
  auto* payment_instrument_1 =
      user_data_response.add_available_payment_instruments();
  AddCompleteCardEntriesToMap("John Doe",
                              payment_instrument_1->mutable_card_values());

  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .WillRepeatedly(RunOnceCallback<1>(true, user_data_response));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault([&](CollectUserDataOptions* collect_user_data_options) {
        user_data_.terms_and_conditions_ = ACCEPTED;

        std::move(collect_user_data_options->confirm_callback)
            .Run(&user_data_, &user_model_);
      });

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction fallback_action(&mock_action_delegate_, action_proto);
  fallback_action.ProcessAction(callback_.Get());
  EXPECT_THAT(
      GetUkmCollectUserDataResult(ukm_recorder_),
      ElementsAreArray({ToHumanReadableEntry(
          source_id_, kResult,
          static_cast<int64_t>(Metrics::CollectUserDataResult::SUCCESS))}));
  EXPECT_THAT(
      GetUkmUserDataSource(ukm_recorder_),
      ElementsAreArray({ToHumanReadableEntry(
          source_id_, kUserDataSource,
          static_cast<int64_t>(Metrics::UserDataSource::FALLBACK_BACKEND))}));
}

TEST_F(CollectUserDataActionTest, LogUkmFallbackChromeAutofillDataFromBackend) {
  GetUserDataResponseProto user_data_response;
  user_data_response.set_locale("en-US");
  auto* response_profile = user_data_response.add_available_contacts();
  (*response_profile->mutable_values())[7] = MakeAutofillEntry("John Doe");
  autofill::CountryNames::SetLocaleString("en-US");
  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .WillRepeatedly(RunOnceCallback<1>(true, user_data_response));

  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(
      &profile, "Adam", "", "West", "adam.west@gmail.com", "", "Main St. 18",
      "", "abc", "New York", "NY", "10001", "us", "+1 123-456-7890");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(
          Return(std::vector<autofill::AutofillProfile*>({&profile})));
  ON_CALL(mock_action_delegate_, MustUseBackendData)
      .WillByDefault(Return(false));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->set_request_payment_method(false);
  collect_user_data->set_billing_address_name("billing");
  collect_user_data->mutable_data_origin_notice()->set_link_text("Link");
  collect_user_data->mutable_data_origin_notice()->set_dialog_title("Title");
  collect_user_data->mutable_data_origin_notice()->set_dialog_text("Text");
  collect_user_data->mutable_data_origin_notice()->set_dialog_button_text(
      "Button");
  collect_user_data->mutable_data_source()->set_allow_fallback_on_missing_data(
      true);

  auto* contact_details = collect_user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_request_payer_email(true);
  contact_details->set_contact_details_name("contact");
  collect_user_data->set_shipping_address_name("shipping-address");
  collect_user_data->mutable_data_source();

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault([&](CollectUserDataOptions* collect_user_data_options) {
        ExpectSelectedProfileMatches("contact", &profile);
        ExpectSelectedProfileMatches("shipping-address", &profile);
        user_data_.terms_and_conditions_ = ACCEPTED;
        std::move(collect_user_data_options->confirm_callback)
            .Run(&user_data_, &user_model_);
      });

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_THAT(
      GetUkmCollectUserDataResult(ukm_recorder_),
      ElementsAreArray({ToHumanReadableEntry(
          source_id_, kResult,
          static_cast<int64_t>(Metrics::CollectUserDataResult::SUCCESS))}));
  EXPECT_THAT(GetUkmUserDataSource(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kUserDataSource,
                  static_cast<int64_t>(
                      Metrics::UserDataSource::
                          FALLBACK_CHROME_AUTOFILL_ON_MISSING_DATA))}));
}

TEST_F(CollectUserDataActionTest, LogsUkmInitialSelectionFieldBitArray) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  // We add artificial constraints on the fields in the CUD proto below to
  // make sure that we get a different profile as default for each kind of
  // entry.
  autofill::AutofillProfile default_contact;
  autofill::test::SetProfileInfo(&default_contact, "Adam", "", "",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "", "", "", "", "");
  int expected_contact_bitarray =
      Metrics::AutofillAssistantProfileFields::NAME_FIRST |
      Metrics::AutofillAssistantProfileFields::NAME_FULL |
      Metrics::AutofillAssistantProfileFields::EMAIL_ADDRESS |
      Metrics::AutofillAssistantProfileFields::ADDRESS_HOME_LINE1;

  autofill::AutofillProfile default_shipping;
  autofill::test::SetProfileInfo(&default_shipping, "", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "", "", "", "", "");
  int expected_shipping_bitarray =
      Metrics::AutofillAssistantProfileFields::NAME_LAST |
      Metrics::AutofillAssistantProfileFields::NAME_FULL |
      Metrics::AutofillAssistantProfileFields::EMAIL_ADDRESS |
      Metrics::AutofillAssistantProfileFields::ADDRESS_HOME_LINE1;

  autofill::AutofillProfile default_billing;
  autofill::test::SetProfileInfo(&default_billing, "", "", "",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "", "", "", "", "");
  int expected_billing_bitarray =
      Metrics::AutofillAssistantProfileFields::EMAIL_ADDRESS |
      Metrics::AutofillAssistantProfileFields::ADDRESS_HOME_LINE1;

  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("GUID"))
      .WillByDefault(Return(&default_billing));

  autofill::CreditCard default_card;
  autofill::test::SetCreditCardInfo(&default_card, "Jim West",
                                    "4111111111111111", "1",
                                    /* expiration_year= */ "",
                                    /* billing_address_id= */ "GUID");
  int expected_credit_card_bitarray =
      Metrics::AutofillAssistantCreditCardFields::CREDIT_CARD_NAME_FULL |
      Metrics::AutofillAssistantCreditCardFields::CREDIT_CARD_EXP_MONTH |
      Metrics::AutofillAssistantCreditCardFields::VALID_NUMBER;

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&default_card})));

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(Return(std::vector<autofill::AutofillProfile*>(
          {&default_contact, &default_shipping, &default_billing})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            // We can't submit here since the user data is not complete.
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  user_data->add_supported_basic_card_networks("visa");
  user_data->set_request_payment_method(true);
  user_data->set_billing_address_name("billing");
  auto* contact_details = user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_request_payer_email(true);
  contact_details->set_contact_details_name("contact");
  *contact_details->add_required_data_piece() =
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FIRST);
  user_data->set_shipping_address_name("shipping-address");
  *user_data->add_required_shipping_address_data_piece() =
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_LAST);

  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());

    // The CollectUserDataAction destructor is called, this simulates the user
    // closing the bottom sheet or the tab.
  }

  EXPECT_THAT(GetUkmInitialContactFieldsStatus(ukm_recorder_),
              ElementsAreArray(
                  {ToHumanReadableEntry(source_id_, kInitialContactFieldsStatus,
                                        expected_contact_bitarray)}));
  EXPECT_THAT(GetUkmInitialShippingFieldsStatus(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kInitialShippingFieldsStatus,
                  expected_shipping_bitarray)}));
  EXPECT_THAT(GetUkmInitialBillingAddressFieldsStatus(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kInitialBillingAddressFieldsStatus,
                  expected_billing_bitarray)}));
  EXPECT_THAT(GetUkmInitialCreditCardFieldsStatus(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kInitialCreditCardFieldsStatus,
                  expected_credit_card_bitarray)}));
}

TEST_F(CollectUserDataActionTest, NoDefaultProfileLogsAllFieldsAsEmpty) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            // We can't submit here since the user data is not complete.
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  user_data->set_request_payment_method(true);
  user_data->set_billing_address_name("billing");
  auto* contact_details = user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_request_payer_email(true);
  contact_details->set_contact_details_name("contact");
  user_data->set_shipping_address_name("shipping-address");

  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());

    // The CollectUserDataAction destructor is called, this simulates the user
    // closing the bottom sheet or the tab.
  }

  EXPECT_THAT(GetUkmInitialContactFieldsStatus(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kInitialContactFieldsStatus, 0)}));
  EXPECT_THAT(GetUkmInitialShippingFieldsStatus(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kInitialShippingFieldsStatus, 0)}));
  EXPECT_THAT(GetUkmInitialBillingAddressFieldsStatus(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kInitialBillingAddressFieldsStatus, 0)}));
  EXPECT_THAT(GetUkmInitialCreditCardFieldsStatus(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kInitialCreditCardFieldsStatus, 0)}));
}

TEST_F(CollectUserDataActionTest, ReloadsDataIfRequested) {
  base::HistogramTester histogram_tester;

  ON_CALL(mock_action_delegate_, GetPersonalDataManager)
      .WillByDefault(Return(nullptr));
  ON_CALL(mock_action_delegate_, MustUseBackendData)
      .WillByDefault(Return(true));
  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .Times(3)
      .WillRepeatedly(RunOnceCallback<1>(true, GetUserDataResponseProto()));
  EXPECT_CALL(mock_action_delegate_, CollectUserData(_))
      .WillOnce(Invoke([=](CollectUserDataOptions* collect_user_data_options) {
        std::move(collect_user_data_options->reload_data_callback)
            .Run(UserDataEventField::NONE, &user_data_);
      }))
      .WillOnce(Invoke([=](CollectUserDataOptions* collect_user_data_options) {
        std::move(collect_user_data_options->reload_data_callback)
            .Run(UserDataEventField::NONE, &user_data_);
      }))
      .WillOnce(Invoke([=](CollectUserDataOptions* collect_user_data_options) {
        // We can't submit here since the user data is not complete.
      }));

  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(false);
  collect_user_data_proto->mutable_data_source();

  // We do not expect the action to end.
  std::unique_ptr<CollectUserDataAction> action =
      std::make_unique<CollectUserDataAction>(&mock_action_delegate_,
                                              action_proto);
  action->ProcessAction(callback_.Get());

  action.reset();
  histogram_tester.ExpectTotalCount(
      "Android.AutofillAssistant.PaymentRequest.AutofillChanged", 1u);
}

TEST_F(CollectUserDataActionTest, MergesTransientDataWithUserDataFromBackend) {
  auto transient_contact = std::make_unique<autofill::AutofillProfile>();
  transient_contact->SetRawInfo(autofill::NAME_FULL, u"Jane Doe");
  user_data_.transient_contacts_.emplace_back(
      std::make_unique<Contact>(std::move(transient_contact)));

  auto transient_phone_number = std::make_unique<autofill::AutofillProfile>();
  transient_phone_number->SetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER,
                                     u"+16505678910");
  user_data_.transient_phone_numbers_.emplace_back(
      std::make_unique<PhoneNumber>(std::move(transient_phone_number)));

  GetUserDataResponseProto user_data_response;
  user_data_response.set_locale("en-US");
  auto* profile = user_data_response.add_available_contacts();
  (*profile->mutable_values())[7] = MakeAutofillEntry("John Doe");
  *user_data_response.add_available_phone_numbers()->mutable_value() =
      MakeAutofillEntry("+1 187-654-3210");

  ON_CALL(mock_action_delegate_, GetPersonalDataManager)
      .WillByDefault(Return(nullptr));
  ON_CALL(mock_action_delegate_, MustUseBackendData)
      .WillByDefault(Return(true));
  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .Times(2)
      .WillRepeatedly(RunOnceCallback<1>(true, user_data_response));
  EXPECT_CALL(mock_action_delegate_, CollectUserData(_))
      .WillOnce(Invoke([=](CollectUserDataOptions* collect_user_data_options) {
        ASSERT_EQ(user_data_.available_contacts_.size(), 2u);
        EXPECT_EQ(user_data_.available_contacts_[0]->profile->GetRawInfo(
                      autofill::NAME_FULL),
                  u"Jane Doe");
        EXPECT_EQ(user_data_.available_contacts_[1]->profile->GetRawInfo(
                      autofill::NAME_FULL),
                  u"John Doe");

        ASSERT_EQ(user_data_.available_phone_numbers_.size(), 2u);
        EXPECT_EQ(user_data_.available_phone_numbers_[0]->profile->GetRawInfo(
                      autofill::PHONE_HOME_WHOLE_NUMBER),
                  u"+16505678910");
        EXPECT_EQ(user_data_.available_phone_numbers_[1]->profile->GetRawInfo(
                      autofill::PHONE_HOME_WHOLE_NUMBER),
                  u"+1 187-654-3210");

        std::move(collect_user_data_options->reload_data_callback)
            .Run(UserDataEventField::NONE, &user_data_);
      }))
      .WillOnce(Invoke([=](CollectUserDataOptions* collect_user_data_options) {
        EXPECT_EQ(user_data_.available_contacts_.size(), 2u);
        EXPECT_EQ(user_data_.available_phone_numbers_.size(), 2u);

        // Don't end the action.
      }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->mutable_contact_details()->set_request_payer_name(true);
  collect_user_data->mutable_contact_details()
      ->set_separate_phone_number_section(true);
  collect_user_data->mutable_contact_details()->set_phone_number_section_title(
      "Phone number");
  collect_user_data->mutable_contact_details()->set_contact_details_name(
      kMemoryLocation);
  collect_user_data->mutable_data_source();

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, FallBackToChromeDataOnFailedRequest) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(
      &profile, "Adam", "", "West", "adam.west@gmail.com", "", "Main St. 18",
      "", "abc", "New York", "NY", "10001", "us", "+1 123-456-7890");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(
          Return(std::vector<autofill::AutofillProfile*>({&profile})));
  ON_CALL(mock_action_delegate_, MustUseBackendData)
      .WillByDefault(Return(false));
  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .WillOnce(RunOnceCallback<1>(false, GetUserDataResponseProto()));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault([&](CollectUserDataOptions* collect_user_data_options) {
        ExpectSelectedProfileMatches(kMemoryLocation, &profile);

        EXPECT_TRUE(collect_user_data_options->request_payer_phone);
        EXPECT_FALSE(
            collect_user_data_options->request_phone_number_separately);
        EXPECT_THAT(collect_user_data_options->contact_summary_fields,
                    ElementsAre(AutofillContactField::NAME_FULL,
                                AutofillContactField::PHONE_HOME_WHOLE_NUMBER));
        EXPECT_EQ(collect_user_data_options->contact_summary_max_lines, 2);
        EXPECT_THAT(collect_user_data_options->contact_full_fields,
                    ElementsAre(AutofillContactField::NAME_FULL,
                                AutofillContactField::PHONE_HOME_WHOLE_NUMBER));
        EXPECT_EQ(collect_user_data_options->contact_full_max_lines, 2);
        EXPECT_FALSE(collect_user_data_options->data_origin_notice.has_value());
        EXPECT_TRUE(collect_user_data_options->should_store_data_changes);
        EXPECT_FALSE(collect_user_data_options->use_alternative_edit_dialogs);

        std::move(collect_user_data_options->confirm_callback)
            .Run(&user_data_, nullptr);
      });

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->mutable_contact_details()->set_request_payer_name(true);
  collect_user_data->mutable_contact_details()->add_summary_fields(
      ContactDetailsProto::NAME_FULL);
  collect_user_data->mutable_contact_details()->set_max_number_summary_lines(1);
  collect_user_data->mutable_contact_details()->add_full_fields(
      ContactDetailsProto::NAME_FULL);
  collect_user_data->mutable_contact_details()->set_max_number_full_lines(1);
  *collect_user_data->mutable_contact_details()->add_required_data_piece() =
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FULL);
  collect_user_data->mutable_contact_details()->set_contact_details_name(
      kMemoryLocation);
  collect_user_data->mutable_contact_details()
      ->set_separate_phone_number_section(true);
  collect_user_data->mutable_contact_details()->set_phone_number_section_title(
      "Phone number");
  *collect_user_data->mutable_contact_details()
       ->add_phone_number_required_data_piece() =
      MakeRequiredDataPiece(autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER);
  collect_user_data->mutable_data_source()->set_allow_fallback_on_failure(true);
  collect_user_data->mutable_data_origin_notice()->set_link_text("Link");
  collect_user_data->mutable_data_origin_notice()->set_dialog_title("Title");
  collect_user_data->mutable_data_origin_notice()->set_dialog_text("Text");
  collect_user_data->mutable_data_origin_notice()->set_dialog_button_text(
      "Button");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_THAT(GetUkmUserDataSource(ukm_recorder_),
              ElementsAreArray({ToHumanReadableEntry(
                  source_id_, kUserDataSource,
                  static_cast<int64_t>(
                      Metrics::UserDataSource::
                          FALLBACK_CHROME_AUTOFILL_ON_FAILED_REQUEST))}));
}

TEST_F(CollectUserDataActionTest, FailActionIfFallbackIsNotPossible) {
  ON_CALL(mock_action_delegate_, GetPersonalDataManager)
      .WillByDefault(Return(nullptr));
  ON_CALL(mock_action_delegate_, MustUseBackendData)
      .WillByDefault(Return(true));
  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .WillOnce(RunOnceCallback<1>(false, GetUserDataResponseProto()));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->mutable_contact_details()->set_request_payer_name(true);
  collect_user_data->mutable_contact_details()->set_contact_details_name(
      kMemoryLocation);
  collect_user_data->mutable_data_source()->set_allow_fallback_on_failure(true);

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              USER_DATA_REQUEST_FAILED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, FailActionIfReloadFails) {
  ON_CALL(mock_action_delegate_, GetPersonalDataManager)
      .WillByDefault(Return(nullptr));
  ON_CALL(mock_action_delegate_, MustUseBackendData)
      .WillByDefault(Return(false));
  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .WillOnce(RunOnceCallback<1>(true, GetUserDataResponseProto()))
      .WillOnce(RunOnceCallback<1>(false, GetUserDataResponseProto()));
  EXPECT_CALL(mock_action_delegate_, CollectUserData(_))
      .WillOnce([&](CollectUserDataOptions* collect_user_data_options) {
        std::move(collect_user_data_options->reload_data_callback)
            .Run(UserDataEventField::NONE, &user_data_);
      });

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->mutable_contact_details()->set_request_payer_name(true);
  collect_user_data->mutable_contact_details()->set_contact_details_name(
      kMemoryLocation);
  collect_user_data->mutable_data_source()->set_allow_fallback_on_failure(true);

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              USER_DATA_REQUEST_FAILED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

}  // namespace autofill_assistant
