// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_handle_drop.h"

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/logging/logging_settings.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/analysis/clipboard_request_handler.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/connectors/test/fake_clipboard_request_handler.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/files_scan_data.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/file_analysis_request_base.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/file_system/external_mount_points.h"
#endif

class TestDragDropRequestHandler
    : public enterprise_connectors::test::FakeClipboardRequestHandler {
 public:
  static std::unique_ptr<ClipboardRequestHandler> Create(
      enterprise_connectors::test::FakeContentAnalysisDelegate* delegate,
      enterprise_connectors::ContentAnalysisInfo* content_analysis_info,
      enterprise_connectors::BinaryUploadService* upload_service,
      Profile* profile,
      GURL url,
      Type type,
      enterprise_connectors::DeepScanAccessPoint access_point,
      enterprise_connectors::ContentMetaData::CopiedTextSource clipboard_source,
      std::string source_content_area_email,
      std::string content_transfer_method,
      std::string data,
      CompletionCallback callback) {
    auto handler = base::WrapUnique(new TestDragDropRequestHandler(
        content_analysis_info, upload_service, profile, std::move(url), type,
        access_point, std::move(clipboard_source),
        std::move(source_content_area_email),
        std::move(content_transfer_method), std::move(data),
        std::move(callback)));
    handler->delegate_ = delegate;
    return handler;
  }

 protected:
  using FakeClipboardRequestHandler::FakeClipboardRequestHandler;

 private:
  void UploadForDeepScanning(
      std::unique_ptr<enterprise_connectors::ClipboardAnalysisRequest> request)
      override {
    ASSERT_EQ(request->reason(),
              enterprise_connectors::ContentAnalysisRequest::DRAG_AND_DROP);

    enterprise_connectors::BinaryUploadRequest::Data data;
    request->GetRequestData(base::BindLambdaForTesting(
        [&data](enterprise_connectors::ScanRequestUploadResult,
                enterprise_connectors::BinaryUploadRequest::Data data_arg) {
          data = std::move(data_arg);
        }));

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestDragDropRequestHandler::OnContentAnalysisResponse,
                       base::Unretained(this),
                       enterprise_connectors::ScanRequestUploadResult::kSuccess,
                       delegate_->GetStatus(data.contents, base::FilePath())));
  }
};

class MockFileAnalysisRequest
    : public enterprise_connectors::FileAnalysisRequestBase {
 public:
  MockFileAnalysisRequest(
      const enterprise_connectors::AnalysisSettings& analysis_settings,
      base::FilePath path,
      enterprise_connectors::BinaryUploadRequest::ContentAnalysisCallback
          callback)
      : FileAnalysisRequestBase(
            analysis_settings,
            path,
            path.BaseName(),
            "application/octet-stream",
            /*delay_opening_file=*/false,
            std::move(callback),
            base::BindRepeating([]() -> policy::BrowserPolicyConnector* {
              return g_browser_process->browser_policy_connector();
            }),
            content::GetUIThreadTaskRunner({})) {}

  void GetRequestData(DataCallback callback) override {
    Data data;
    data.size = 100;
    data.mime_type = "application/octet-stream";
    data.path = path_;
    data.hash =
        std::string("fake_sha256_hash_value_for_") + file_name_.AsUTF8Unsafe();
    std::move(callback).Run(
        enterprise_connectors::ScanRequestUploadResult::kSuccess,
        std::move(data));
  }

 protected:
  void ProcessZipFile(Data data) override {}
  void ProcessRarFile(Data data) override {}
};

class MockDelegate : public enterprise_connectors::FilesRequestHandler {
 public:
  MockDelegate(
      Profile* profile,
      const std::string& source,
      const std::string& destination,
      const std::vector<base::FilePath>& paths,
      enterprise_connectors::FilesRequestHandler::CompletionCallback callback)
      : enterprise_connectors::FilesRequestHandler(profile,
                                                   source,
                                                   destination,
                                                   paths,
                                                   std::move(callback)) {}

  std::unique_ptr<enterprise_connectors::FileAnalysisRequestBase>
  CreateFileRequest(
      size_t index,
      const enterprise_connectors::AnalysisSettings& settings,
      base::OnceCallback<void(enterprise_connectors::ScanRequestUploadResult,
                              enterprise_connectors::ContentAnalysisResponse)>
          callback,
      base::OnceCallback<
          void(const enterprise_connectors::BinaryUploadRequest&)>
          request_start_callback) override {
    return std::make_unique<MockFileAnalysisRequest>(settings, GetPath(index),
                                                     std::move(callback));
  }

