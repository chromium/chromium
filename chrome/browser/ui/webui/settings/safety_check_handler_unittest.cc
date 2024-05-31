// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_check_handler.h"

#include <optional>
#include <string>
#include <unordered_map>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/test_passwords_private_delegate.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/ui/webui/help/test_version_updater.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/crx_file/id_util.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safety_check/test_update_check_helper.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/chromeos/devicetype_utils.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

// Components for building event strings.
constexpr char kParent[] = "parent";
constexpr char kUpdates[] = "updates";
constexpr char kPasswords[] = "passwords";
constexpr char kSafeBrowsing[] = "safe-browsing";
constexpr char kExtensions[] = "extensions";

namespace {
using Enabled = base::StrongAlias<class EnabledTag, bool>;
using UserCanDisable = base::StrongAlias<class UserCanDisableTag, bool>;

extensions::api::passwords_private::PasswordUiEntry CreateInsecureCredential(
    int id,
    extensions::api::passwords_private::CompromiseType type) {
  extensions::api::passwords_private::PasswordUiEntry entry;
  entry.username = "test" + base::NumberToString(id);
  extensions::api::passwords_private::CompromisedInfo compromise_info;
  compromise_info.compromise_types.push_back(type);
  entry.compromised_info = std::move(compromise_info);
  return entry;
}

class TestingSafetyCheckHandler : public SafetyCheckHandler {
 public:
  using SafetyCheckHandler::AllowJavascript;
  using SafetyCheckHandler::DisallowJavascript;
  using SafetyCheckHandler::set_web_ui;
  using SafetyCheckHandler::SetTimestampDelegateForTesting;
  using SafetyCheckHandler::SetVersionUpdaterForTesting;

  TestingSafetyCheckHandler(
      std::unique_ptr<safety_check::UpdateCheckHelper> update_helper,
      std::unique_ptr<VersionUpdater> version_updater,
      password_manager::BulkLeakCheckService* leak_service,
      extensions::PasswordsPrivateDelegate* passwords_delegate,
      extensions::ExtensionPrefs* extension_prefs,
      extensions::ExtensionServiceInterface* extension_service,
      std::unique_ptr<TimestampDelegate> timestamp_delegate)
      : SafetyCheckHandler(std::move(update_helper),
                           std::move(version_updater),
                           leak_service,
                           passwords_delegate,
                           extension_prefs,
                           extension_service,
                           std::move(timestamp_delegate)) {}
};

class TestDestructionVersionUpdater : public TestVersionUpdater {
 public:
  ~TestDestructionVersionUpdater() override { destructor_invoked_ = true; }

  void CheckForUpdate(StatusCallback callback, PromoteCallback) override {}

  static bool GetDestructorInvoked() { return destructor_invoked_; }

 private:
  static bool destructor_invoked_;
};

class TestTimestampDelegate : public TimestampDelegate {
 public:
  base::Time GetSystemTime() override {
    // 1 second before midnight Dec 31st 2020, so that -(24h-1s) is still on the
    // same day. This test time is hard coded to prevent DST flakiness, see
    // crbug.com/1066576.
    return base::Time::FromSecondsSinceUnixEpoch(1609459199).LocalMidnight() -
           base::Seconds(1);
  }
};

bool TestDestructionVersionUpdater::destructor_invoked_ = false;

class TestPasswordsDelegate : public extensions::TestPasswordsPrivateDelegate {
 public:
  TestPasswordsDelegate() {
    store_->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
    presenter_.Init();
  }

  void TearDown() { store_->ShutdownOnUIThread(); }

  void SetBulkLeakCheckService(
      password_manager::BulkLeakCheckService* leak_service) {
    leak_service_ = leak_service;
  }

  void SetNumLeakedCredentials(int leaked_password_count,
                               int muted_credentials = 0) {
    DCHECK_LE(muted_credentials, leaked_password_count);
    leaked_password_count_ = leaked_password_count;
    muted_leaked_password_count_ = muted_credentials;
  }

  void SetNumPhishedCredentials(int phished_password_count) {
    phished_password_count_ = phished_password_count;
  }

  void SetNumWeakCredentials(int weak_password_count) {
    weak_password_count_ = weak_password_count;
  }

  void SetNumReusedCredentials(int reused_password_count) {
    reused_password_count_ = reused_password_count;
  }

  void SetPasswordCheckState(
      extensions::api::passwords_private::PasswordCheckState state) {
    state_ = state;
  }

  void SetProgress(int done, int total) {
    done_ = done;
    total_ = total;
  }

  void StoreCompromisedPassword() {
    // Compromised credentials can be added only after password form to which
    // they corresponds exists.
    password_manager::PasswordForm form;
    form.signon_realm = std::string("test.com");
    form.url = GURL("test.com");
    // Credentials have to be unique, so the callback is always invoked.
    form.username_value = base::ASCIIToUTF16(
        "test" + base::NumberToString(test_credential_counter_++));
    form.password_value = u"password";
    form.username_element = u"username_element";
    form.password_issues = {
        {password_manager::InsecureType::kLeaked,
         password_manager::InsecurityMetadata(
             base::Time(), password_manager::IsMuted(false),
             password_manager::TriggerBackendNotification(false))}};
    store_->AddLogin(form);
  }

