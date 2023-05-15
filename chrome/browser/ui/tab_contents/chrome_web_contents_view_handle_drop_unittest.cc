// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_handle_drop.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  void EnableDeepScanning(bool enable) {
    if (enable) {
      static constexpr char kEnabled[] = R"(
          {
              "service_provider": "google",
              "enable": [
                {
                  "url_list": ["*"],
                  "tags": ["dlp"]
                }
              ],
              "block_until_verdict": 1
          })";
      safe_browsing::SetAnalysisConnector(
          profile_->GetPrefs(), enterprise_connectors::FILE_ATTACHED, kEnabled);
      safe_browsing::SetAnalysisConnector(
          profile_->GetPrefs(), enterprise_connectors::BULK_DATA_ENTRY,
          kEnabled);
    } else {
      safe_browsing::ClearAnalysisConnector(
          profile_->GetPrefs(), enterprise_connectors::FILE_ATTACHED);
      safe_browsing::ClearAnalysisConnector(
          profile_->GetPrefs(), enterprise_connectors::BULK_DATA_ENTRY);
    }

    run_loop_ = std::make_unique<base::RunLoop>();

    using FakeDelegate = enterprise_connectors::FakeContentAnalysisDelegate;

    policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));
    auto callback = base::BindLambdaForTesting(
        [this](const std::string& contents, const base::FilePath& path)
            -> enterprise_connectors::ContentAnalysisResponse {
          std::set<std::string> dlp_tag = {"dlp"};
          current_requests_count_++;
          bool scan_succeeds =
              (path.empty() && text_scan_succeeds_) ||
              (!path.empty() && !base::Contains(failing_file_scans_, path));
          enterprise_connectors::ContentAnalysisResponse response =
              scan_succeeds
                  ? FakeDelegate::SuccessfulResponse(std::move(dlp_tag))
                  : FakeDelegate::DlpResponse(
                        enterprise_connectors::ContentAnalysisResponse::Result::
                            SUCCESS,
                        "block_rule",
                        enterprise_connectors::ContentAnalysisResponse::Result::
                            TriggeredRule::BLOCK);
          std::string request_token =
              path.empty() ? "text_request_token" : path.AsUTF8Unsafe();
          response.set_request_token(request_token);
          if (path.empty()) {
            expected_final_actions_[request_token] =
                scan_succeeds ? enterprise_connectors::
                                    ContentAnalysisAcknowledgement::ALLOW
                              : enterprise_connectors::
                                    ContentAnalysisAcknowledgement::BLOCK;
          } else {
            expected_final_actions_[request_token] =
                failing_file_acks_.count(path)
                    ? enterprise_connectors::ContentAnalysisAcknowledgement::
                          BLOCK
                    : enterprise_connectors::ContentAnalysisAcknowledgement::
                          ALLOW;
          }
          return response;
        });
    enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
        base::BindRepeating(
            &enterprise_connectors::FakeContentAnalysisDelegate::Create,
            run_loop_->QuitClosure(), callback, "dm_token"));
    enterprise_connectors::ContentAnalysisDelegate::DisableUIForTesting();
    enterprise_connectors::ContentAnalysisDelegate::
        SetOnAckAllRequestsCallbackForTesting(base::BindOnce(
            &ChromeWebContentsViewDelegateHandleOnPerformDrop::OnAckAllActions,
            base::Unretained(this)));
  }

  // Common code for running the test cases.
  void RunTest(const content::DropData& data,
               bool enable,
               bool successful_text_scan,
               std::set<base::FilePath> successful_file_paths) {
    current_requests_count_ = 0;
    expected_final_actions_.clear();
    EnableDeepScanning(enable);
    SetTextScanSucceeds(successful_text_scan);

    bool called = false;
    HandleOnPerformDrop(
        contents(), data,
        base::BindLambdaForTesting(
            [&data, &successful_text_scan, &successful_file_paths,
             &called](absl::optional<content::DropData> result_data) {
              if (successful_text_scan || !successful_file_paths.empty()) {
                EXPECT_TRUE(result_data.has_value());
                EXPECT_EQ(result_data->filenames.size(),
                          successful_file_paths.size());
                for (const auto& filename : result_data->filenames) {
                  EXPECT_TRUE(successful_file_paths.count(filename.path));
                }
                if (successful_text_scan) {
                  EXPECT_EQ(result_data->url_title, data.url_title);
                  EXPECT_EQ(result_data->text, data.text);
                  EXPECT_EQ(result_data->html, data.html);
                }
              } else {
                EXPECT_FALSE(result_data.has_value());
              }
              called = true;
            }));
    if (enable)
      RunUntilDone();

    EXPECT_TRUE(called);
    ASSERT_EQ(expected_requests_count_, current_requests_count_);
  }

  void SetExpectedRequestsCount(int count) { expected_requests_count_ = count; }

  void SetTextScanSucceeds(bool succeeds) { text_scan_succeeds_ = succeeds; }

  void SetFailingFileScans(std::set<base::FilePath> paths) {
    failing_file_scans_ = std::move(paths);
  }

  void SetFailingFileAcks(std::set<base::FilePath> paths) {
    failing_file_acks_ = std::move(paths);
  }

  void OnAckAllActions(
      const std::map<
          std::string,
          enterprise_connectors::ContentAnalysisAcknowledgement::FinalAction>&
          final_actions) {
    ASSERT_EQ(final_actions, expected_final_actions_);
  }

  // Helpers to get text with sizes relative to the minimum required size of 100
  // bytes for scans to trigger.
  std::string large_text() const { return std::string(100, 'a'); }

  std::string small_text() const { return "random small text"; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<content::WebContents> web_contents_;
  int expected_requests_count_ = 0;
  int current_requests_count_ = 0;
  bool text_scan_succeeds_ = true;
  std::set<base::FilePath> failing_file_scans_;
  std::set<base::FilePath> failing_file_acks_;
  std::map<std::string,
           enterprise_connectors::ContentAnalysisAcknowledgement::FinalAction>
      expected_final_actions_;
};

