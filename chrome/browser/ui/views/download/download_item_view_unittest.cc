// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_item_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/label.h"

using DownloadItemViewDangerousDownloadLabelTest = ChromeViewsTestBase;

TEST_F(DownloadItemViewDangerousDownloadLabelTest, AdjustTextAndGetSize) {
  // For very short label that can fit in a single line, no need to do any
  // adjustment, return it directly.
  base::string16 label_text = base::ASCIIToUTF16("short");
  views::Label label(label_text);
  label.SetMultiLine(true);
  DownloadItemView::AdjustTextAndGetSize(&label);
  EXPECT_EQ(label_text, label.GetText());

  // When we have multiple linebreaks that result in the same minimum width, we
  // should place as much text as possible on the first line.
  label_text = base::ASCIIToUTF16(
      "aaaa aaaa aaaa aaaa aaaa aaaa bb aaaa aaaa aaaa aaaa aaaa aaaa");
  base::string16 expected_text = base::ASCIIToUTF16(
      "aaaa aaaa aaaa aaaa aaaa aaaa bb\n"
      "aaaa aaaa aaaa aaaa aaaa aaaa");
  label.SetText(label_text);
  DownloadItemView::AdjustTextAndGetSize(&label);
  EXPECT_EQ(expected_text, label.GetText());

  // If the label is a single word and extremely long, we should not break it
  // into 2 lines.
  label_text = base::ASCIIToUTF16(
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  label.SetText(label_text);
  DownloadItemView::AdjustTextAndGetSize(&label);
  EXPECT_EQ(label_text, label.GetText());

  // Two lines have the same length.
  label_text =
      base::ASCIIToUTF16("aaaa aaaa aaaa aaaa bb bb aaaa aaaa aaaa aaaa");
  expected_text = base::ASCIIToUTF16(
      "aaaa aaaa aaaa aaaa bb\n"
      "bb aaaa aaaa aaaa aaaa");
  label.SetText(label_text);
  DownloadItemView::AdjustTextAndGetSize(&label);
  EXPECT_EQ(expected_text, label.GetText());

  // Text begins with a very long word.
  label_text = base::ASCIIToUTF16(
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa aaaa aaaa");
  expected_text = base::ASCIIToUTF16(
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "aaaa aaaa");
  label.SetText(label_text);
  DownloadItemView::AdjustTextAndGetSize(&label);
  EXPECT_EQ(expected_text, label.GetText());

  // Text ends with a very long word.
  label_text = base::ASCIIToUTF16(
      "aaa aaaa aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  expected_text = base::ASCIIToUTF16(
      "aaa aaaa\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  label.SetText(label_text);
  DownloadItemView::AdjustTextAndGetSize(&label);
  EXPECT_EQ(expected_text, label.GetText());
}