  std::vector<extensions::api::passwords_private::PasswordUiEntry>
  GetInsecureCredentials() override {
    std::vector<extensions::api::passwords_private::PasswordUiEntry> insecure;
    for (int i = 0; i < leaked_password_count_; ++i) {
      insecure.push_back(CreateInsecureCredential(
          i, extensions::api::passwords_private::CompromiseType::kLeaked));
      if (i < muted_leaked_password_count_) {
        insecure[i].compromised_info->is_muted = true;
      }
    }
    for (int i = 0; i < phished_password_count_; ++i) {
      insecure.push_back(CreateInsecureCredential(
          insecure.size(),
          extensions::api::passwords_private::CompromiseType::kPhished));
    }
    for (int i = 0; i < weak_password_count_; ++i) {
      insecure.push_back(CreateInsecureCredential(
          insecure.size(),
          extensions::api::passwords_private::CompromiseType::kWeak));
    }
    for (int i = 0; i < reused_password_count_; ++i) {
      insecure.push_back(CreateInsecureCredential(
          insecure.size(),
          extensions::api::passwords_private::CompromiseType::kReused));
    }
    return insecure;
  }

  extensions::api::passwords_private::PasswordCheckStatus
  GetPasswordCheckStatus() override {
    extensions::api::passwords_private::PasswordCheckStatus status;
    status.state = state_;
    if (total_ != 0) {
      status.already_processed = done_;
      status.remaining_in_queue = total_ - done_;
    }
    return status;
  }

  password_manager::InsecureCredentialsManager* GetInsecureCredentialsManager()
      override {
    return &credentials_manager_;
  }

 private:
  ~TestPasswordsDelegate() override = default;

  raw_ptr<password_manager::BulkLeakCheckService> leak_service_ = nullptr;
  int leaked_password_count_ = 0;
  int muted_leaked_password_count_ = 0;
  int phished_password_count_ = 0;
  int weak_password_count_ = 0;
  int reused_password_count_ = 0;
  int done_ = 0;
  int total_ = 0;
  int test_credential_counter_ = 0;
  extensions::api::passwords_private::PasswordCheckState state_ =
      extensions::api::passwords_private::PasswordCheckState::kIdle;
  scoped_refptr<password_manager::TestPasswordStore> store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>();
  affiliations::FakeAffiliationService affiliation_service_;
  password_manager::SavedPasswordsPresenter presenter_{
      &affiliation_service_, store_, /*account_store=*/nullptr};
  password_manager::InsecureCredentialsManager credentials_manager_{
      &presenter_};
};

class TestSafetyCheckExtensionService : public TestExtensionService {
 public:
  void AddExtensionState(const std::string& extension_id,
                         Enabled enabled,
                         UserCanDisable user_can_disable) {
    state_map_.emplace(extension_id, ExtensionState{enabled.value(),
                                                    user_can_disable.value()});
  }

  bool IsExtensionEnabled(const std::string& extension_id) const override {
    auto it = state_map_.find(extension_id);
    if (it == state_map_.end()) {
      return false;
    }
    return it->second.enabled;
  }

  bool UserCanDisableInstalledExtension(
      const std::string& extension_id) override {
    auto it = state_map_.find(extension_id);
    if (it == state_map_.end()) {
      return false;
    }
    return it->second.user_can_disable;
  }

 private:
  struct ExtensionState {
    bool enabled;
    bool user_can_disable;
  };

  std::unordered_map<std::string, ExtensionState> state_map_;
};

}  // namespace

class SafetyCheckHandlerTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

  // Returns a |base::Value::Dict| for safety check status update that
  // has the specified |component| and |new_state| if it exists; nullptr
  // otherwise.
  const base::Value::Dict* GetSafetyCheckStatusChangedWithDataIfExists(
      const std::string& component,
      int new_state);

  std::string GenerateExtensionId(char char_to_repeat);

  void VerifyDisplayString(const base::Value::Dict* event,
                           const std::u16string& expected);
  void VerifyDisplayString(const base::Value::Dict* event,
                           const std::string& expected);

  // Replaces any instances of browser name (e.g. Google Chrome, Chromium,
  // etc) with "browser" to make sure tests work both on Chromium and
  // Google Chrome.
  void ReplaceBrowserName(std::u16string* s);

 protected:
  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<safety_check::TestUpdateCheckHelper, DanglingUntriaged>
      update_helper_ = nullptr;
  raw_ptr<TestVersionUpdater, DanglingUntriaged> version_updater_ = nullptr;
  std::unique_ptr<password_manager::BulkLeakCheckService> test_leak_service_;
  scoped_refptr<TestPasswordsDelegate> test_passwords_delegate_;
  raw_ptr<extensions::ExtensionPrefs> test_extension_prefs_ = nullptr;
  TestSafetyCheckExtensionService test_extension_service_;
  content::TestWebUI test_web_ui_;
  std::unique_ptr<TestingSafetyCheckHandler> safety_check_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
};

void SafetyCheckHandlerTest::SetUp() {
  TestingProfile::Builder builder;
  profile_ = builder.Build();

  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile_.get()));

  // The unique pointer to a TestVersionUpdater gets moved to
  // SafetyCheckHandler, but a raw pointer is retained here to change its
  // state.
  auto update_helper = std::make_unique<safety_check::TestUpdateCheckHelper>();
  update_helper_ = update_helper.get();
  auto version_updater = std::make_unique<TestVersionUpdater>();
  version_updater_ = version_updater.get();
  test_leak_service_ = std::make_unique<password_manager::BulkLeakCheckService>(
      nullptr, nullptr);
  test_passwords_delegate_ = base::MakeRefCounted<TestPasswordsDelegate>();
  test_passwords_delegate_->SetBulkLeakCheckService(test_leak_service_.get());
  test_web_ui_.set_web_contents(web_contents_.get());
  test_extension_prefs_ = extensions::ExtensionPrefs::Get(profile_.get());
  auto timestamp_delegate = std::make_unique<TestTimestampDelegate>();
  safety_check_ = std::make_unique<TestingSafetyCheckHandler>(
      std::move(update_helper), std::move(version_updater),
      test_leak_service_.get(), test_passwords_delegate_.get(),
      test_extension_prefs_, &test_extension_service_,
      std::move(timestamp_delegate));
  test_web_ui_.ClearTrackedCalls();
  safety_check_->set_web_ui(&test_web_ui_);
  safety_check_->AllowJavascript();

  browser_task_environment_.RunUntilIdle();
}

