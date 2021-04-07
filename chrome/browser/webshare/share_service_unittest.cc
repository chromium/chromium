// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/guid.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/webshare/share_service_impl.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "url/gurl.h"

using blink::mojom::ShareError;

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/webshare/chromeos/sharesheet_client.h"
#endif
#if defined(OS_MAC)
#include "chrome/browser/webshare/mac/sharing_service_operation.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#endif
#if defined(OS_WIN)
#include "chrome/browser/webshare/win/scoped_share_operation_fake_components.h"
#endif

class ShareServiceUnitTest : public ChromeRenderViewHostTestHarness {
 public:
  ShareServiceUnitTest() {
    feature_list_.InitAndEnableFeature(features::kWebShare);
  }
  ~ShareServiceUnitTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    share_service_ = std::make_unique<ShareServiceImpl>(*main_rfh());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    webshare::SharesheetClient::SetSharesheetCallbackForTesting(
        base::BindRepeating(&ShareServiceUnitTest::AcceptShareRequest));
#endif
#if defined(OS_MAC)
    webshare::SharingServiceOperation::SetSharePickerCallbackForTesting(
        base::BindRepeating(&ShareServiceUnitTest::AcceptShareRequest));
#endif
#if defined(OS_WIN)
    if (!IsSupportedEnvironment())
      return;

    ASSERT_NO_FATAL_FAILURE(scoped_fake_components_.SetUp());
#endif
  }

#if defined(OS_WIN)
  bool IsSupportedEnvironment() {
    return webshare::ScopedShareOperationFakeComponents::
        IsSupportedEnvironment();
  }
