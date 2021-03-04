// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class WebUISourcesTest : public testing::Test {
 public:
  WebUISourcesTest() : result_data_size_(0) {}

  TestingProfile* profile() const { return profile_.get(); }
  ThemeSource* theme_source() const { return theme_source_.get(); }
  size_t result_data_size() const { return result_data_size_; }

  void StartDataRequest(const std::string& source) {
    theme_source()->StartDataRequest(
        GURL(base::StrCat({content::kChromeUIScheme, "://",
                           chrome::kChromeUIThemeHost, "/", source})),
        content::WebContents::Getter(),
        base::BindOnce(&WebUISourcesTest::SendResponse,
                       base::Unretained(this)));
  }

  size_t result_data_size_;

 private:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    theme_source_ = std::make_unique<ThemeSource>(profile_.get());
  }

  void TearDown() override {
    theme_source_.reset();
    profile_.reset();
  }

  void SendResponse(scoped_refptr<base::RefCountedMemory> data) {
    result_data_size_ = data ? data->size() : 0;
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ThemeSource> theme_source_;
};

TEST_F(WebUISourcesTest, ThemeSourceMimeTypes) {
  EXPECT_EQ(theme_source()->GetMimeType("css/new_tab_theme.css"), "text/css");
  EXPECT_EQ(theme_source()->GetMimeType("css/new_tab_theme.css?foo"),
                                        "text/css");
  EXPECT_EQ(theme_source()->GetMimeType("WRONGURL"), "image/png");
}

TEST_F(WebUISourcesTest, ThemeSourceImages) {
  // We used to PNGEncode the images ourselves, but encoder differences
  // invalidated that. We now just check that the image exists.
  StartDataRequest("IDR_THEME_FRAME_INCOGNITO");
  size_t min = 0;
  EXPECT_GT(result_data_size_, min);

  StartDataRequest("IDR_THEME_TOOLBAR");
  EXPECT_GT(result_data_size_, min);
}

TEST_F(WebUISourcesTest, ThemeSourceCSS) {
  // Generating the test data for the NTP CSS would just involve copying the
  // method, or being super brittle and hard-coding the result (requiring
  // an update to the unittest every time the CSS template changes), so we
  // just check for a successful request and data that is non-null.
  size_t empty_size = 0;

  StartDataRequest("css/new_tab_theme.css");
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(result_data_size_, empty_size);

  StartDataRequest("css/new_tab_theme.css?pie");
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(result_data_size_, empty_size);

#if !DCHECK_IS_ON()
  // Check that we send NULL back when we can't find what we're looking for.
  StartDataRequest("css/WRONGURL");
  EXPECT_EQ(result_data_size_, empty_size);
#endif
}

TEST_F(WebUISourcesTest, ThemeAllowedOrigin) {
  EXPECT_EQ(
      theme_source()->GetAccessControlAllowOriginForOrigin("chrome://settings"),
      "chrome://settings");
  EXPECT_EQ(theme_source()->GetAccessControlAllowOriginForOrigin(
                "chrome-extensions://some-id"),
            "");
  EXPECT_EQ(
      theme_source()->GetAccessControlAllowOriginForOrigin("http://google.com"),
      "");
}
