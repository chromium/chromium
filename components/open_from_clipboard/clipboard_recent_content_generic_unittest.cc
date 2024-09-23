// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/open_from_clipboard/clipboard_recent_content_generic.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/open_from_clipboard/clipboard_recent_content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace {

class HasDataCallbackWaiter {
 public:
  explicit HasDataCallbackWaiter(ClipboardRecentContentGeneric* recent_content)
      : received_(false) {
    std::set<ClipboardContentType> desired_types = {
        ClipboardContentType::URL, ClipboardContentType::Text,
        ClipboardContentType::Image};

    recent_content->HasRecentContentFromClipboard(
        desired_types, base::BindOnce(&HasDataCallbackWaiter::OnComplete,
                                      weak_ptr_factory_.GetWeakPtr()));
  }

  void WaitForCallbackDone() {
    if (received_)
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  std::set<ClipboardContentType> GetContentType() { return result; }

 private:
  void OnComplete(std::set<ClipboardContentType> matched_types) {
    result = std::move(matched_types);
    received_ = true;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  base::OnceClosure quit_closure_;
  bool received_;
  std::set<ClipboardContentType> result;

  base::WeakPtrFactory<HasDataCallbackWaiter> weak_ptr_factory_{this};
};

}  // namespace

const char kChromeUIScheme[] = "chrome";

class ClipboardRecentContentGenericTest : public testing::Test {
 protected:
  void SetUp() override {
    // Make sure "chrome" as standard scheme for non chrome embedder.
    std::vector<std::string> standard_schemes = url::GetStandardSchemes();
    if (!base::Contains(standard_schemes, kChromeUIScheme)) {
      url::AddStandardScheme(kChromeUIScheme, url::SCHEME_WITH_HOST);
    }

    test_clipboard_ = ui::TestClipboard::CreateForCurrentThread();
  }

  void TearDown() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  raw_ptr<ui::TestClipboard, DanglingUntriaged> test_clipboard_;
  url::ScopedSchemeRegistryForTests scoped_scheme_registry_;
};

TEST_F(ClipboardRecentContentGenericTest, RecognizesURLs) {
  struct {
    std::string clipboard;
    const bool expected_get_recent_url_value;
  } test_data[] = {
      {"www", false},
      {"query string", false},
      {"www.example.com", false},
      {"http://www.example.com/", true},
      // The missing trailing slash shouldn't matter.
      {"http://www.example.com", true},
      {"https://another-example.com/", true},
      {"http://example.com/with-path/", true},
      {"about:version", true},
      {"chrome://urls", true},
      {"data:,Hello%2C%20World!", true},
      // Certain schemes are not eligible to be suggested.
      {"ftp://example.com/", true},
      // Leading and trailing spaces are okay, other spaces not.
      {"  http://leading.com", true},
      {" http://both.com/trailing  ", true},
      {"http://malformed url", false},
      {"http://another.com/malformed url", false},
      // Internationalized domain names should work.
      {"http://xn--c1yn36f", true},
      {" http://xn--c1yn36f/path   ", true},
      {"http://xn--c1yn36f extra ", false},
      {"http://點看", true},
      {"http://點看/path", true},
      {"  http://點看/path ", true},
      {" http://點看/path extra word", false},
  };

  ClipboardRecentContentGeneric recent_content;
  base::Time now = base::Time::Now();
  for (size_t i = 0; i < std::size(test_data); ++i) {
    test_clipboard_->WriteText(test_data[i].clipboard);
    test_clipboard_->SetLastModifiedTime(now - base::Seconds(10));
    EXPECT_EQ(test_data[i].expected_get_recent_url_value,
              recent_content.GetRecentURLFromClipboard().has_value())
        << "for input " << test_data[i].clipboard;
  }
}

TEST_F(ClipboardRecentContentGenericTest,
       OlderContentNotSuggestedDefaultLimit) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kClipboardMaximumAge, {{kClipboardMaximumAgeParam, "600"}});
  ClipboardRecentContentGeneric recent_content;
  base::Time now = base::Time::Now();
  std::string text = "http://example.com/";
  test_clipboard_->WriteText(text);
  test_clipboard_->SetLastModifiedTime(now - base::Minutes(9));
  EXPECT_TRUE(recent_content.GetRecentURLFromClipboard().has_value());
  // If the last modified time is 10 minutes ago, the URL shouldn't be
  // suggested.
  test_clipboard_->SetLastModifiedTime(now - base::Minutes(11));
  EXPECT_FALSE(recent_content.GetRecentURLFromClipboard().has_value());
}