#endif

  ShareError ShareGeneratedFileData(const std::string& extension,
                                    const std::string& content_type,
                                    unsigned file_length = 100,
                                    unsigned file_count = 1) {
    const std::string kTitle;
    const std::string kText;
    const GURL kUrl;
    std::vector<blink::mojom::SharedFilePtr> files;
    files.reserve(file_count);
    for (unsigned index = 0; index < file_count; ++index) {
      const std::string name =
          base::StringPrintf("share%d%s", index, extension.c_str());
      files.push_back(CreateSharedFile(name, content_type, file_length));
    }

    ShareError result;
    base::RunLoop run_loop;
    share_service_->Share(
        kTitle, kText, kUrl, std::move(files),
        base::BindLambdaForTesting([&result, &run_loop](ShareError error) {
          result = error;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

 private:
  blink::mojom::SharedFilePtr CreateSharedFile(const std::string& name,
                                               const std::string& content_type,
                                               unsigned file_length) {
    const std::string uuid = base::GenerateGUID();

    auto blob = blink::mojom::SerializedBlob::New();
    blob->uuid = uuid;
    blob->content_type = content_type;

    base::RunLoop run_loop;
    auto blob_context_getter =
        content::BrowserContext::GetBlobStorageContext(browser_context());
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindLambdaForTesting(
            [&blob_context_getter, &blob, &uuid, &content_type, file_length]() {
              storage::BlobImpl::Create(
                  blob_context_getter.Run()->AddFinishedBlob(
                      CreateBuilder(uuid, content_type, file_length)),
                  blob->blob.InitWithNewPipeAndPassReceiver());
            }),
        base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));

    run_loop.Run();
    return blink::mojom::SharedFile::New(name, std::move(blob));
  }

  static std::unique_ptr<storage::BlobDataBuilder> CreateBuilder(
      const std::string& uuid,
      const std::string& content_type,
      unsigned file_length) {
    auto builder = std::make_unique<storage::BlobDataBuilder>(uuid);
    builder->set_content_type(content_type);
    const std::string contents(file_length, '*');
    builder->AppendData(contents);
    return builder;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  static void AcceptShareRequest(
      content::WebContents* web_contents,
      const std::vector<base::FilePath>& file_paths,
      const std::vector<std::string>& content_types,
      const std::string& text,
      const std::string& title,
      sharesheet::DeliveredCallback delivered_callback) {
    std::move(delivered_callback).Run(sharesheet::SharesheetResult::kSuccess);
  }
#endif

#if defined(OS_MAC)
  static void AcceptShareRequest(
      content::WebContents* web_contents,
      const std::vector<base::FilePath>& file_paths,
      const std::string& text,
      const std::string& title,
      const GURL& url,
      blink::mojom::ShareService::ShareCallback close_callback) {
    std::move(close_callback).Run(blink::mojom::ShareError::OK);
  }
#endif

#if defined(OS_WIN)
  webshare::ScopedShareOperationFakeComponents scoped_fake_components_;
#endif
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ShareServiceImpl> share_service_;
};

TEST_F(ShareServiceUnitTest, FileCount) {
#if defined(OS_WIN)
  if (!IsSupportedEnvironment())
    return;
#endif

  EXPECT_EQ(ShareError::OK, ShareGeneratedFileData(".txt", "text/plain", 1234,
                                                   kMaxSharedFileCount));
  EXPECT_EQ(ShareError::PERMISSION_DENIED,
            ShareGeneratedFileData(".txt", "text/plain", 1234,
                                   kMaxSharedFileCount + 1));
}

TEST_F(ShareServiceUnitTest, DangerousFilename) {
  EXPECT_TRUE(ShareServiceImpl::IsDangerousFilename(""));
  EXPECT_TRUE(ShareServiceImpl::IsDangerousFilename("."));
  EXPECT_TRUE(ShareServiceImpl::IsDangerousFilename("./"));
  EXPECT_TRUE(ShareServiceImpl::IsDangerousFilename(".\\"));

  EXPECT_TRUE(ShareServiceImpl::IsDangerousFilename("a.a"));
  EXPECT_TRUE(ShareServiceImpl::IsDangerousFilename("zzz.zzz"));

  EXPECT_TRUE(ShareServiceImpl::IsDangerousFilename("a/a"));
  EXPECT_TRUE(ShareServiceImpl::IsDangerousFilename("zzz/zzz"));

  EXPECT_FALSE(ShareServiceImpl::IsDangerousFilename("1.XBM"));
  EXPECT_FALSE(ShareServiceImpl::IsDangerousFilename("2.bMP"));
  EXPECT_FALSE(ShareServiceImpl::IsDangerousFilename("3.Flac"));
  EXPECT_FALSE(ShareServiceImpl::IsDangerousFilename("4.webM"));
}

TEST_F(ShareServiceUnitTest, DangerousMimeType) {
  EXPECT_TRUE(ShareServiceImpl::IsDangerousMimeType(""));
  EXPECT_TRUE(ShareServiceImpl::IsDangerousMimeType("/"));

  EXPECT_TRUE(ShareServiceImpl::IsDangerousMimeType("a/a"));
  EXPECT_TRUE(ShareServiceImpl::IsDangerousMimeType("zzz/zzz"));

  EXPECT_TRUE(ShareServiceImpl::IsDangerousMimeType("audio/Flac"));
  EXPECT_TRUE(ShareServiceImpl::IsDangerousMimeType("Video/webm"));

  EXPECT_FALSE(ShareServiceImpl::IsDangerousMimeType("audio/mp3"));
  EXPECT_FALSE(ShareServiceImpl::IsDangerousMimeType("audio/mpeg"));
}

TEST_F(ShareServiceUnitTest, Multimedia) {
#if defined(OS_WIN)
  if (!IsSupportedEnvironment())
    return;
#endif

  EXPECT_EQ(ShareError::OK, ShareGeneratedFileData(".bmp", "image/bmp"));
  EXPECT_EQ(ShareError::OK, ShareGeneratedFileData(".xbm", "image/x-xbitmap"));
  EXPECT_EQ(ShareError::OK, ShareGeneratedFileData(".flac", "audio/flac"));
  EXPECT_EQ(ShareError::OK, ShareGeneratedFileData(".webm", "video/webm"));
}

TEST_F(ShareServiceUnitTest, PortableDocumentFormat) {
#if defined(OS_WIN)
  if (!IsSupportedEnvironment())
    return;
#endif

  // TODO(crbug.com/1006055): Support sharing of pdf files.
  // The URL will be checked using Safe Browsing.
  EXPECT_EQ(ShareError::PERMISSION_DENIED,
            ShareGeneratedFileData(".pdf", "application/pdf"));
}

#if defined(OS_WIN)
TEST_F(ShareServiceUnitTest, ReservedNames) {
  EXPECT_TRUE(ShareServiceImpl::IsDangerousFilename("CON"));
  EXPECT_TRUE(ShareServiceImpl::IsDangerousFilename("PRN"));
  EXPECT_TRUE(ShareServiceImpl::IsDangerousFilename("AUX"));
  EXPECT_TRUE(ShareServiceImpl::IsDangerousFilename("NUL"));
  EXPECT_TRUE(ShareServiceImpl::IsDangerousFilename("COM1"));
  EXPECT_TRUE(ShareServiceImpl::IsDangerousFilename("COM9"));
  EXPECT_TRUE(ShareServiceImpl::IsDangerousFilename("LPT1"));
  EXPECT_TRUE(ShareServiceImpl::IsDangerousFilename("LPT9"));
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// On Chrome OS, like Android, we prevent sharing of Android applications.
TEST_F(ShareServiceUnitTest, AndroidPackage) {
  EXPECT_EQ(ShareError::PERMISSION_DENIED,
            ShareGeneratedFileData(".apk", "text/plain"));
  EXPECT_EQ(ShareError::PERMISSION_DENIED,
            ShareGeneratedFileData(".dex", "text/plain"));
  EXPECT_EQ(ShareError::PERMISSION_DENIED,
            ShareGeneratedFileData(".txt", "vnd.android.package-archive"));
}

TEST_F(ShareServiceUnitTest, TotalBytes) {
  EXPECT_EQ(ShareError::OK,
            ShareGeneratedFileData(".txt", "text/plain",
                                   kMaxSharedFileBytes / kMaxSharedFileCount,
                                   kMaxSharedFileCount));
  EXPECT_EQ(
      ShareError::PERMISSION_DENIED,
      ShareGeneratedFileData(".txt", "text/plain",
                             (kMaxSharedFileBytes / kMaxSharedFileCount) + 1,
                             kMaxSharedFileCount));
}

TEST_F(ShareServiceUnitTest, FileBytes) {
  EXPECT_EQ(ShareError::OK,
            ShareGeneratedFileData(".txt", "text/plain", kMaxSharedFileBytes));
  EXPECT_EQ(
      ShareError::PERMISSION_DENIED,
      ShareGeneratedFileData(".txt", "text/plain", kMaxSharedFileBytes + 1));
}
#endif
