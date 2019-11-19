// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/fake_deep_scanning_dialog_delegate.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_handle_drop.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeWebContentsViewDelegateHandleOnPerformDrop : public testing::Test {
 public:
  ChromeWebContentsViewDelegateHandleOnPerformDrop() {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  void RunUntilDone() { run_loop_->Run(); }

  content::WebContents* contents() {
    if (!web_contents_) {
      content::WebContents::CreateParams params(profile_);
      web_contents_ = content::WebContents::Create(params);
    }
    return web_contents_.get();
  }

  void EnableFeature(const base::Feature& feature) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeature(feature);
  }

  void EnableDeepScanning(bool enable, bool scan_succeeds) {
    SetScanPolicies(enable ? safe_browsing::CHECK_UPLOADS
                           : safe_browsing::CHECK_NONE);
    if (!enable)
      return;

    EnableFeature(safe_browsing::kDeepScanningOfUploads);

    run_loop_.reset(new base::RunLoop());

    using FakeDelegate = safe_browsing::FakeDeepScanningDialogDelegate;
    using Verdict = safe_browsing::DlpDeepScanningVerdict;
    auto callback = base::Bind(
        [](bool scan_succeeds, const base::FilePath&) {
          return scan_succeeds ? FakeDelegate::SuccessfulResponse()
                               : FakeDelegate::DlpResponse(
                                     Verdict::FAILURE, std::string(),
                                     Verdict::TriggeredRule::REPORT_ONLY);
        },
        scan_succeeds);

    safe_browsing::DeepScanningDialogDelegate::SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting("dm_token"));
    safe_browsing::DeepScanningDialogDelegate::SetFactoryForTesting(
        base::BindRepeating(
            &safe_browsing::FakeDeepScanningDialogDelegate::Create,
            run_loop_->QuitClosure(), callback, "dm_token"));
  }

  // Common code for running the test cases.
  void RunTest(const content::DropData& data,
               bool enable,
               bool scan_succeeds = false) {
    EnableDeepScanning(enable, scan_succeeds);

    content::WebContentsViewDelegate::DropCompletionResult result =
        scan_succeeds
            ? content::WebContentsViewDelegate::DropCompletionResult::kContinue
            : content::WebContentsViewDelegate::DropCompletionResult::kAbort;
    bool called = false;
    HandleOnPerformDrop(
        contents(), data,
        base::Bind(
            [](content::WebContentsViewDelegate::DropCompletionResult
                   expected_result,
               bool* called,
               content::WebContentsViewDelegate::DropCompletionResult result) {
              EXPECT_EQ(expected_result, result);
              *called = true;
            },
            result, &called));
    if (enable)
      RunUntilDone();

    EXPECT_TRUE(called);
  }

 private:
  void SetScanPolicies(safe_browsing::CheckContentComplianceValues state) {
    PrefService* pref_service =
        TestingBrowserProcess::GetGlobal()->local_state();
    pref_service->SetInteger(prefs::kCheckContentCompliance, state);
    pref_service->SetInteger(prefs::kDelayDeliveryUntilVerdict,
                             safe_browsing::DELAY_UPLOADS);
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  TestingProfile* profile_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<content::WebContents> web_contents_;
};

// When no drop data is specified, HandleOnPerformDrop() should indicate
// the caller can proceed, whether scanning is enabled or not.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, NoData) {
  content::DropData data;
  RunTest(data, /*enable=*/false, /*scan_succeeds=*/true);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/true);
}

// Make sure DropData::url_title is handled correctly.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, UrlTitle) {
  content::DropData data;
  data.url_title = base::UTF8ToUTF16("title");
  RunTest(data, /*enable=*/false, /*scan_succeeds=*/true);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/false);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/true);
}

// Make sure DropData::text is handled correctly.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, Text) {
  content::DropData data;
  data.text = base::NullableString16(base::UTF8ToUTF16("text"), false);
  RunTest(data, /*enable=*/false, /*scan_succeeds=*/true);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/false);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/true);
}

// Make sure DropData::html is handled correctly.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, Html) {
  content::DropData data;
  data.html = base::NullableString16(base::UTF8ToUTF16("<html></html>"), false);
  RunTest(data, /*enable=*/false, /*scan_succeeds=*/true);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/false);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/true);
}

// Make sure DropData::file_contents is handled correctly.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, FileContents) {
  content::DropData data;
  data.file_contents = "file_contents";
  RunTest(data, /*enable=*/false, /*scan_succeeds=*/true);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/false);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/true);
}

// Make sure DropData::filenames is handled correctly.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, Files) {
  content::DropData data;
  data.filenames.emplace_back(base::FilePath(FILE_PATH_LITERAL("C:\\Foo.doc")),
                              base::FilePath(FILE_PATH_LITERAL("Foo.doc")));
  data.filenames.emplace_back(base::FilePath(FILE_PATH_LITERAL("C:\\Bar.doc")),
                              base::FilePath(FILE_PATH_LITERAL("Bar.doc")));
  RunTest(data, /*enable=*/false, /*scan_succeeds=*/true);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/false);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/true);
}
