// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/safe_base_name.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "build/build_config.h"
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

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/webshare/chromeos/sharesheet_client.h"
#include "chromeos/components/sharesheet/constants.h"
#endif
#if BUILDFLAG(IS_MAC)
#include "chrome/browser/webshare/mac/sharing_service_operation.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#endif
#if BUILDFLAG(IS_WIN)
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
    ShareServiceImpl::Create(
        main_rfh(), share_service_remote_.BindNewPipeAndPassReceiver());

#if BUILDFLAG(IS_CHROMEOS)
    webshare::SharesheetClient::SetSharesheetCallbackForTesting(
        base::BindRepeating(&ShareServiceUnitTest::AcceptShareRequest));
#endif
#if BUILDFLAG(IS_MAC)
    webshare::SharingServiceOperation::SetSharePickerCallbackForTesting(
        base::BindRepeating(&ShareServiceUnitTest::AcceptShareRequest));
#endif
#if BUILDFLAG(IS_WIN)
    ASSERT_NO_FATAL_FAILURE(scoped_fake_components_.SetUp());
#endif
  }

  ShareError ShareGeneratedFileData(std::string_view extension,
                                    const std::string& content_type,
                                    unsigned file_length = 100,
                                    unsigned file_count = 1) {
    const std::string kTitle;
    const std::string kText;
    const GURL kUrl;
    std::vector<blink::mojom::SharedFilePtr> files;
    files.reserve(file_count);
    for (unsigned index = 0; index < file_count; ++index) {
      files.push_back(CreateSharedFile(
          base::FilePath::FromASCII(
              base::StrCat({"share", base::NumberToString(index), extension})),
          content_type, file_length));
    }

    ShareError result;
    base::RunLoop run_loop;
    share_service_remote_->Share(
        kTitle, kText, kUrl, std::move(files),
        base::BindLambdaForTesting([&result, &run_loop](ShareError error) {
          result = error;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  bool IsDangerousFilename(base::FilePath::StringPieceType path) {
    return ShareServiceImpl::IsDangerousFilename(base::FilePath(path));
  }

 private:
  blink::mojom::SharedFilePtr CreateSharedFile(const base::FilePath& name,
                                               const std::string& content_type,
                                               unsigned file_length) {
    const std::string uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();

    auto blob = blink::mojom::SerializedBlob::New();
    blob->uuid = uuid;
    blob->content_type = content_type;
    blob->size = file_length;

    base::RunLoop run_loop;
    auto blob_context_getter = browser_context()->GetBlobStorageContext();
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
    return blink::mojom::SharedFile::New(*base::SafeBaseName::Create(name),
                                         std::move(blob));
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

#if BUILDFLAG(IS_CHROMEOS)
  static void AcceptShareRequest(
      content::WebContents* web_contents,
      const std::vector<base::FilePath>& file_paths,
      const std::vector<std::string>& content_types,
      const std::vector<uint64_t>& file_sizes,
      const std::string& text,
      const std::string& title,
      sharesheet::DeliveredCallback delivered_callback) {
    std::move(delivered_callback).Run(sharesheet::SharesheetResult::kSuccess);
  }
#endif

#if BUILDFLAG(IS_MAC)
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

#if BUILDFLAG(IS_WIN)
  webshare::ScopedShareOperationFakeComponents scoped_fake_components_;
#endif
  base::test::ScopedFeatureList feature_list_;
  mojo::Remote<blink::mojom::ShareService> share_service_remote_;
};

TEST_F(ShareServiceUnitTest, FileCount) {
  EXPECT_EQ(ShareError::OK, ShareGeneratedFileData(".txt", "text/plain", 1234,
                                                   kMaxSharedFileCount));
  EXPECT_EQ(ShareError::PERMISSION_DENIED,
            ShareGeneratedFileData(".txt", "text/plain", 1234,
                                   kMaxSharedFileCount + 1));
}

TEST_F(ShareServiceUnitTest, DangerousFilename) {
  EXPECT_TRUE(IsDangerousFilename(FILE_PATH_LITERAL("")));
  EXPECT_TRUE(IsDangerousFilename(FILE_PATH_LITERAL(".")));
  EXPECT_TRUE(IsDangerousFilename(FILE_PATH_LITERAL("./")));
  EXPECT_TRUE(IsDangerousFilename(FILE_PATH_LITERAL(".\\")));

  EXPECT_TRUE(IsDangerousFilename(FILE_PATH_LITERAL("a.a")));
  EXPECT_TRUE(IsDangerousFilename(FILE_PATH_LITERAL("zzz.zzz")));

  EXPECT_TRUE(IsDangerousFilename(FILE_PATH_LITERAL("a/a")));
  EXPECT_TRUE(IsDangerousFilename(FILE_PATH_LITERAL("zzz/zzz")));

  EXPECT_FALSE(IsDangerousFilename(FILE_PATH_LITERAL("1.XBM")));
  EXPECT_FALSE(IsDangerousFilename(FILE_PATH_LITERAL("2.bMP")));
  EXPECT_FALSE(IsDangerousFilename(FILE_PATH_LITERAL("3.Flac")));
  EXPECT_FALSE(IsDangerousFilename(FILE_PATH_LITERAL("4.webM")));
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
  EXPECT_EQ(ShareError::OK, ShareGeneratedFileData(".avif", "image/avif"));
  EXPECT_EQ(ShareError::OK, ShareGeneratedFileData(".bmp", "image/bmp"));
  EXPECT_EQ(ShareError::OK, ShareGeneratedFileData(".xbm", "image/x-xbitmap"));
  EXPECT_EQ(ShareError::OK, ShareGeneratedFileData(".flac", "audio/flac"));
  EXPECT_EQ(ShareError::OK, ShareGeneratedFileData(".webm", "video/webm"));
}

TEST_F(ShareServiceUnitTest, PortableDocumentFormat) {
  EXPECT_EQ(ShareError::OK, ShareGeneratedFileData(".pdf", "application/pdf"));
}

#if BUILDFLAG(IS_WIN)
TEST_F(ShareServiceUnitTest, ReservedNames) {
  EXPECT_TRUE(IsDangerousFilename(FILE_PATH_LITERAL("CON")));
  EXPECT_TRUE(IsDangerousFilename(FILE_PATH_LITERAL("PRN")));
  EXPECT_TRUE(IsDangerousFilename(FILE_PATH_LITERAL("AUX")));
  EXPECT_TRUE(IsDangerousFilename(FILE_PATH_LITERAL("NUL")));
  EXPECT_TRUE(IsDangerousFilename(FILE_PATH_LITERAL("COM1")));
  EXPECT_TRUE(IsDangerousFilename(FILE_PATH_LITERAL("COM9")));
  EXPECT_TRUE(IsDangerousFilename(FILE_PATH_LITERAL("LPT1")));
  EXPECT_TRUE(IsDangerousFilename(FILE_PATH_LITERAL("LPT9")));
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
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
