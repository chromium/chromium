// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/content/browser/last_replaced_clipboard_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace data_controls {

namespace {

const SkBitmap kBitmap = gfx::test::CreateBitmap(2, 3);
constexpr char16_t kText[] = u"text";
constexpr char16_t kHtml[] = u"html";

class LastReplacedClipboardDataTest : public testing::Test {
 public:
  void SetUp() override { ui::TestClipboard::CreateForCurrentThread(); }

  void TearDown() override { GetLastReplacedClipboardData() = {}; }
};

}  // namespace

TEST_F(LastReplacedClipboardDataTest, WithoutPendingData) {
  ASSERT_TRUE(GetLastReplacedClipboardData().clipboard_paste_data.empty());

  auto* observer = LastReplacedClipboardDataObserver::GetInstance();
  ASSERT_TRUE(observer);

  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
  ASSERT_TRUE(GetLastReplacedClipboardData().clipboard_paste_data.empty());
}

TEST_F(LastReplacedClipboardDataTest, WithPendingData) {
  ASSERT_TRUE(GetLastReplacedClipboardData().clipboard_paste_data.empty());

  auto* observer = LastReplacedClipboardDataObserver::GetInstance();
  ASSERT_TRUE(observer);

  content::ClipboardPasteData text_and_html;
  text_and_html.text = kText;
  text_and_html.html = kHtml;
  observer->AddDataToNextSeqno(std::move(text_and_html));

  content::ClipboardPasteData bitmap;
  bitmap.bitmap = kBitmap;
  observer->AddDataToNextSeqno(std::move(bitmap));

  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();

  ASSERT_FALSE(GetLastReplacedClipboardData().clipboard_paste_data.empty());
  ASSERT_EQ(GetLastReplacedClipboardData().seqno,
            ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
                ui::ClipboardBuffer::kCopyPaste));
  ASSERT_EQ(GetLastReplacedClipboardData().clipboard_paste_data.text, kText);
  ASSERT_EQ(GetLastReplacedClipboardData().clipboard_paste_data.html, kHtml);

  // Bitmaps are converted to PNG as that is the format used for pasting that
  // will be used to substitute back replaced data.
  ASSERT_TRUE(
      GetLastReplacedClipboardData().clipboard_paste_data.bitmap.empty());
  ASSERT_EQ(GetLastReplacedClipboardData().clipboard_paste_data.png,
            ui::clipboard_util::EncodeBitmapToPngAcceptJank(kBitmap));
}

}  // namespace data_controls
