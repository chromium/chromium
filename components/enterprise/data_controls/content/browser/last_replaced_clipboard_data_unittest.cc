// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/content/browser/last_replaced_clipboard_data.h"

#include "testing/gmock/include/gmock/gmock.h"
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
  ASSERT_TRUE(GetLastReplacedClipboardData().GetAvailableTypes().empty());

  auto* observer = LastReplacedClipboardDataObserver::GetInstance();
  ASSERT_TRUE(observer);

  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
  ASSERT_TRUE(GetLastReplacedClipboardData().clipboard_paste_data.empty());
  ASSERT_TRUE(GetLastReplacedClipboardData().GetAvailableTypes().empty());
}

TEST_F(LastReplacedClipboardDataTest, WithPendingData) {
  ASSERT_TRUE(GetLastReplacedClipboardData().clipboard_paste_data.empty());
  ASSERT_TRUE(GetLastReplacedClipboardData().GetAvailableTypes().empty());

  auto* observer = LastReplacedClipboardDataObserver::GetInstance();
  ASSERT_TRUE(observer);

  content::ClipboardPasteData text_and_html;
  text_and_html.text = kText;
  text_and_html.html = kHtml;
  observer->AddDataToNextSeqno(std::move(text_and_html),
                               CopyRestrictionLevel::kKeptInManagedChrome);

  content::ClipboardPasteData bitmap;
  bitmap.bitmap = kBitmap;
  observer->AddDataToNextSeqno(std::move(bitmap),
                               CopyRestrictionLevel::kKeptInManagedChrome);

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

  auto available_types = GetLastReplacedClipboardData().GetAvailableTypes();
  ASSERT_EQ(available_types.size(), 3u);
  ASSERT_EQ(available_types[0], u"text/plain");
  ASSERT_EQ(available_types[1], u"text/html");
  ASSERT_EQ(available_types[2], u"image/png");
}

TEST_F(LastReplacedClipboardDataTest, WithRestrictionLevel) {
  ASSERT_EQ(GetLastReplacedClipboardData().restriction_level,
            CopyRestrictionLevel::kNone);

  auto* observer = LastReplacedClipboardDataObserver::GetInstance();
  ASSERT_TRUE(observer);

  content::ClipboardPasteData data;
  data.text = kText;
  observer->AddDataToNextSeqno(std::move(data), CopyRestrictionLevel::kBlocked);

  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();

  ASSERT_EQ(GetLastReplacedClipboardData().restriction_level,
            CopyRestrictionLevel::kBlocked);
}

TEST_F(LastReplacedClipboardDataTest, RestrictionLevelOverridePrecedence) {
  auto* observer = LastReplacedClipboardDataObserver::GetInstance();
  ASSERT_TRUE(observer);

  content::ClipboardPasteData data1;
  data1.text = kText;
  observer->AddDataToNextSeqno(std::move(data1),
                               CopyRestrictionLevel::kOngoingScan);

  content::ClipboardPasteData data2;
  data2.html = u"<html></html>";
  observer->AddDataToNextSeqno(std::move(data2),
                               CopyRestrictionLevel::kKeptInManagedChrome);

  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
  ASSERT_EQ(GetLastReplacedClipboardData().restriction_level,
            CopyRestrictionLevel::kKeptInManagedChrome);
  EXPECT_EQ(GetLastReplacedClipboardData().clipboard_paste_data.text, kText);
  EXPECT_EQ(GetLastReplacedClipboardData().clipboard_paste_data.html,
            u"<html></html>");

  content::ClipboardPasteData data3;
  data3.svg = u"<svg></svg>";
  observer->AddDataToNextSeqno(std::move(data3),
                               CopyRestrictionLevel::kOngoingScan);

  content::ClipboardPasteData data4;
  data4.png = {1, 2, 3};
  observer->AddDataToNextSeqno(std::move(data4),
                               CopyRestrictionLevel::kBlocked);

  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
  ASSERT_EQ(GetLastReplacedClipboardData().restriction_level,
            CopyRestrictionLevel::kBlocked);
  EXPECT_EQ(GetLastReplacedClipboardData().clipboard_paste_data.svg,
            u"<svg></svg>");
  EXPECT_EQ(GetLastReplacedClipboardData().clipboard_paste_data.png,
            std::vector<uint8_t>({1, 2, 3}));
}

}  // namespace data_controls