void SafetyCheckHandlerTest::TearDown() {
  test_passwords_delegate_->TearDown();
  browser_task_environment_.RunUntilIdle();
}

const base::Value::Dict*
SafetyCheckHandlerTest::GetSafetyCheckStatusChangedWithDataIfExists(
    const std::string& component,
    int new_state) {
  // Return the latest update if multiple, so iterate from the end.
  const std::vector<std::unique_ptr<content::TestWebUI::CallData>>& call_data =
      test_web_ui_.call_data();
  for (int i = call_data.size() - 1; i >= 0; --i) {
    const content::TestWebUI::CallData& data = *(call_data[i]);
    if (data.function_name() != "cr.webUIListenerCallback") {
      continue;
    }
    const std::string* event = data.arg1()->GetIfString();
    if (!event || *event != "safety-check-" + component + "-status-changed")
      continue;
    const base::Value::Dict* dictionary = data.arg2()->GetIfDict();
    if (!dictionary) {
      continue;
    }
    std::optional<int> cur_new_state = dictionary->FindInt("newState");
    if (cur_new_state == new_state)
      return dictionary;
  }
  return nullptr;
}

std::string SafetyCheckHandlerTest::GenerateExtensionId(char char_to_repeat) {
  return std::string(crx_file::id_util::kIdSize * 2, char_to_repeat);
}

void SafetyCheckHandlerTest::VerifyDisplayString(
    const base::Value::Dict* event,
    const std::u16string& expected) {
  const std::string* display_ptr = event->FindString("displayString");
  ASSERT_TRUE(display_ptr);
  std::u16string display = base::UTF8ToUTF16(*display_ptr);
  ReplaceBrowserName(&display);
  // Need to also replace any instances of Chrome and Chromium in the
  // expected string due to an edge case on ChromeOS, where a device name
  // is "Chrome", which gets replaced in the display string.
  std::u16string expected_replaced = expected;
  ReplaceBrowserName(&expected_replaced);
  EXPECT_EQ(expected_replaced, display);
}

void SafetyCheckHandlerTest::VerifyDisplayString(const base::Value::Dict* event,
                                                 const std::string& expected) {
  VerifyDisplayString(event, base::ASCIIToUTF16(expected));
}

void SafetyCheckHandlerTest::ReplaceBrowserName(std::u16string* s) {
  base::ReplaceSubstringsAfterOffset(s, 0, u"Google Chrome", u"Browser");
  base::ReplaceSubstringsAfterOffset(s, 0, u"Chrome", u"Browser");
  base::ReplaceSubstringsAfterOffset(s, 0, u"Chromium", u"Browser");
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Checking) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::CHECKING);
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kUpdates, static_cast<int>(SafetyCheckHandler::UpdateStatus::kChecking));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, u"");
  // Checking state should not get recorded.
  histogram_tester_.ExpectTotalCount("Settings.SafetyCheck.UpdatesResult", 0);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Updated) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::UPDATED);
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kUpdates, static_cast<int>(SafetyCheckHandler::UpdateStatus::kUpdated));
  ASSERT_TRUE(event);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::u16string expected =
      u"Your " + ui::GetChromeOSDeviceName() + u" is up to date";
  VerifyDisplayString(event, expected);
#else
  VerifyDisplayString(event, "Browser is up to date");
#endif
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kUpdated, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Updating) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::UPDATING);
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kUpdates, static_cast<int>(SafetyCheckHandler::UpdateStatus::kUpdating));
  ASSERT_TRUE(event);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  VerifyDisplayString(event, "Updating your device");
#else
  VerifyDisplayString(event, "Updating Browser");
#endif
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kUpdating, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Relaunch) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::NEARLY_UPDATED);
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kUpdates, static_cast<int>(SafetyCheckHandler::UpdateStatus::kRelaunch));
  ASSERT_TRUE(event);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  VerifyDisplayString(
      event, "Nearly up to date! Restart your device to finish updating.");
#else
  VerifyDisplayString(event,
                      "Nearly up to date! Relaunch Browser to finish "
                      "updating.");