  void SetHandler(
      enterprise_connectors::FilesRequestHandlerBase* handler) override {
    handler_ = handler;
    enterprise_connectors::FilesRequestHandler::SetHandler(handler);
  }

  bool UploadDataImpl() override {
    for (size_t i = 0; i < GetFileCount(); ++i) {
      handler_->PrepareFileRequest(i);
    }
    return true;
  }

 private:
  raw_ptr<enterprise_connectors::FilesRequestHandlerBase> handler_ = nullptr;
};

class DragDropTestContentAnalysisDelegate
    : public enterprise_connectors::test::FakeContentAnalysisDelegate {
 public:
  DragDropTestContentAnalysisDelegate(StatusCallback status_callback,
                                      std::string dm_token,
                                      content::WebContents* web_contents,
                                      Data data,
                                      CompletionCallback callback)
      : enterprise_connectors::test::FakeContentAnalysisDelegate(
            base::DoNothing(),
            std::move(status_callback),
            std::move(dm_token),
            web_contents,
            std::move(data),
            std::move(callback)) {}

  static std::unique_ptr<ContentAnalysisDelegate> Create(
      StatusCallback status_callback,
      std::string dm_token,
      bool use_mock_handler,
      content::WebContents* web_contents,
      Data data,
      CompletionCallback callback) {
    auto ret = std::make_unique<DragDropTestContentAnalysisDelegate>(
        std::move(status_callback), std::move(dm_token), web_contents,
        std::move(data), std::move(callback));
    if (use_mock_handler) {
      enterprise_connectors::FilesRequestHandler::SetFactoryForTesting(
          base::BindRepeating(
              [](enterprise_connectors::test::FakeFilesRequestHandler::
                     FakeFileUploadCallback fake_file_upload_callback,
                 enterprise_connectors::ContentAnalysisInfo*
                     content_analysis_info,
                 enterprise_connectors::BinaryUploadService* upload_service,
                 Profile* profile, GURL url, const std::string& source,
                 const std::string& destination,
                 const std::string& content_transfer_method,
                 enterprise_connectors::DeepScanAccessPoint access_point,
                 const std::vector<base::FilePath>& paths,
                 enterprise_connectors::FilesRequestHandler::CompletionCallback
                     callback)
                  -> std::unique_ptr<
                      enterprise_connectors::FilesRequestHandlerBase> {
                auto delegate = std::make_unique<MockDelegate>(
                    profile, source, destination, paths, std::move(callback));
                return enterprise_connectors::test::FakeFilesRequestHandler::
                    CreateWithDelegate(
                        fake_file_upload_callback, content_analysis_info,
                        upload_service, url, content_transfer_method,
                        access_point, paths, std::move(delegate));
              },
              base::BindRepeating(&DragDropTestContentAnalysisDelegate::
                                      FakeUploadFileForDeepScanning,
                                  base::Unretained(ret.get()))));
    } else {
      enterprise_connectors::FilesRequestHandler::SetFactoryForTesting(
          base::BindRepeating(
              &enterprise_connectors::test::FakeFilesRequestHandler::Create,
              base::BindRepeating(&DragDropTestContentAnalysisDelegate::
                                      FakeUploadFileForDeepScanning,
                                  base::Unretained(ret.get()))));
    }
    enterprise_connectors::ClipboardRequestHandler::SetFactoryForTesting(
        base::BindRepeating(TestDragDropRequestHandler::Create,
                            base::Unretained(ret.get())));
    return ret;
  }

 private:
  void FakeUploadFileForDeepScanning(
      enterprise_connectors::ScanRequestUploadResult result,
      const base::FilePath& path,
      std::unique_ptr<enterprise_connectors::BinaryUploadRequest> request,
      enterprise_connectors::test::FakeFilesRequestHandler::
          FakeFileRequestCallback callback) override {
    ASSERT_EQ(request->reason(),
              enterprise_connectors::ContentAnalysisRequest::DRAG_AND_DROP);

    enterprise_connectors::test::FakeContentAnalysisDelegate::
        FakeUploadFileForDeepScanning(result, path, std::move(request),
                                      std::move(callback));
  }
};

#if BUILDFLAG(IS_CHROMEOS)
class FakeFuseboxDelegate : public fusebox::Server::Delegate {
 public:
  FakeFuseboxDelegate() = default;

  void OnRegisterFSURLPrefix(const std::string& subdir) override {}
  void OnUnregisterFSURLPrefix(const std::string& subdir) override {}
};
#endif

