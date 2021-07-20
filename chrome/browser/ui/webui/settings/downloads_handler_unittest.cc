// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/downloads_handler.h"

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/file_system/service_settings.h"
#include "chrome/browser/enterprise/connectors/file_system/test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

namespace ec = enterprise_connectors;
using WebUIDataReceivedPtr = std::unique_ptr<content::TestWebUI::CallData>;

struct DownloadsSettings {
  std::string auto_open_downloads = "";
  bool downloads_connection_policy_enabled = false;
};

void VerifyLinkedAccountInfo(const base::Value* dict) {
  ASSERT_TRUE(dict->is_dict());
  absl::optional<bool> has_account_linked_opt = dict->FindBoolKey("linked");
  ASSERT_TRUE(has_account_linked_opt.has_value());
  const bool has_account_linked = has_account_linked_opt.value();
  ASSERT_TRUE(has_account_linked);

  const std::string* account_name = dict->FindStringKey("account.name");
  const std::string* account_login = dict->FindStringKey("account.login");
  const std::string* folder_link = dict->FindStringKey("folder.link");
  const std::string* folder_name = dict->FindStringKey("folder.name");
  ASSERT_EQ(has_account_linked, account_name != nullptr);
  ASSERT_EQ(has_account_linked, account_login != nullptr);
  ASSERT_EQ(has_account_linked, folder_link != nullptr);
  ASSERT_EQ(has_account_linked, folder_name != nullptr);

  if (has_account_linked) {
    ASSERT_EQ(*account_name, "Jane Doe");
    ASSERT_EQ(*account_login, "janedoe@example.com");
    ASSERT_EQ(*folder_link, "https://example.com/folder/12345");
    ASSERT_EQ(*folder_name, "ChromeDownloads");
  }
}

void VerifyDownloadsConnectionPolicyChangedCallback(
    const std::vector<WebUIDataReceivedPtr>& web_ui_call_data,
    bool policy_enabled) {
  ASSERT_FALSE(web_ui_call_data.empty());
  bool policy_change_received = false;
  bool account_info_received = false;
  for (auto& data : web_ui_call_data) {
    EXPECT_EQ("cr.webUIListenerCallback", data->function_name());
    std::string event;
    ASSERT_TRUE(data->arg1()->GetAsString(&event));
    if (event == "downloads-connection-policy-changed") {
      policy_change_received = true;
      ASSERT_TRUE(data->arg2()->is_bool());
      EXPECT_EQ(data->arg2()->GetBool(), policy_enabled);
    } else if (event == "downloads-connection-link-changed") {
      account_info_received = true;
      VerifyLinkedAccountInfo(data->arg2());
    }
  }
  ASSERT_TRUE(policy_change_received);
  ASSERT_EQ(policy_enabled, account_info_received);
}

void VerifyAutoOpenDownloadsChangedCallback(const WebUIDataReceivedPtr& data) {
  EXPECT_EQ("cr.webUIListenerCallback", data->function_name());
  std::string event;
  ASSERT_TRUE(data->arg1()->GetAsString(&event));
  EXPECT_EQ("auto-open-downloads-changed", event);
  ASSERT_TRUE(data->arg2()->is_bool());
  EXPECT_FALSE(data->arg2()->GetBool());
}

}  // namespace

namespace settings {

class DownloadsHandlerTest : public testing::TestWithParam<DownloadsSettings> {
 public:
  DownloadsHandlerTest()
      : download_manager_(new content::MockDownloadManager()),
        handler_(&profile_) {
    profile_.SetDownloadManagerForTesting(base::WrapUnique(download_manager_));
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
    feature_list_.InitWithFeatures({ec::kFileSystemConnectorEnabled}, {});
    SetDownloadsConnectionPolicy(param.downloads_connection_policy_enabled);
    size_t expected_inlt_callback_count = 2u;
    // SendDownloadsConnectionInfoToJavascript().
    expected_inlt_callback_count += param.downloads_connection_policy_enabled;

    base::ListValue args;
    handler()->HandleInitialize(&args);

    EXPECT_TRUE(handler()->IsJavascriptAllowed());
    ASSERT_EQ(web_ui_call_data().size(), expected_inlt_callback_count);
    VerifyAutoOpenDownloadsChangedCallback();
    VerifyDownloadsConnectionPolicyChangedCallback();

    test_web_ui_.ClearTrackedCalls();
  }

  void TearDown() override {
    service_->SetDownloadManagerDelegateForTesting(nullptr);
    testing::Test::TearDown();
  }

  void SetDownloadsConnectionPolicy(bool enable) {
    profile()->GetPrefs()->Set(
        ec::kSendDownloadToCloudPref,
        enable ? *base::JSONReader::Read(ec::kWildcardSendDownloadToCloudPref)
               : base::ListValue());
    ASSERT_EQ(handler_.IsDownloadsConnectionPolicyEnabled(), enable);
    policy_enabled_ = enable;
  }

  void ToggleDownloadsConnectionPolicy() {
    SetDownloadsConnectionPolicy(!policy_enabled_);
  }

  void VerifyDownloadsConnectionPolicyChangedCallback() const {
    ::VerifyDownloadsConnectionPolicyChangedCallback(web_ui_call_data(),
                                                     policy_enabled_);
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

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestWebUI test_web_ui_;
  TestingProfile profile_;

  DownloadCoreService* service_;
  content::MockDownloadManager* download_manager_;  // Owned by |profile_|.
  ChromeDownloadManagerDelegate* chrome_download_manager_delegate_;

  bool policy_enabled_;
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

TEST_P(DownloadsHandlerTest, DownloadsConnection) {
  // Touch the pref.
  ToggleDownloadsConnectionPolicy();
  VerifyDownloadsConnectionPolicyChangedCallback();
}

const DownloadsSettings test_settings[] = {{"", true}, {"abc", false}};
INSTANTIATE_TEST_SUITE_P(SettingsPage,
                         DownloadsHandlerTest,
                         testing::ValuesIn(test_settings));

}  // namespace settings