#endif
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kRelaunch, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Disabled) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::DISABLED);
  safety_check_->PerformSafetyCheck();
  // TODO(crbug.com/40127188): Since the UNKNOWN state is not present in JS in
  // M83, use FAILED_OFFLINE, which uses the same icon.
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kUpdates,
      static_cast<int>(SafetyCheckHandler::UpdateStatus::kFailedOffline));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event, base::UTF16ToUTF8(VersionUI::GetAnnotatedVersionStringForUi()));
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kUnknown, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_DisabledByAdmin) {
  version_updater_->SetReturnedStatus(
      VersionUpdater::Status::DISABLED_BY_ADMIN);
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kUpdates,
      static_cast<int>(SafetyCheckHandler::UpdateStatus::kDisabledByAdmin));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event,
      "Updates are managed by <a target=\"_blank\" "
      "href=\"https://support.google.com/chrome?p=your_administrator\">your "
      "administrator</a>");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kDisabledByAdmin, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_FailedOffline) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::FAILED_OFFLINE);
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kUpdates,
      static_cast<int>(SafetyCheckHandler::UpdateStatus::kFailedOffline));
  ASSERT_TRUE(event);
  VerifyDisplayString(event,
                      "Browser can't check for updates. Try checking your "
                      "internet connection.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kFailedOffline, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Failed_ConnectivityOnline) {
  update_helper_->SetConnectivity(true);
  version_updater_->SetReturnedStatus(VersionUpdater::Status::FAILED);
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kUpdates, static_cast<int>(SafetyCheckHandler::UpdateStatus::kFailed));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event,
      "Browser didn't update, something went wrong. <a target=\"_blank\" "
      "href=\"https://support.google.com/chrome?p=fix_chrome_updates\">Fix "
      "Browser update problems and failed updates.</a>");
  histogram_tester_.ExpectBucketCount("Settings.SafetyCheck.UpdatesResult",
                                      SafetyCheckHandler::UpdateStatus::kFailed,
                                      1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Failed_ConnectivityOffline) {
  update_helper_->SetConnectivity(false);
  version_updater_->SetReturnedStatus(VersionUpdater::Status::FAILED);
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kUpdates,
      static_cast<int>(SafetyCheckHandler::UpdateStatus::kFailedOffline));
  ASSERT_TRUE(event);
  VerifyDisplayString(event,
                      "Browser can't check for updates. Try checking your "
                      "internet connection.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kFailedOffline, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_DestroyedOnJavascriptDisallowed) {
  EXPECT_FALSE(TestDestructionVersionUpdater::GetDestructorInvoked());
  safety_check_->SetVersionUpdaterForTesting(
      std::make_unique<TestDestructionVersionUpdater>());
  safety_check_->PerformSafetyCheck();
  safety_check_->DisallowJavascript();
  EXPECT_TRUE(TestDestructionVersionUpdater::GetDestructorInvoked());
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_UpdateToRollbackVersionDisallowed) {
  version_updater_->SetReturnedStatus(
      VersionUpdater::Status::UPDATE_TO_ROLLBACK_VERSION_DISALLOWED);
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kUpdates, static_cast<int>(SafetyCheckHandler::UpdateStatus::
                                     kUpdateToRollbackVersionDisallowed));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event,
      "You reverted to a previous version of ChromeOS. "
      "To get updates, wait until the next version is available.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kUpdateToRollbackVersionDisallowed, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_EnabledStandard) {
  TestingProfile::FromWebUI(&test_web_ui_)
      ->AsTestingProfile()
      ->GetTestingPrefService()
      ->SetManagedPref(prefs::kSafeBrowsingEnabled,
                       std::make_unique<base::Value>(true));
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kSafeBrowsing,
      static_cast<int>(
          SafetyCheckHandler::SafeBrowsingStatus::kEnabledStandard));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, "Standard Protection is on");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kEnabledStandard, 1);
}

