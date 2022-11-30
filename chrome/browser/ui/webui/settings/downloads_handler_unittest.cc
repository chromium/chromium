// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/downloads_handler.h"

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/file_system/account_info_utils.h"
#include "chrome/browser/enterprise/connectors/file_system/service_settings.h"
#include "chrome/browser/enterprise/connectors/file_system/test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

namespace ec = enterprise_connectors;
using testing::NiceMock;
using WebUIDataReceivedPtr = std::unique_ptr<content::TestWebUI::CallData>;

struct DownloadsSettings {
  std::string auto_open_downloads = "";
  bool connection_init_enabled = false;
  bool connection_init_linked_account = false;
};

const char kAccountName[] = "Jane Doe";
const char kAccountLogin[] = "janedoe@downloads_handler_unittest.com";
const char kFolderId[] = "12345";
const char kFolderLinkFormat[] = "https://app.box.com/folder/%s";
const char kFolderName[] = "ChromeDownloads";

void VerifyLinkedAccountInfo(const base::Value* dict,
                             bool expect_linked,
                             std::string expect_account_name,
                             std::string expect_account_login,
                             std::string expect_folder_name,
                             std::string expect_folder_id) {
  ASSERT_TRUE(dict->is_dict());
  absl::optional<bool> has_account_linked_opt = dict->FindBoolKey("linked");
  ASSERT_TRUE(has_account_linked_opt.has_value());
  const bool has_account_linked = has_account_linked_opt.value();
  ASSERT_EQ(has_account_linked, expect_linked) << *dict;

  const std::string* account_name = dict->FindStringPath("account.name");
  const std::string* account_login = dict->FindStringPath("account.login");
  const std::string* folder_link = dict->FindStringPath("folder.link");
  const std::string* folder_name = dict->FindStringPath("folder.name");
  ASSERT_EQ(has_account_linked, account_name != nullptr) << *dict;
  ASSERT_EQ(has_account_linked, account_login != nullptr) << *dict;
  ASSERT_EQ(has_account_linked, folder_link != nullptr) << *dict;
  ASSERT_EQ(has_account_linked, folder_name != nullptr) << *dict;

  if (has_account_linked) {
    ASSERT_EQ(*account_name, expect_account_name);
    ASSERT_EQ(*account_login, expect_account_login);
    ASSERT_EQ(*folder_name, expect_folder_name);
    std::string expect_folder_link =
        base::StringPrintf(kFolderLinkFormat, expect_folder_id.c_str());
    ASSERT_EQ(*folder_link, expect_folder_link);
  }
}

void VerifyDownloadsConnectionPolicyChangedCallback(
    const std::vector<WebUIDataReceivedPtr>& web_ui_call_data,
    bool expect_connection_policy_enabled,
    bool expect_policy_change_received,
    size_t wait_till_nth_account_info,
    bool expect_account_linked,
    std::string expect_account_name,
    std::string expect_account_login,
    std::string expect_folder_name,
    std::string expect_folder_id) {
  ASSERT_FALSE(web_ui_call_data.empty());
  bool policy_change_received = false;
  size_t account_info_received = 0;
  for (auto& data : web_ui_call_data) {
    EXPECT_EQ("cr.webUIListenerCallback", data->function_name());
    ASSERT_TRUE(data->arg1()->is_string());
    const std::string& event = data->arg1()->GetString();
    if (event == "downloads-connection-policy-changed") {
      policy_change_received = true;
      ASSERT_TRUE(data->arg2()->is_bool());
      ASSERT_TRUE(expect_policy_change_received);
      EXPECT_EQ(data->arg2()->GetBool(), expect_connection_policy_enabled);
    } else if (event == "downloads-connection-link-changed") {
      ++account_info_received;
      if (account_info_received >= wait_till_nth_account_info) {
        VerifyLinkedAccountInfo(data->arg2(), expect_account_linked,
                                expect_account_name, expect_account_login,
                                expect_folder_name, expect_folder_id);
      }
    }
  }
  ASSERT_EQ(expect_policy_change_received, policy_change_received);
  ASSERT_GE(account_info_received, wait_till_nth_account_info);
  ASSERT_EQ(expect_connection_policy_enabled, account_info_received > 0);
}

void VerifyAutoOpenDownloadsChangedCallback(const WebUIDataReceivedPtr& data) {
  EXPECT_EQ("cr.webUIListenerCallback", data->function_name());
  ASSERT_TRUE(data->arg1()->is_string());
  EXPECT_EQ("auto-open-downloads-changed", data->arg1()->GetString());
  ASSERT_TRUE(data->arg2()->is_bool());
  EXPECT_FALSE(data->arg2()->GetBool());
}

}  // namespace