TEST_F(ClipboardRecentContentGenericTest, OlderContentNotSuggestedLowerLimit) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kClipboardMaximumAge, {{kClipboardMaximumAgeParam, "119"}});
  ClipboardRecentContentGeneric recent_content;
  base::Time now = base::Time::Now();
  std::string text = "http://example.com/";
  test_clipboard_->WriteText(text);
  test_clipboard_->SetLastModifiedTime(now - base::Minutes(2));
  EXPECT_FALSE(recent_content.GetRecentURLFromClipboard().has_value());
}

TEST_F(ClipboardRecentContentGenericTest, GetClipboardContentAge) {
  ClipboardRecentContentGeneric recent_content;
  base::Time now = base::Time::Now();
  std::string text = " whether URL or not should not matter here.";
  test_clipboard_->WriteText(text);
  test_clipboard_->SetLastModifiedTime(now - base::Seconds(32));
  base::TimeDelta age = recent_content.GetClipboardContentAge();
  // It's possible the GetClipboardContentAge() took some time, so allow a
  // little slop (5 seconds) in this comparison; don't check for equality.
  EXPECT_LT(age - base::Seconds(32), base::Seconds(5));
}

TEST_F(ClipboardRecentContentGenericTest, SuppressClipboardContent) {
  // Make sure the URL is suggested.
  ClipboardRecentContentGeneric recent_content;
  base::Time now = base::Time::Now();
  std::string text = "http://example.com/";
  test_clipboard_->WriteText(text);
  test_clipboard_->SetLastModifiedTime(now - base::Seconds(10));
  EXPECT_TRUE(recent_content.GetRecentURLFromClipboard().has_value());
  EXPECT_TRUE(recent_content.GetRecentTextFromClipboard().has_value());
  EXPECT_FALSE(recent_content.HasRecentImageFromClipboard());

  // After suppressing it, it shouldn't be suggested.
  recent_content.SuppressClipboardContent();
  EXPECT_FALSE(recent_content.GetRecentURLFromClipboard().has_value());

  // If the clipboard changes, even if to the same thing again, the content
  // should be suggested again.
  test_clipboard_->WriteText(text);
  test_clipboard_->SetLastModifiedTime(now);
  EXPECT_TRUE(recent_content.GetRecentURLFromClipboard().has_value());
  EXPECT_TRUE(recent_content.GetRecentTextFromClipboard().has_value());
  EXPECT_FALSE(recent_content.HasRecentImageFromClipboard());
}

TEST_F(ClipboardRecentContentGenericTest, GetRecentTextFromClipboard) {
  // Make sure the Text is suggested.
  ClipboardRecentContentGeneric recent_content;
  base::Time now = base::Time::Now();
  std::string text = "  Foo Bar   ";
  test_clipboard_->WriteText(text);
  test_clipboard_->SetLastModifiedTime(now - base::Seconds(10));
  EXPECT_TRUE(recent_content.GetRecentTextFromClipboard().has_value());
  EXPECT_FALSE(recent_content.GetRecentURLFromClipboard().has_value());
  EXPECT_FALSE(recent_content.HasRecentImageFromClipboard());
  EXPECT_STREQ(
      "Foo Bar",
      base::UTF16ToUTF8(recent_content.GetRecentTextFromClipboard().value())
          .c_str());
}

