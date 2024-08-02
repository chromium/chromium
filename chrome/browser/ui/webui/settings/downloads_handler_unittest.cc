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
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings {

class DownloadsHandlerTest : public testing::Test {
 public:
  DownloadsHandlerTest()
      : download_manager_(new content::MockDownloadManager()),
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

  void VerifyAutoOpenDownloadsChangedCallback() {
    EXPECT_EQ(1u, test_web_ui_.call_data().size());

    auto& data = *(test_web_ui_.call_data().back());
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ("auto-open-downloads-changed", data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->is_bool());
    EXPECT_FALSE(data.arg2()->GetBool());
  }

  void SetUp() override {
    EXPECT_TRUE(test_web_ui_.call_data().empty());
    handler()->HandleInitialize(base::Value::List());
    EXPECT_TRUE(handler()->IsJavascriptAllowed());
    VerifyAutoOpenDownloadsChangedCallback();
    test_web_ui_.ClearTrackedCalls();
  }

  void TearDown() override {
    service_->SetDownloadManagerDelegateForTesting(nullptr);
    testing::Test::TearDown();
  }

  Profile* profile() { return &profile_; }
  DownloadsHandler* handler() { return &handler_; }
  bool connection_policy_enabled() const { return connection_policy_enabled_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestWebUI test_web_ui_;
  TestingProfile profile_;

  raw_ptr<DownloadCoreService> service_;
  raw_ptr<content::MockDownloadManager>
      download_manager_;  // Owned by |profile_|.
  raw_ptr<ChromeDownloadManagerDelegate, DanglingUntriaged>
      chrome_download_manager_delegate_;

  bool connection_policy_enabled_;
  std::string account_name_, account_login_, folder_name_, folder_id_;
  // Experimental flag for downloads connection.
  base::test::ScopedFeatureList feature_list_;

  DownloadsHandler handler_;
};

TEST_F(DownloadsHandlerTest, AutoOpenDownloads) {
  // Touch the pref.
  profile()->GetPrefs()->SetString(prefs::kDownloadExtensionsToOpen, "def");
  VerifyAutoOpenDownloadsChangedCallback();
}

}  // namespace settings