TEST_F(SafetyCheckHandlerTest,
       CheckSafeBrowsing_EnabledStandardAvailableEnhanced) {
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kSafeBrowsing, static_cast<int>(SafetyCheckHandler::SafeBrowsingStatus::
                                          kEnabledStandardAvailableEnhanced));
  ASSERT_TRUE(event);
  VerifyDisplayString(event,
                      "Standard protection is on. For even more security, use "
                      "enhanced protection.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kEnabledStandardAvailableEnhanced,
      1);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_EnabledEnhanced) {
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kSafeBrowsing,
      static_cast<int>(
          SafetyCheckHandler::SafeBrowsingStatus::kEnabledEnhanced));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, "Enhanced Protection is on");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kEnabledEnhanced, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_InconsistentEnhanced) {
  // Tests the corner case of SafeBrowsingEnhanced pref being on, while
  // SafeBrowsing enabled is off. This should be treated as SB off.
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kSafeBrowsing,
      static_cast<int>(SafetyCheckHandler::SafeBrowsingStatus::kDisabled));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event, "Safe Browsing is off. Browser recommends turning it on.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kDisabled, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_Disabled) {
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kSafeBrowsing,
      static_cast<int>(SafetyCheckHandler::SafeBrowsingStatus::kDisabled));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event, "Safe Browsing is off. Browser recommends turning it on.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kDisabled, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_DisabledByAdmin) {
  TestingProfile::FromWebUI(&test_web_ui_)
      ->AsTestingProfile()
      ->GetTestingPrefService()
      ->SetManagedPref(prefs::kSafeBrowsingEnabled,
                       std::make_unique<base::Value>(false));
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kSafeBrowsing,
      static_cast<int>(
          SafetyCheckHandler::SafeBrowsingStatus::kDisabledByAdmin));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event,
      "<a target=\"_blank\" "
      "href=\"https://support.google.com/chrome?p=your_administrator\">Your "
      "administrator</a> has turned off Safe Browsing");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kDisabledByAdmin, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_DisabledByExtension) {
  TestingProfile::FromWebUI(&test_web_ui_)
      ->AsTestingProfile()
      ->GetTestingPrefService()
      ->SetExtensionPref(prefs::kSafeBrowsingEnabled,
                         std::make_unique<base::Value>(false));
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kSafeBrowsing,
      static_cast<int>(
          SafetyCheckHandler::SafeBrowsingStatus::kDisabledByExtension));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, "An extension has turned off Safe Browsing");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kDisabledByExtension, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_ObserverRemovedAfterError) {
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, u"");
  histogram_tester_.ExpectTotalCount("Settings.SafetyCheck.PasswordsResult2",
                                     0);
  // Second, an "offline" state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kNetworkError);
  const base::Value::Dict* event2 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kOffline));
  ASSERT_TRUE(event2);
  VerifyDisplayString(event2,
                      "Browser can't check your passwords. Try checking your "
                      "internet connection.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kOffline, 1);
  // Another error, but since the previous state is terminal, the handler
  // should no longer be observing the BulkLeakCheckService state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kServiceError);
  const base::Value::Dict* event3 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kOffline));
  ASSERT_TRUE(event3);
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kOffline, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_InterruptedAndRefreshed) {
  safety_check_->PerformSafetyCheck();
  // Password check running.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, u"");
  // The check gets interrupted and the page is refreshed.
  safety_check_->DisallowJavascript();
  safety_check_->AllowJavascript();
  // Need to set the |TestVersionUpdater| instance again to prevent
  // |PerformSafetyCheck()| from creating a real |VersionUpdater| instance.
  safety_check_->SetVersionUpdaterForTesting(
      std::make_unique<TestVersionUpdater>());
  // Another run of the safety check.
  safety_check_->PerformSafetyCheck();
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  const base::Value::Dict* event2 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event2);
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kSignedOut);
  const base::Value::Dict* event3 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kSignedOut));
  ASSERT_TRUE(event3);
  VerifyDisplayString(event3,
                      "Browser can't check your passwords because you're not "
                      "signed in");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kSignedOut, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_StartedTwice) {
  safety_check_->PerformSafetyCheck();
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event);
  // Then, a network error.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kNetworkError);
  const base::Value::Dict* event2 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kOffline));
  EXPECT_TRUE(event2);
  VerifyDisplayString(event2,
                      "Browser can't check your passwords. Try checking your "
                      "internet connection.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kOffline, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_ObserverNotifiedTwice) {
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(test_passwords_delegate_->StartPasswordCheckTriggered());
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(
          password_manager::BulkLeakCheckService::State::kServiceError);
  // Another notification about the same state change.
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(
          password_manager::BulkLeakCheckService::State::kServiceError);
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kError));
  ASSERT_TRUE(event);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_Safe) {
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Second, a "safe" state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords, static_cast<int>(SafetyCheckHandler::PasswordsStatus::kSafe));
  EXPECT_TRUE(event);
  VerifyDisplayString(event, "No compromised passwords found");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kSafe, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_StaleSafeThenCompromised) {
  constexpr int kCompromised = 7;
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  test_passwords_delegate_->SetPasswordCheckState(
      extensions::api::passwords_private::PasswordCheckState::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Not a "safe" state, so send an |OnCredentialDone| with is_leaked=true.
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnCredentialDone({u"login", u"password"},
                         password_manager::IsLeaked(true));
  // The service goes idle, but the disk still has a stale "safe" state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  test_passwords_delegate_->SetPasswordCheckState(
      extensions::api::passwords_private::PasswordCheckState::kIdle);
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords, static_cast<int>(SafetyCheckHandler::PasswordsStatus::kSafe));
  EXPECT_TRUE(event);
  // An InsecureCredentialsManager callback fires once the compromised passwords
  // get written to disk.
  test_passwords_delegate_->SetNumLeakedCredentials(kCompromised);
  test_passwords_delegate_->StoreCompromisedPassword();
  browser_task_environment_.RunUntilIdle();
  const base::Value::Dict* event2 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  ASSERT_TRUE(event2);
  VerifyDisplayString(
      event2, base::NumberToString(kCompromised) + " compromised passwords");
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_SafeStateThenMoreEvents) {
  safety_check_->PerformSafetyCheck();
  // Running state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  test_passwords_delegate_->SetPasswordCheckState(
      extensions::api::passwords_private::PasswordCheckState::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));

  // Previous safe state got loaded.
  test_passwords_delegate_->SetNumLeakedCredentials(0);
  test_passwords_delegate_->StoreCompromisedPassword();
  browser_task_environment_.RunUntilIdle();
  // The event should get ignored, since the state is still running.
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords, static_cast<int>(SafetyCheckHandler::PasswordsStatus::kSafe));
  EXPECT_FALSE(event);

  // The check is completed with another safe state.
  test_passwords_delegate_->SetPasswordCheckState(
      extensions::api::passwords_private::PasswordCheckState::kIdle);
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  // This time the safe state should be reflected.
  event = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords, static_cast<int>(SafetyCheckHandler::PasswordsStatus::kSafe));
  EXPECT_TRUE(event);

  // After some time, some compromises were discovered (unrelated to SC).
  constexpr int kCompromised = 7;
  test_passwords_delegate_->SetNumLeakedCredentials(kCompromised);
  test_passwords_delegate_->StoreCompromisedPassword();
  browser_task_environment_.RunUntilIdle();
  // The new event should get ignored, since the safe state was final.
  const base::Value::Dict* event2 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  EXPECT_FALSE(event2);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_OnlyLeakedExist) {
  constexpr int kLeaked = 7;
  test_passwords_delegate_->SetNumLeakedCredentials(kLeaked);
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Compromised passwords found state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::Value::Dict* event2 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  ASSERT_TRUE(event2);
  VerifyDisplayString(event2,
                      base::NumberToString(kLeaked) + " compromised passwords");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kCompromisedExist, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_OnlyPhishedExist) {
  constexpr int kPhished = 7;
  test_passwords_delegate_->SetNumPhishedCredentials(kPhished);
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Compromised passwords found state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::Value::Dict* event2 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  ASSERT_TRUE(event2);
  VerifyDisplayString(
      event2, base::NumberToString(kPhished) + " compromised passwords");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kCompromisedExist, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_LeakedAndPhishedExist) {
  constexpr int kLeaked = 7, kPhished = 7;
  test_passwords_delegate_->SetNumLeakedCredentials(kLeaked);
  test_passwords_delegate_->SetNumPhishedCredentials(kPhished);
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Compromised passwords found state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::Value::Dict* event2 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  ASSERT_TRUE(event2);
  VerifyDisplayString(event2, base::NumberToString(kLeaked + kPhished) +
                                  " compromised passwords");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kCompromisedExist, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_CompromisedAndWeakExist) {
  constexpr int kCompromised = 7;
  constexpr int kWeak = 13;
  test_passwords_delegate_->SetNumLeakedCredentials(kCompromised);
  test_passwords_delegate_->SetNumWeakCredentials(kWeak);
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Compromised passwords found state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::Value::Dict* event2 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  ASSERT_TRUE(event2);
  VerifyDisplayString(
      event2, base::NumberToString(kCompromised) + " compromised passwords, " +
                  base::NumberToString(kWeak) + " weak passwords");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kCompromisedExist, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_CompromisedAndReusedExist) {
  constexpr int kCompromised = 7;
  constexpr int kReused = 13;
  test_passwords_delegate_->SetNumLeakedCredentials(kCompromised);
  test_passwords_delegate_->SetNumReusedCredentials(kReused);
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Compromised passwords found state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::Value::Dict* event2 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  ASSERT_TRUE(event2);
  VerifyDisplayString(
      event2, base::NumberToString(kCompromised) + " compromised passwords, " +
                  base::NumberToString(kReused) + " reused passwords");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kCompromisedExist, 1);
}