TEST_F(ClipboardRecentContentGenericTest, ClearClipboardContent) {
  // Make sure the URL is suggested.
  ClipboardRecentContentGeneric recent_content;
  base::Time now = base::Time::Now();
  std::string text = "http://example.com/";
  test_clipboard_->WriteText(text);
  test_clipboard_->SetLastModifiedTime(now - base::Seconds(10));
  EXPECT_TRUE(recent_content.GetRecentURLFromClipboard().has_value());

  // After clear it, it shouldn't be suggested.
  recent_content.ClearClipboardContent();
  EXPECT_FALSE(recent_content.GetRecentURLFromClipboard().has_value());

  // If the clipboard changes, even if to the same thing again, the content
  // should be suggested again.
  test_clipboard_->WriteText(text);
  test_clipboard_->SetLastModifiedTime(now);
  EXPECT_TRUE(recent_content.GetRecentURLFromClipboard().has_value());
}

TEST_F(ClipboardRecentContentGenericTest, HasRecentImageFromClipboard) {
  ClipboardRecentContentGeneric recent_content;
  base::Time now = base::Time::Now();
  SkBitmap bitmap = gfx::test::CreateBitmap(3, 2);

  EXPECT_FALSE(recent_content.HasRecentImageFromClipboard());
  test_clipboard_->WriteBitmap(bitmap);
  test_clipboard_->SetLastModifiedTime(now - base::Seconds(10));
  EXPECT_TRUE(recent_content.HasRecentImageFromClipboard());
  EXPECT_FALSE(recent_content.GetRecentURLFromClipboard().has_value());
  EXPECT_FALSE(recent_content.GetRecentTextFromClipboard().has_value());
  recent_content.GetRecentImageFromClipboard(
      base::BindLambdaForTesting([&bitmap](std::optional<gfx::Image> image) {
        EXPECT_TRUE(gfx::BitmapsAreEqual(image->AsBitmap(), bitmap));
      }));
}

TEST_F(ClipboardRecentContentGenericTest, HasRecentContentFromClipboard_URL) {
  ClipboardRecentContentGeneric recent_content;
  base::Time now = base::Time::Now();
  std::string title = "foo";
  std::string url_text = "http://example.com/";
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // The linux and chromeos clipboard treats the presence of text on the
  // clipboard as the url format being available.
  test_clipboard_->WriteText(url_text);
#else
  test_clipboard_->WriteBookmark(title, url_text);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  test_clipboard_->SetLastModifiedTime(now - base::Seconds(10));

  HasDataCallbackWaiter waiter(&recent_content);
  waiter.WaitForCallbackDone();
  std::set<ClipboardContentType> types = waiter.GetContentType();

  EXPECT_TRUE(types.find(ClipboardContentType::URL) != types.end());
}

TEST_F(ClipboardRecentContentGenericTest, HasRecentContentFromClipboard_Text) {
  ClipboardRecentContentGeneric recent_content;
  base::Time now = base::Time::Now();
  std::string text = "  Foo Bar   ";
  test_clipboard_->WriteText(text);
  test_clipboard_->SetLastModifiedTime(now - base::Seconds(10));

  HasDataCallbackWaiter waiter(&recent_content);
  waiter.WaitForCallbackDone();
  std::set<ClipboardContentType> types = waiter.GetContentType();

  EXPECT_TRUE(types.find(ClipboardContentType::Text) != types.end());
}

TEST_F(ClipboardRecentContentGenericTest, HasRecentContentFromClipboard_Image) {
  ClipboardRecentContentGeneric recent_content;
  base::Time now = base::Time::Now();
  SkBitmap bitmap = gfx::test::CreateBitmap(3, 2);
  test_clipboard_->WriteBitmap(bitmap);
  test_clipboard_->SetLastModifiedTime(now - base::Seconds(10));

  HasDataCallbackWaiter waiter(&recent_content);
  waiter.WaitForCallbackDone();
  std::set<ClipboardContentType> types = waiter.GetContentType();

  EXPECT_TRUE(types.find(ClipboardContentType::Image) != types.end());
}