// When no drop data is specified, HandleOnPerformDrop() should indicate
// the caller can proceed, whether scanning is enabled or not.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, NoData) {
  content::DropData data;

  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/false, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});
}

// Make sure DropData::url_title is handled correctly.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, UrlTitle) {
  content::DropData data;
  data.url_title = base::UTF8ToUTF16(large_text());

  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/false, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});

  SetExpectedRequestsCount(1);
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
          /*successful_file_paths*/ {});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});

  data.url_title = base::UTF8ToUTF16(small_text());
  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});
}

// Make sure DropData::text is handled correctly.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, Text) {
  content::DropData data;
  data.text = base::UTF8ToUTF16(large_text());

  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/false, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});

  SetExpectedRequestsCount(1);
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
          /*successful_file_paths*/ {});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});

  data.text = base::UTF8ToUTF16(small_text());
  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});
}

// Make sure DropData::html is handled correctly.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, Html) {
  content::DropData data;
  data.html = base::UTF8ToUTF16(large_text());

  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/false, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});

  SetExpectedRequestsCount(1);
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
          /*successful_file_paths*/ {});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});

  data.html = base::UTF8ToUTF16(small_text());
  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});
}

// Make sure DropData::filenames is handled correctly.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, Files) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath path_1 = temp_dir.GetPath().AppendASCII("Foo.doc");
  base::FilePath path_2 = temp_dir.GetPath().AppendASCII("Bar.doc");

  base::File file_1(path_1, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  base::File file_2(path_2, base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  ASSERT_TRUE(file_1.IsValid());
  ASSERT_TRUE(file_2.IsValid());

  file_1.WriteAtCurrentPos("foo content", 11);
  file_2.WriteAtCurrentPos("bar content", 11);

  content::DropData data;
  data.filenames.emplace_back(path_1, path_1);
  data.filenames.emplace_back(path_2, path_2);

  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/false, /*successful_text_scan=*/false,
          /*successful_file_paths*/ {path_1, path_2});

  SetExpectedRequestsCount(2);
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
          /*successful_file_paths*/ {path_1, path_2});
  SetFailingFileScans({path_1});
  SetFailingFileAcks({path_1});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
          /*successful_file_paths*/ {path_2});
  SetFailingFileScans({path_2});
  SetFailingFileAcks({path_2});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
          /*successful_file_paths*/ {path_1});
  SetFailingFileScans({path_1, path_2});
  SetFailingFileAcks({path_1, path_2});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
          /*successful_file_paths*/ {});
}

// Make sure DropData::filenames directories are handled correctly.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, Directories) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath folder_1 = temp_dir.GetPath().AppendASCII("folder1");
  ASSERT_TRUE(base::CreateDirectory(folder_1));
  base::FilePath path_1 = folder_1.AppendASCII("Foo.doc");
  base::FilePath path_2 = folder_1.AppendASCII("Bar.doc");
  base::FilePath path_3 = folder_1.AppendASCII("Baz.doc");

  base::FilePath folder_2 = temp_dir.GetPath().AppendASCII("folder2");
  ASSERT_TRUE(base::CreateDirectory(folder_2));
  base::FilePath path_4 = folder_2.AppendASCII("sub1.doc");
  base::FilePath path_5 = folder_2.AppendASCII("sub2.doc");

  for (const auto& path : {path_1, path_2, path_3, path_4, path_5}) {
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(file.IsValid());
    file.WriteAtCurrentPos("foo content", 11);
  }

  content::DropData data;
  data.filenames.emplace_back(folder_1, folder_1);
  data.filenames.emplace_back(path_4, path_4);
  data.filenames.emplace_back(path_5, path_5);

  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/false, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {folder_1, path_4, path_5});

  // There are 5 files total, so every subsequent `RunTest()` call should have 5
  // corresponding requests.
  SetExpectedRequestsCount(5);

  // If any of the files in `folder_1` fail, the entire folder is removed from
  // the final DropData.
  SetFailingFileScans({path_1});
  SetFailingFileAcks({path_1, path_2, path_3});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
          /*successful_file_paths*/ {path_4, path_5});
  SetFailingFileScans({path_2});
  SetFailingFileAcks({path_1, path_2, path_3});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
          /*successful_file_paths*/ {path_4, path_5});
  SetFailingFileScans({path_3});
  SetFailingFileAcks({path_1, path_2, path_3});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
          /*successful_file_paths*/ {path_4, path_5});

  // The files in `folder_2` are individually in `data`, so one failing doesn't
  // prevent the other from being in the final result.
  SetFailingFileScans({path_4});
  SetFailingFileAcks({path_4});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
          /*successful_file_paths*/ {folder_1, path_5});
  SetFailingFileScans({path_5});
  SetFailingFileAcks({path_5});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
          /*successful_file_paths*/ {folder_1, path_4});

  // If any of the files in `folder_1` fail while the last 2 files also fail,
  // then there are no files at all in the final dropped data.
  SetFailingFileScans({path_1, path_4, path_5});
  SetFailingFileAcks({path_1, path_2, path_3, path_4, path_5});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
          /*successful_file_paths*/ {});
  SetFailingFileScans({path_2, path_4, path_5});
  SetFailingFileAcks({path_1, path_2, path_3, path_4, path_5});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
          /*successful_file_paths*/ {});
  SetFailingFileScans({path_3, path_4, path_5});
  SetFailingFileAcks({path_1, path_2, path_3, path_4, path_5});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
          /*successful_file_paths*/ {});
}