TEST_F(SafetyCheckHandlerTest,
       CheckPasswords_CompromisedAndWeakAndReusedExist) {
  constexpr int kCompromised = 7;
  constexpr int kWeak = 13;
  constexpr int kReused = 6;
  test_passwords_delegate_->SetNumLeakedCredentials(kCompromised);
  test_passwords_delegate_->SetNumWeakCredentials(kWeak);
  test_passwords_delegate_->SetNumReusedCredentials(kReused);
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Compromised passwords found state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::Value::Dict* event2 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  ASSERT_TRUE(event2);
  VerifyDisplayString(
      event2, base::NumberToString(kCompromised) + " compromised passwords, " +
                  base::NumberToString(kWeak) + " weak passwords, " +
                  base::NumberToString(kReused) + " reused passwords");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kCompromisedExist, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_WeakAndReusedExist) {
  constexpr int kWeak = 13;
  constexpr int kReused = 6;
  test_passwords_delegate_->SetNumWeakCredentials(kWeak);
  test_passwords_delegate_->SetNumReusedCredentials(kReused);
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Compromised passwords found state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::Value::Dict* event2 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(
          SafetyCheckHandler::PasswordsStatus::kWeakPasswordsExist));
  ASSERT_TRUE(event2);
  VerifyDisplayString(event2,
                      base::NumberToString(kWeak) + " weak passwords, " +
                          base::NumberToString(kReused) + " reused passwords");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kWeakPasswordsExist, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_OnlyWeakExist) {
  constexpr int kWeak = 13;
  test_passwords_delegate_->SetNumWeakCredentials(kWeak);
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Compromised passwords found state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::Value::Dict* event2 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(
          SafetyCheckHandler::PasswordsStatus::kWeakPasswordsExist));
  ASSERT_TRUE(event2);
  VerifyDisplayString(event2, base::NumberToString(kWeak) + " weak passwords");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kWeakPasswordsExist, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_OnlyReusedExist) {
  constexpr int kReused = 13;
  test_passwords_delegate_->SetNumReusedCredentials(kReused);
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Compromised passwords found state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(
          SafetyCheckHandler::PasswordsStatus::kReusedPasswordsExist));
  ASSERT_TRUE(event);
  VerifyDisplayString(event,
                      base::NumberToString(kReused) + " reused passwords");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kReusedPasswordsExist, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_Error) {
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(test_passwords_delegate_->StartPasswordCheckTriggered());
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(
          password_manager::BulkLeakCheckService::State::kServiceError);
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kError));
  ASSERT_TRUE(event);
  VerifyDisplayString(event,
                      "Browser can't check your passwords. Try again "
                      "later.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kError, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_MutedCompromisedExist) {
  constexpr int kCompromised = 7;
  constexpr int kMuted = 3;
  test_passwords_delegate_->SetNumLeakedCredentials(kCompromised, kMuted);
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Compromised passwords found state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::Value::Dict* event2 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  ASSERT_TRUE(event2);
  VerifyDisplayString(event2, base::NumberToString(kCompromised - kMuted) +
                                  " compromised passwords");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kCompromisedExist, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_AllMutedCompromisedCredentials) {
  constexpr int kCompromised = 7;
  test_passwords_delegate_->SetNumLeakedCredentials(kCompromised, kCompromised);
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Compromised passwords not found.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords, static_cast<int>(SafetyCheckHandler::PasswordsStatus::kSafe));
  ASSERT_TRUE(event);
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kSafe, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_Error_FutureEventsIgnored) {
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(test_passwords_delegate_->StartPasswordCheckTriggered());
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(
          password_manager::BulkLeakCheckService::State::kServiceError);
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kError));
  ASSERT_TRUE(event);
  VerifyDisplayString(event,
                      "Browser can't check your passwords. Try again "
                      "later.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kError, 1);
  // At some point later, the service discovers compromised passwords and goes
  // idle.
  constexpr int kCompromised = 7;
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(password_manager::BulkLeakCheckService::State::kRunning);
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(password_manager::BulkLeakCheckService::State::kIdle);
  // An InsecureCredentialsManager callback fires once the compromised passwords
  // get written to disk.
  test_passwords_delegate_->SetNumLeakedCredentials(kCompromised);
  test_passwords_delegate_->StoreCompromisedPassword();
  browser_task_environment_.RunUntilIdle();
  const base::Value::Dict* event2 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  // The event for compromised passwords should not exist, since the changes
  // should no longer be observed.
  EXPECT_FALSE(event2);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_FeatureUnavailable) {
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(test_passwords_delegate_->StartPasswordCheckTriggered());
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(
          password_manager::BulkLeakCheckService::State::kTokenRequestFailure);
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(
          SafetyCheckHandler::PasswordsStatus::kFeatureUnavailable));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, "Password check is not available in Chromium");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kFeatureUnavailable, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_RunningOneCompromised) {
  test_passwords_delegate_->SetNumLeakedCredentials(1);
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(test_passwords_delegate_->StartPasswordCheckTriggered());
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(password_manager::BulkLeakCheckService::State::kIdle);
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, "1 compromised password");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kCompromisedExist, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_NoPasswords) {
  test_passwords_delegate_->ClearSavedPasswordsList();
  test_passwords_delegate_->SetStartPasswordCheckState(
      password_manager::BulkLeakCheckService::State::kIdle);
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kNoPasswords));
  EXPECT_TRUE(event);
  VerifyDisplayString(event,
                      "No saved passwords. Chrome can check your passwords "
                      "when you save them.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult2",
      SafetyCheckHandler::PasswordsStatus::kNoPasswords, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_Progress) {
  auto credential = password_manager::LeakCheckCredential(u"test", u"test");
  auto is_leaked = password_manager::IsLeaked(false);
  safety_check_->PerformSafetyCheck();
  test_passwords_delegate_->SetPasswordCheckState(
      extensions::api::passwords_private::PasswordCheckState::kRunning);
  test_passwords_delegate_->SetProgress(1, 3);
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnCredentialDone(credential, is_leaked);
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  EXPECT_TRUE(event);
  VerifyDisplayString(event, u"Checking passwords (1 of 3)…");

  test_passwords_delegate_->SetProgress(2, 3);
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnCredentialDone(credential, is_leaked);
  const base::Value::Dict* event2 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  EXPECT_TRUE(event2);
  VerifyDisplayString(event2, u"Checking passwords (2 of 3)…");

  // Final update comes after status change, so no new progress message should
  // be present.
  test_passwords_delegate_->SetPasswordCheckState(
      extensions::api::passwords_private::PasswordCheckState::kIdle);
  test_passwords_delegate_->SetProgress(3, 3);
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnCredentialDone(credential, is_leaked);
  const base::Value::Dict* event3 = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  EXPECT_TRUE(event3);
  // Still 2/3 event.
  VerifyDisplayString(event3, u"Checking passwords (2 of 3)…");
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_NoExtensions) {
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kExtensions,
      static_cast<int>(
          SafetyCheckHandler::ExtensionsStatus::kNoneBlocklisted)));
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_NoneBlocklisted) {
  std::string extension_id = GenerateExtensionId('a');
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(extension_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  extensions::blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      extension_id, extensions::BitMapBlocklistState::NOT_BLOCKLISTED,
      test_extension_prefs_);
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kExtensions,
      static_cast<int>(SafetyCheckHandler::ExtensionsStatus::kNoneBlocklisted));
  EXPECT_TRUE(event);
  VerifyDisplayString(event,
                      "You're protected from potentially harmful extensions");
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_BlocklistedAllDisabled) {
  std::string extension_id = GenerateExtensionId('a');
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test0").SetID(extension_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::DISABLED,
      syncer::StringOrdinal(), "");
  extensions::blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      extension_id, extensions::BitMapBlocklistState::BLOCKLISTED_MALWARE,
      test_extension_prefs_);
  test_extension_service_.AddExtensionState(extension_id, Enabled(false),
                                            UserCanDisable(false));
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kExtensions,
      static_cast<int>(
          SafetyCheckHandler::ExtensionsStatus::kBlocklistedAllDisabled));
  EXPECT_TRUE(event);
  VerifyDisplayString(
      event, "1 potentially harmful extension is off. You can also remove it.");
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_BlocklistedReenabledAllByUser) {
  std::string extension_id = GenerateExtensionId('a');
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test0").SetID(extension_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  extensions::blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      extension_id,
      extensions::BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      test_extension_prefs_);
  test_extension_service_.AddExtensionState(extension_id, Enabled(true),
                                            UserCanDisable(true));
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kExtensions, static_cast<int>(SafetyCheckHandler::ExtensionsStatus::
                                        kBlocklistedReenabledAllByUser));
  EXPECT_TRUE(event);
  VerifyDisplayString(event,
                      "You turned 1 potentially harmful extension back on");
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_BlocklistedReenabledAllByAdmin) {
  std::string extension_id = GenerateExtensionId('a');
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test0").SetID(extension_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  extensions::blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      extension_id,
      extensions::BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      test_extension_prefs_);
  test_extension_service_.AddExtensionState(extension_id, Enabled(true),
                                            UserCanDisable(false));
  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kExtensions, static_cast<int>(SafetyCheckHandler::ExtensionsStatus::
                                        kBlocklistedReenabledAllByAdmin));
  VerifyDisplayString(event,
                      "Your administrator turned 1 potentially harmful "
                      "extension back on");
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_BlocklistedReenabledSomeByUser) {
  std::string extension_id = GenerateExtensionId('a');
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test0").SetID(extension_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  extensions::blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      extension_id,
      extensions::BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      test_extension_prefs_);
  test_extension_service_.AddExtensionState(extension_id, Enabled(true),
                                            UserCanDisable(true));

  std::string extension2_id = GenerateExtensionId('b');
  scoped_refptr<const extensions::Extension> extension2 =
      extensions::ExtensionBuilder("test1").SetID(extension2_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension2.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  extensions::blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      extension2_id,
      extensions::BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      test_extension_prefs_);
  test_extension_service_.AddExtensionState(extension2_id, Enabled(true),
                                            UserCanDisable(false));

  safety_check_->PerformSafetyCheck();
  const base::Value::Dict* event = GetSafetyCheckStatusChangedWithDataIfExists(
      kExtensions, static_cast<int>(SafetyCheckHandler::ExtensionsStatus::
                                        kBlocklistedReenabledSomeByUser));
  EXPECT_TRUE(event);
  VerifyDisplayString(event,
                      "You turned 1 potentially harmful extension back "
                      "on. Your administrator "
                      "turned 1 potentially harmful extension back on.");
}