namespace settings {

class DownloadsHandlerTest : public testing::TestWithParam<DownloadsSettings> {
 public:
  DownloadsHandlerTest()
      : download_manager_(new NiceMock<content::MockDownloadManager>()),
        handler_(&profile_) {
    profile_.SetDownloadManagerForTesting(
        base::WrapUnique(download_manager_.get()));
    std::unique_ptr<ChromeDownloadManagerDelegate> delegate =
        std::make_unique<ChromeDownloadManagerDelegate>(&profile_);
    chrome_download_manager_delegate_ = delegate.get();
    service_ = DownloadCoreServiceFactory::GetForBrowserContext(&profile_);
    service_->SetDownloadManagerDelegateForTesting(std::move(delegate));

    EXPECT_CALL(*download_manager_, GetBrowserContext())
        .WillRepeatedly(testing::Return(&profile_));
    EXPECT_CALL(*download_manager_, Shutdown());

    handler_.set_web_ui(&test_web_ui_);
  }

  void SetUp() override {
    EXPECT_TRUE(web_ui_call_data().empty());

    const auto& param = GetParam();
    profile()->GetPrefs()->SetString(prefs::kDownloadExtensionsToOpen,
                                     param.auto_open_downloads);

    // Setup the downloads connection feature.
    OSCryptMocker::SetUp();
    feature_list_.InitWithFeatures({ec::kFileSystemConnectorEnabled}, {});
    ASSERT_TRUE(ec::SetFileSystemOAuth2Tokens(
        profile()->GetPrefs(), ec::kFileSystemServiceProviderPrefNameBox,
        "AToken", "RToken"));
    if (param.connection_init_linked_account)
      ASSERT_TRUE(param.connection_init_enabled);
    SetDownloadsConnectionPolicy(param.connection_init_enabled);
    if (param.connection_init_linked_account)
      SetLinkedAccount(kAccountName, kAccountLogin, kFolderName, kFolderId);

    size_t expected_init_callback_count = 2u;
    // SendDownloadsConnectionToJavascript().
    expected_init_callback_count += param.connection_init_enabled;

    base::ListValue args;
    handler()->HandleInitialize(args.GetList());

    EXPECT_TRUE(handler()->IsJavascriptAllowed());
    ASSERT_EQ(web_ui_call_data().size(), expected_init_callback_count);
    VerifyAutoOpenDownloadsChangedCallback();
    VerifyDownloadsConnectionPolicyChangedCallback(
        param.connection_init_linked_account);

    ClearWebUiTrackedCalls();
  }

  void TearDown() override {
    service_->SetDownloadManagerDelegateForTesting(nullptr);
    OSCryptMocker::TearDown();
    testing::Test::TearDown();
  }

  void SetDownloadsConnectionPolicy(bool enable) {
    profile()->GetPrefs()->Set(
        ec::kSendDownloadToCloudPref,
        enable ? *base::JSONReader::Read(ec::kWildcardSendDownloadToCloudPref)
               : base::ListValue());
    ASSERT_EQ(handler_.IsDownloadsConnectionPolicyEnabled(), enable);
    connection_policy_enabled_ = enable;
  }

  void SetLinkedAccount(std::string account_name,
                        std::string account_login,
                        std::string folder_name,
                        std::string folder_id) {
    base::DictionaryValue account_info;
    account_info.SetStringKey("name", account_name);
    account_info.SetStringKey("login", account_login);
    ec::SetFileSystemAccountInfo(profile()->GetPrefs(),
                                 ec::kFileSystemServiceProviderPrefNameBox,
                                 std::move(account_info));
    ec::SetDefaultFolder(profile()->GetPrefs(),
                         ec::kFileSystemServiceProviderPrefNameBox, folder_id,
                         folder_name);
    account_name_ = account_name;
    account_login_ = account_login;
    folder_name_ = folder_name;
    folder_id_ = folder_id;
  }

  void ToggleDownloadsConnectionPolicy() {
    SetDownloadsConnectionPolicy(!connection_policy_enabled());
  }