class ChromeWebContentsViewDelegateHandleOnPerformingDrop
    : public testing::TestWithParam</*EnableDlpFileSystemApi_enabled=*/bool> {
 public:
  ChromeWebContentsViewDelegateHandleOnPerformingDrop() {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

 protected:
  bool IsDlpFileSystemApiEnabled() const { return GetParam(); }

  void SetUp() override {
    if (IsDlpFileSystemApiEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          enterprise_data_protection::kEnableDlpFileSystemApi);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          enterprise_data_protection::kEnableDlpFileSystemApi);
    }
#if BUILDFLAG(IS_CHROMEOS)
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        "fake_mount", storage::kFileSystemTypeProvided,
        storage::FileSystemMountOption(),
        base::FilePath(FILE_PATH_LITERAL("/media/archive/fake_mount")));
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        "not_backed_mount", storage::kFileSystemTypeProvided,
        storage::FileSystemMountOption(),
        base::FilePath(FILE_PATH_LITERAL("/media/archive/not_backed_mount")));

    fusebox_server_ =
        std::make_unique<fusebox::Server>(&fake_fusebox_delegate_);
    fusebox_server_->RegisterFSURLPrefix(
        "fake_mount", "filesystem:chrome://file-manager/external/fake_mount",
        /*read_only=*/false);
#endif
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS)
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        "fake_mount");
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        "not_backed_mount");
    fusebox_server_.reset();
#endif
  }

 public:
  content::WebContents* contents() {
    if (!web_contents_) {
      content::WebContents::CreateParams params(profile_);
      web_contents_ = content::WebContents::Create(params);
    }
    return web_contents_.get();
  }

  void EnableDeepScanning(bool enable, bool use_mock_handler = false) {
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
      enterprise_connectors::test::SetAnalysisConnector(
          profile_->GetPrefs(), enterprise_connectors::FILE_ATTACHED, kEnabled);
      enterprise_connectors::test::SetAnalysisConnector(
          profile_->GetPrefs(), enterprise_connectors::BULK_DATA_ENTRY,
          kEnabled);
    } else {
      enterprise_connectors::test::ClearAnalysisConnector(
          profile_->GetPrefs(), enterprise_connectors::FILE_ATTACHED);
      enterprise_connectors::test::ClearAnalysisConnector(
          profile_->GetPrefs(), enterprise_connectors::BULK_DATA_ENTRY);
    }

    using FakeDelegate =
        enterprise_connectors::test::FakeContentAnalysisDelegate;

    policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));
    auto callback = base::BindLambdaForTesting(
        [this](const std::string& contents, const base::FilePath& path)
            -> enterprise_connectors::ContentAnalysisResponse {
          std::set<std::string> dlp_tag = {"dlp"};
          current_requests_count_++;
          bool scan_succeeds =
              (path.empty() && text_scan_succeeds_) ||
              (!path.empty() && !failing_file_scans_.contains(path));
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
        base::BindRepeating(&DragDropTestContentAnalysisDelegate::Create,
                            callback, "dm_token", use_mock_handler));
    enterprise_connectors::ContentAnalysisDelegate::DisableUIForTesting();
    enterprise_connectors::ContentAnalysisDelegate::
        SetOnAckAllRequestsCallbackForTesting(base::BindOnce(
            &ChromeWebContentsViewDelegateHandleOnPerformingDrop::
                OnAckAllActions,
            base::Unretained(this)));
  }

  // Common code for running the test cases.
  void RunTest(const content::DropData& data,
               bool enable,
               bool successful_text_scan,
               std::set<base::FilePath> successful_file_paths,
               std::set<GURL> successful_vfs_urls = {},
               bool use_mock_handler = false) {
    current_requests_count_ = 0;
    expected_final_actions_.clear();
    EnableDeepScanning(enable, use_mock_handler);
    SetTextScanSucceeds(successful_text_scan);

    base::RunLoop run_loop;

    auto quit_closure = run_loop.QuitClosure();
    HandleOnPerformingDrop(
        contents(), data,
        base::BindLambdaForTesting(
            [&data, &successful_text_scan, &successful_file_paths,
             &successful_vfs_urls,
             quit_closure](std::optional<content::DropData> result_data) {
              if (successful_text_scan || !successful_file_paths.empty() ||
                  !successful_vfs_urls.empty()) {
                EXPECT_TRUE(result_data.has_value());
                EXPECT_EQ(result_data->filenames.size(),
                          successful_file_paths.size());
                for (const auto& filename : result_data->filenames) {
                  EXPECT_TRUE(successful_file_paths.count(filename.path));
                }
                EXPECT_EQ(result_data->file_system_files.size(),
                          successful_vfs_urls.size());
                for (const auto& file_system_file :
                     result_data->file_system_files) {
                  EXPECT_TRUE(successful_vfs_urls.count(file_system_file.url));
                }
                if (successful_text_scan) {
                  if (data.url_infos.empty()) {
                    EXPECT_TRUE(result_data->url_infos.empty());
                  } else {
                    ASSERT_FALSE(result_data->url_infos.empty());
                    EXPECT_EQ(result_data->url_infos.front().title,
                              data.url_infos.front().title);
                  }
                  EXPECT_EQ(result_data->text, data.text);
                  EXPECT_EQ(result_data->html, data.html);
                }
              } else {
                EXPECT_FALSE(result_data.has_value());
              }
              quit_closure.Run();
            }));
    run_loop.Run();

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
#if BUILDFLAG(IS_CHROMEOS)
  FakeFuseboxDelegate fake_fusebox_delegate_;
  std::unique_ptr<fusebox::Server> fusebox_server_;
#endif
};