TEST_F(SafetyCheckHandlerTest, CheckParentRanDisplayString) {
  // 1 second before midnight Dec 31st 2020, so that -(24h-1s) is still on the
  // same day. This test time is hard coded to prevent DST flakiness, see
  // crbug.com/1066576.
  const base::Time system_time =
      base::Time::FromSecondsSinceUnixEpoch(1609459199).LocalMidnight() -
      base::Seconds(1);
  // Display strings for given time deltas in seconds.
  std::vector<std::tuple<std::u16string, int>> tuples{
      std::make_tuple(u"a moment ago", 1),
      std::make_tuple(u"a moment ago", 59),
      std::make_tuple(u"1 minute ago", 60),
      std::make_tuple(u"2 minutes ago", 60 * 2),
      std::make_tuple(u"59 minutes ago", 60 * 60 - 1),
      std::make_tuple(u"1 hour ago", 60 * 60),
      std::make_tuple(u"2 hours ago", 60 * 60 * 2),
      std::make_tuple(u"23 hours ago", 60 * 60 * 23),
      std::make_tuple(u"yesterday", 60 * 60 * 24),
      std::make_tuple(u"yesterday", 60 * 60 * 24 * 2 - 1),
      std::make_tuple(u"2 days ago", 60 * 60 * 24 * 2),
      std::make_tuple(u"2 days ago", 60 * 60 * 24 * 3 - 1),
      std::make_tuple(u"3 days ago", 60 * 60 * 24 * 3),
      std::make_tuple(u"3 days ago", 60 * 60 * 24 * 4 - 1)};
  // Test that above time deltas produce the corresponding display strings.
  for (auto tuple : tuples) {
    const base::Time time = system_time - base::Seconds(std::get<1>(tuple));
    const std::u16string display_string =
        safety_check_->GetStringForParentRan(time, system_time);
    EXPECT_EQ(base::StrCat({u"Safety check ran ", std::get<0>(tuple)}),
              display_string);
  }
}