  void VerifyDownloadsConnectionAccountLinkedUpdateCallback(
      bool has_linked_account,
      size_t wait_till_nth_account_info,
      bool expect_policy_change_received = false) {
    ASSERT_NE(web_ui_call_data().size(), 0u);
    ::VerifyDownloadsConnectionPolicyChangedCallback(
        web_ui_call_data(), connection_policy_enabled(),
        expect_policy_change_received, wait_till_nth_account_info,
        has_linked_account, account_name_, account_login_, folder_name_,
        folder_id_);
  }

  void VerifyDownloadsConnectionPolicyChangedCallback(
      bool has_linked_account = GetParam().connection_init_linked_account) {
    ASSERT_NE(web_ui_call_data().size(), 0u);
    VerifyDownloadsConnectionAccountLinkedUpdateCallback(
        has_linked_account, /* wait_till_nth_account_info = */ 0,
        /* expect_policy_change_received = */ true);
  }

  void VerifyAutoOpenDownloadsChangedCallback() const {
    ASSERT_FALSE(web_ui_call_data().empty());
    ::VerifyAutoOpenDownloadsChangedCallback(web_ui_call_data().back());
  }

  Profile* profile() { return &profile_; }
  DownloadsHandler* handler() { return &handler_; }
  const std::vector<WebUIDataReceivedPtr>& web_ui_call_data() const {
    return test_web_ui_.call_data();
  }
  void ClearWebUiTrackedCalls() {
    test_web_ui_.ClearTrackedCalls();
    ASSERT_EQ(web_ui_call_data().size(), 0u);
  }
  bool connection_policy_enabled() const { return connection_policy_enabled_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestWebUI test_web_ui_;
  TestingProfile profile_;

  raw_ptr<DownloadCoreService> service_;
  raw_ptr<content::MockDownloadManager>
      download_manager_;  // Owned by |profile_|.
  raw_ptr<ChromeDownloadManagerDelegate> chrome_download_manager_delegate_;

  bool connection_policy_enabled_;
  std::string account_name_, account_login_, folder_name_, folder_id_;
  // Experimental flag for downloads connection.
  base::test::ScopedFeatureList feature_list_;

  DownloadsHandler handler_;
};

TEST_P(DownloadsHandlerTest, AutoOpenDownloads) {
  // Touch the pref.
  profile()->GetPrefs()->SetString(prefs::kDownloadExtensionsToOpen, "def");
  EXPECT_EQ(web_ui_call_data().size(), 1u);
  VerifyAutoOpenDownloadsChangedCallback();
}

TEST_P(DownloadsHandlerTest, DownloadsConnectionToggle) {
  // Toggle the feature policy.
  ToggleDownloadsConnectionPolicy();
  VerifyDownloadsConnectionPolicyChangedCallback();
  ASSERT_EQ(connection_policy_enabled(), !GetParam().connection_init_enabled);
}

TEST_P(DownloadsHandlerTest, DownloadsConnectionSendAccountInfo) {
  // Ensure we can enable the feature policy.
  if (!connection_policy_enabled()) {
    SetDownloadsConnectionPolicy(true);
    VerifyDownloadsConnectionPolicyChangedCallback(
        /* has_linked_account = */ GetParam().connection_init_linked_account);
  }
  ASSERT_TRUE(connection_policy_enabled());
  ClearWebUiTrackedCalls();
  // Once feature is enabled via policy, account info updates are sent.
  SetLinkedAccount("John Smith", "johnsmith@example.com", "DifferentFolderName",
                   "24680");
  // Expect number of account updates due to number of prefs being observed. The
  // account infos received before this are mid-update.
  const size_t expect_account_updates_count =
      ec::GetFileSystemConnectorAccountInfoPrefs(
          ec::kFileSystemServiceProviderPrefNameBox)
          .size();
  ASSERT_GE(web_ui_call_data().size(), expect_account_updates_count);
  VerifyDownloadsConnectionAccountLinkedUpdateCallback(
      /* has_linked_account = */ true,
      /* wait_till_nth_account_info = */ expect_account_updates_count,
      /* expect_policy_change_received = */ false);
}

const DownloadsSettings test_settings[] =
    //  .-- .auto_open_downloads
    // |    .-- .connection_init_enabled
    // |   |     .---- .connection_init_linked_account
    // |   |     |     (must be false if `connection_init_enabled` is false)
    {{"", true, true},        // Connection enabled + has linked account
     {"", true, false},       // Connection enabled + no linked account
     {"abc", false, false}};  // Connection disabled + no linked account
INSTANTIATE_TEST_SUITE_P(SettingsPage,
                         DownloadsHandlerTest,
                         testing::ValuesIn(test_settings));

}  // namespace settings