// When no drop data is specified, HandleOnPerformingDrop() should indicate
// the caller can proceed, whether scanning is enabled or not.
TEST_P(ChromeWebContentsViewDelegateHandleOnPerformingDrop, NoData) {
  content::DropData data;

  SetExpectedRequestsCount(0);
  data.document_is_handling_drag = true;
  RunTest(data, /*enable=*/false, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});
}

// When drop data is specified, but document_is_handling_drag is false,
// HandleOnPerformingDrop() should indicate the caller can proceed
// and no scanning is done.
TEST_P(ChromeWebContentsViewDelegateHandleOnPerformingDrop,
       WithData_NoneDocOp) {
  content::DropData data;
  data.text = base::UTF8ToUTF16(large_text());

  SetExpectedRequestsCount(0);
  data.document_is_handling_drag = false;
  RunTest(data, /*enable=*/false, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});
}

// Make sure DropData::url_title is handled correctly.
TEST_P(ChromeWebContentsViewDelegateHandleOnPerformingDrop, UrlTitle) {
  content::DropData data;
  data.document_is_handling_drag = true;
  data.url_infos = {ui::ClipboardUrlInfo(GURL("https://example.com"),
                                         base::UTF8ToUTF16(large_text()))};

  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/false, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});

  SetExpectedRequestsCount(1);
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
          /*successful_file_paths*/ {});
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});

  data.url_infos.front().title = base::UTF8ToUTF16(small_text());
  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/true, /*successful_text_scan=*/true,
          /*successful_file_paths*/ {});
}

