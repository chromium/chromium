// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/webshare/share_service_impl.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "url/gurl.h"

using blink::mojom::ShareError;

class ShareServiceUnitTest : public ChromeRenderViewHostTestHarness {
 public:
  ShareServiceUnitTest() {
    feature_list_.InitAndEnableFeature(features::kWebShare);
  }
  ~ShareServiceUnitTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    share_service_ = std::make_unique<ShareServiceImpl>(*main_rfh());
  }

  ShareError ShareGeneratedFileData(const std::string& extension,
                                    const std::string& mime_type,
                                    unsigned file_length,
                                    unsigned file_count = 1) {
    const std::string kTitle;
    const std::string kText;
    const GURL kUrl;
    std::vector<blink::mojom::SharedFilePtr> files;
    files.reserve(file_count);
    for (unsigned index = 0; index < file_count; ++index) {
      const std::string name =
          base::StringPrintf("share%d%s", index, extension.c_str());
      auto blob = blink::mojom::SerializedBlob::New();
      blob->content_type = mime_type;
      blob->size = file_length;
      files.push_back(blink::mojom::SharedFile::New(name, std::move(blob)));
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
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ShareServiceImpl> share_service_;
};

TEST_F(ShareServiceUnitTest, FileCount) {
  EXPECT_EQ(
      ShareError::CANCELED,
      ShareGeneratedFileData(".txt", "text/plain", 1234, kMaxSharedFileCount));
  EXPECT_EQ(ShareError::PERMISSION_DENIED,
            ShareGeneratedFileData(".txt", "text/plain", 1234,
                                   kMaxSharedFileCount + 1));
}

TEST_F(ShareServiceUnitTest, TotalBytes) {
  EXPECT_EQ(ShareError::CANCELED,
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
  EXPECT_EQ(ShareError::CANCELED,
            ShareGeneratedFileData(".txt", "text/plain", kMaxSharedFileBytes));
  EXPECT_EQ(
      ShareError::PERMISSION_DENIED,
      ShareGeneratedFileData(".txt", "text/plain", kMaxSharedFileBytes + 1));
}
