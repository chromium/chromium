// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/open_from_clipboard/clipboard_recent_content_generic.h"

#include <memory>
#include <string>
#include <utility>

#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "url/gurl.h"

class ClipboardRecentContentGenericTest : public testing::Test {
 protected:
  void SetUp() override {
    test_clipboard_ = ui::TestClipboard::CreateForCurrentThread();
  }

  void TearDown() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  ui::TestClipboard* test_clipboard_;
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
      {"data:,Hello%2C%20World!", true},
      // Certain schemes are not eligible to be suggested.
      {"ftp://example.com/", false},
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
  for (size_t i = 0; i < base::size(test_data); ++i) {
    test_clipboard_->WriteText(test_data[i].clipboard.data(),
                               test_data[i].clipboard.length());
    test_clipboard_->SetLastModifiedTime(now -
                                         base::TimeDelta::FromSeconds(10));
    EXPECT_EQ(test_data[i].expected_get_recent_url_value,
              recent_content.GetRecentURLFromClipboard().has_value())
        << "for input " << test_data[i].clipboard;
  }
}

TEST_F(ClipboardRecentContentGenericTest, OlderURLsNotSuggested) {
  ClipboardRecentContentGeneric recent_content;
  base::Time now = base::Time::Now();
  std::string text = "http://example.com/";
  test_clipboard_->WriteText(text.data(), text.length());
  test_clipboard_->SetLastModifiedTime(now - base::TimeDelta::FromSeconds(10));
  EXPECT_TRUE(recent_content.GetRecentURLFromClipboard().has_value());
  // If the last modified time is days ago, the URL shouldn't be suggested.
  test_clipboard_->SetLastModifiedTime(now - base::TimeDelta::FromDays(2));
  EXPECT_FALSE(recent_content.GetRecentURLFromClipboard().has_value());
}

TEST_F(ClipboardRecentContentGenericTest, GetClipboardContentAge) {
  ClipboardRecentContentGeneric recent_content;
  base::Time now = base::Time::Now();
  std::string text = " whether URL or not should not matter here.";
  test_clipboard_->WriteText(text.data(), text.length());
  test_clipboard_->SetLastModifiedTime(now - base::TimeDelta::FromSeconds(32));
  base::TimeDelta age = recent_content.GetClipboardContentAge();
  // It's possible the GetClipboardContentAge() took some time, so allow a
  // little slop (5 seconds) in this comparison; don't check for equality.
  EXPECT_LT(age - base::TimeDelta::FromSeconds(32),
            base::TimeDelta::FromSeconds(5));
}

TEST_F(ClipboardRecentContentGenericTest, SuppressClipboardContent) {
  // Make sure the URL is suggested.
  ClipboardRecentContentGeneric recent_content;
  base::Time now = base::Time::Now();
  std::string text = "http://example.com/";
  test_clipboard_->WriteText(text.data(), text.length());
  test_clipboard_->SetLastModifiedTime(now - base::TimeDelta::FromSeconds(10));
  EXPECT_TRUE(recent_content.GetRecentURLFromClipboard().has_value());

  // After suppressing it, it shouldn't be suggested.
  recent_content.SuppressClipboardContent();
  EXPECT_FALSE(recent_content.GetRecentURLFromClipboard().has_value());

  // If the clipboard changes, even if to the same thing again, the content
  // should be suggested again.
  test_clipboard_->WriteText(text.data(), text.length());
  test_clipboard_->SetLastModifiedTime(now);
  EXPECT_TRUE(recent_content.GetRecentURLFromClipboard().has_value());
}

TEST_F(ClipboardRecentContentGenericTest, GetRecentTextFromClipboard) {
  // Make sure the Text is suggested.
  ClipboardRecentContentGeneric recent_content;
  base::Time now = base::Time::Now();
  std::string text = "  Foo Bar   ";
  test_clipboard_->WriteText(text.data(), text.length());
  test_clipboard_->SetLastModifiedTime(now - base::TimeDelta::FromSeconds(10));
  EXPECT_TRUE(recent_content.GetRecentTextFromClipboard().has_value());
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
  test_clipboard_->WriteText(text.data(), text.length());
  test_clipboard_->SetLastModifiedTime(now - base::TimeDelta::FromSeconds(10));
  EXPECT_TRUE(recent_content.GetRecentURLFromClipboard().has_value());

  // After clear it, it shouldn't be suggested.
  recent_content.ClearClipboardContent();
  EXPECT_FALSE(recent_content.GetRecentURLFromClipboard().has_value());

  // If the clipboard changes, even if to the same thing again, the content
  // should be suggested again.
  test_clipboard_->WriteText(text.data(), text.length());
  test_clipboard_->SetLastModifiedTime(now);
  EXPECT_TRUE(recent_content.GetRecentURLFromClipboard().has_value());
}