TEST_F(SafetyCheckHandlerTest, CheckSafetyCheckStartedWebUiEvents) {
  safety_check_->SendSafetyCheckStartedWebUiUpdates();

  // Check that all initial updates ("running" states) are sent.
  const base::Value::Dict* event_parent =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kParent,
          static_cast<int>(SafetyCheckHandler::ParentStatus::kChecking));
  ASSERT_TRUE(event_parent);
  VerifyDisplayString(event_parent, u"Running…");
  const base::Value::Dict* event_updates =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kChecking));
  ASSERT_TRUE(event_updates);
  VerifyDisplayString(event_updates, u"");
  const base::Value::Dict* event_pws =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event_pws);
  VerifyDisplayString(event_pws, u"");
  const base::Value::Dict* event_sb =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(SafetyCheckHandler::SafeBrowsingStatus::kChecking));
  ASSERT_TRUE(event_sb);
  VerifyDisplayString(event_sb, u"");
  const base::Value::Dict* event_extensions =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kExtensions,
          static_cast<int>(SafetyCheckHandler::ExtensionsStatus::kChecking));
  ASSERT_TRUE(event_extensions);
  VerifyDisplayString(event_extensions, u"");
}

TEST_F(SafetyCheckHandlerTest, CheckSafetyCheckCompletedWebUiEvents) {
  // Mock safety check invocation.
  safety_check_->PerformSafetyCheck();

  // Set the password check mock response.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kSignedOut);

  // Check that the parent update is sent after all children checks completed.
  const base::Value::Dict* event_parent =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kParent, static_cast<int>(SafetyCheckHandler::ParentStatus::kAfter));
  ASSERT_TRUE(event_parent);
  VerifyDisplayString(event_parent, u"Safety check ran a moment ago");

  // Check that there is no new parent completion event.
  const base::Value::Dict* event_parent2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kParent, static_cast<int>(SafetyCheckHandler::ParentStatus::kAfter));
  ASSERT_TRUE(event_parent2);
  ASSERT_TRUE(event_parent == event_parent2);
}