// Make sure DropData::text is handled correctly.
TEST_P(ChromeWebContentsViewDelegateHandleOnPerformingDrop, Text) {
  content::DropData data;
  data.document_is_handling_drag = true;
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
TEST_P(ChromeWebContentsViewDelegateHandleOnPerformingDrop, Html) {
  content::DropData data;
  data.document_is_handling_drag = true;
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
TEST_P(ChromeWebContentsViewDelegateHandleOnPerformingDrop, Files) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath path_1 = temp_dir.GetPath().AppendASCII("Foo.doc");
  base::FilePath path_2 = temp_dir.GetPath().AppendASCII("Bar.doc");

  base::File file_1(path_1, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  base::File file_2(path_2, base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  ASSERT_TRUE(file_1.IsValid());
  ASSERT_TRUE(file_2.IsValid());

  file_1.WriteAtCurrentPos(base::byte_span_from_cstring("foo content"));
  file_2.WriteAtCurrentPos(base::byte_span_from_cstring("bar content"));

  content::DropData data;
  data.document_is_handling_drag = true;
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
TEST_P(ChromeWebContentsViewDelegateHandleOnPerformingDrop, Directories) {
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
    file.WriteAtCurrentPos(base::byte_span_from_cstring("foo content"));
  }

  content::DropData data;
  data.document_is_handling_drag = true;
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

#if BUILDFLAG(IS_CHROMEOS)
TEST_P(ChromeWebContentsViewDelegateHandleOnPerformingDrop, VirtualFiles) {
  content::WebContents* web_contents = contents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // Setup fusebox files

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath fake_mount_dir =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("fake_mount"));
  ASSERT_TRUE(base::CreateDirectory(fake_mount_dir));

  base::FilePath resolved_path_1 =
      fake_mount_dir.Append(FILE_PATH_LITERAL("doc1.doc"));
  base::FilePath resolved_path_2 =
      fake_mount_dir.Append(FILE_PATH_LITERAL("doc2.doc"));

  ASSERT_TRUE(base::WriteFile(resolved_path_1, "test content 1"));
  ASSERT_TRUE(base::WriteFile(resolved_path_2, "test content 2"));

  fusebox::Server::OverrideFuseBoxMediaPathForTesting(
      temp_dir.GetPath().AsUTF8Unsafe() + "/");
  base::ScopedClosureRunner reset_media_path(base::BindOnce(
      []() { fusebox::Server::OverrideFuseBoxMediaPathForTesting(""); }));

  storage::FileSystemContext* context =
      profile
          ->GetStoragePartition(
              web_contents->GetPrimaryMainFrame()->GetSiteInstance())
          ->GetFileSystemContext();

  url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  // Create mock virtual file URLs
  base::FilePath virtual_path_1(FILE_PATH_LITERAL("fake_mount/doc1.doc"));
  base::FilePath virtual_path_2(FILE_PATH_LITERAL("fake_mount/doc2.doc"));
  base::FilePath virtual_path_3(FILE_PATH_LITERAL("not_backed_mount/doc3.doc"));

  file_manager::util::FileSystemURLAndHandle url_handle_1 =
      file_manager::util::CreateIsolatedURLFromVirtualPath(*context, origin,
                                                           virtual_path_1);
  file_manager::util::FileSystemURLAndHandle url_handle_2 =
      file_manager::util::CreateIsolatedURLFromVirtualPath(*context, origin,
                                                           virtual_path_2);
  file_manager::util::FileSystemURLAndHandle url_handle_3 =
      file_manager::util::CreateIsolatedURLFromVirtualPath(*context, origin,
                                                           virtual_path_3);

  GURL url_1 = url_handle_1.url.ToGURL();
  GURL url_2 = url_handle_2.url.ToGURL();
  GURL url_3 = url_handle_3.url.ToGURL();

  content::DropData data;
  data.file_system_files.push_back({url_1, 10, std::string()});
  data.file_system_files.push_back({url_2, 20, std::string()});
  data.file_system_files.push_back({url_3, 30, std::string()});
  data.document_is_handling_drag = true;

  if (IsDlpFileSystemApiEnabled()) {
    // Scenario 1: DLP Disabled -> All files allowed (including unscanned VFS 3)
    SetExpectedRequestsCount(0);
    RunTest(data, /*enable=*/false, /*successful_text_scan=*/false,
            /*successful_file_paths=*/{},
            /*successful_vfs_urls=*/{url_1, url_2, url_3},
            /*use_mock_handler=*/true);

    // Scenario 2: DLP Enabled, all allowed -> All files allowed (since no
    // violations)
    SetExpectedRequestsCount(2);
    RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
            /*successful_file_paths=*/{},
            /*successful_vfs_urls=*/{url_1, url_2, url_3},
            /*use_mock_handler=*/true);

    // Scenario 3: DLP Enabled, selective block -> url_1 (resolved_path_1)
    // blocks, others allowed. Note: url_3 is not backed by Fusebox, so it is
    // allowed because it was never scanned.
    SetExpectedRequestsCount(2);
    SetFailingFileScans({resolved_path_1});
    SetFailingFileAcks({resolved_path_1});
    RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
            /*successful_file_paths=*/{},
            /*successful_vfs_urls=*/{url_2, url_3},
            /*use_mock_handler=*/true);

    // Scenario 4: DLP Enabled, all scannable files blocked -> Allow
    // non-scannable Note: Since all files that *could* be scanned (url_1,
    // url_2) are blocked, but url_3 is not scannable, the drop is not aborted
    // and url_3 is allowed.
    SetExpectedRequestsCount(2);
    SetFailingFileScans({resolved_path_1, resolved_path_2});
    SetFailingFileAcks({resolved_path_1, resolved_path_2});
    RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
            /*successful_file_paths=*/{},
            /*successful_vfs_urls=*/{url_3},
            /*use_mock_handler=*/true);
  } else {
    // When the feature is disabled, virtual files are allowed by default
    // without scanning, regardless of DLP policy settings.
    SetExpectedRequestsCount(0);
    RunTest(data, /*enable=*/false, /*successful_text_scan=*/false,
            /*successful_file_paths=*/{},
            /*successful_vfs_urls=*/{url_1, url_2, url_3},
            /*use_mock_handler=*/true);

    RunTest(data, /*enable=*/true, /*successful_text_scan=*/false,
            /*successful_file_paths=*/{},
            /*successful_vfs_urls=*/{url_1, url_2, url_3},
            /*use_mock_handler=*/true);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeWebContentsViewDelegateHandleOnPerformingDrop,
                         /*EnableDlpFileSystemApi_enabled=*/testing::Bool());
