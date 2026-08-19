// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/cross_tab_copy_paste_tracker.h"

#include "base/hash/hash.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/sessions/core/session_id.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/test_clipboard.h"

namespace autofill {

class CrossTabCopyPasteTrackerTest : public testing::Test {
 protected:
  void SetUp() override { ui::TestClipboard::CreateForCurrentThread(); }

  void TearDown() override {
    ui::TestClipboard::DestroyClipboardForCurrentThread();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  CrossTabCopyPasteTracker tracker_;
  SessionID tab1_ = SessionID::FromSerializedValue(1);
  SessionID tab2_ = SessionID::FromSerializedValue(2);
};

TEST_F(CrossTabCopyPasteTrackerTest, ValidSequenceTriggers) {
  tracker_.OnCopy(tab1_, /*content_hash=*/0, ukm::AssignNewSourceId());
  EXPECT_TRUE(tracker_.OnPaste(tab2_, ukm::AssignNewSourceId()));
}

TEST_F(CrossTabCopyPasteTrackerTest, SameTabDoesNotTrigger) {
  tracker_.OnCopy(tab1_, /*content_hash=*/0, ukm::AssignNewSourceId());
  EXPECT_FALSE(tracker_.OnPaste(tab1_, ukm::AssignNewSourceId()));
}

TEST_F(CrossTabCopyPasteTrackerTest, SequenceTooSlow) {
  tracker_.OnCopy(tab1_, /*content_hash=*/0, ukm::AssignNewSourceId());
  task_environment_.FastForwardBy(base::Seconds(61));
  EXPECT_FALSE(tracker_.OnPaste(tab2_, ukm::AssignNewSourceId()));
}

TEST_F(CrossTabCopyPasteTrackerTest, NewCopyResetsTimer) {
  tracker_.OnCopy(tab1_, /*content_hash=*/0, ukm::AssignNewSourceId());
  task_environment_.FastForwardBy(base::Seconds(61));
  tracker_.OnCopy(tab1_, /*content_hash=*/0, ukm::AssignNewSourceId());
  EXPECT_TRUE(tracker_.OnPaste(tab2_, ukm::AssignNewSourceId()));
}

TEST_F(CrossTabCopyPasteTrackerTest, PasteWithoutCopyFails) {
  EXPECT_FALSE(tracker_.OnPaste(tab2_, ukm::AssignNewSourceId()));
}

// Verifies that a valid copy-paste sequence records the `CopyFromTabNonce`
// UKM metric on copy and the matching `PasteFromTabNonce` UKM metric on paste
// for Clipboard.CopyPasteEvent when AtMemory is not enabled.
TEST_F(CrossTabCopyPasteTrackerTest,
       RecordsCopyPasteUkmMetric_WithoutAtMemory) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillAtMemory);

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const std::u16string text = u"Test text copied to clipboard";
  const ukm::SourceId copy_source_id = ukm::AssignNewSourceId();
  const ukm::SourceId paste_source_id = ukm::AssignNewSourceId();
  const size_t content_hash = base::FastHash(base::as_byte_span(text));

  // Write the copied text to the clipboard so that OnPaste can read it.
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(text);
  }

  // 1. Trigger copy to clipboard.
  tracker_.OnCopy(tab1_, content_hash, copy_source_id);

  // Verify UKM Copy Event was recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Clipboard_CopyPasteEvent::kEntryName);
  ASSERT_EQ(entries.size(), 1u);

  // Verify the CopyFromTabNonce metric exists and is non-zero.
  const int64_t* copy_nonce = ukm::TestAutoSetUkmRecorder::GetEntryMetric(
      entries[0],
      ukm::builders::Clipboard_CopyPasteEvent::kCopyFromTabNonceName);
  ASSERT_NE(copy_nonce, nullptr);

  // Verify the PasteFromTabNonce metric does not exist yet.
  {
    const int64_t* paste_nonce = ukm::TestAutoSetUkmRecorder::GetEntryMetric(
        entries[0],
        ukm::builders::Clipboard_CopyPasteEvent::kPasteFromTabNonceName);
    ASSERT_EQ(paste_nonce, nullptr);
  }

  // 2. Trigger paste in a different tab.
  EXPECT_TRUE(tracker_.OnPaste(tab2_, paste_source_id));

  // Flush the task environment so the async clipboard callback runs.
  task_environment_.RunUntilIdle();

  // Verify a second UKM Copy Event was recorded (associated with the paste).
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Clipboard_CopyPasteEvent::kEntryName);
  ASSERT_EQ(entries.size(), 2u);

  // Verify the PasteFromTabNonce metric exists and both nonces match.
  const int64_t* paste_nonce = ukm::TestAutoSetUkmRecorder::GetEntryMetric(
      entries[1],
      ukm::builders::Clipboard_CopyPasteEvent::kPasteFromTabNonceName);
  ASSERT_NE(paste_nonce, nullptr);
  EXPECT_EQ(*copy_nonce, *paste_nonce);

  // Verify that WithAtMemory event was NOT recorded.
  EXPECT_EQ(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Clipboard_CopyPasteEvent_WithAtMemory::kEntryName)
          .size(),
      0u);
}

// Verifies that a valid copy-paste sequence records the `CopyFromTabNonce`
// UKM metric on copy and the matching `PasteFromTabNonce` UKM metric on paste
// for Clipboard.CopyPasteEvent.WithAtMemory when AtMemory is enabled.
TEST_F(CrossTabCopyPasteTrackerTest, RecordsCopyPasteUkmMetric_WithAtMemory) {
  base::test::ScopedFeatureList feature_list(features::kAutofillAtMemory);

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const std::u16string text = u"Test text copied to clipboard";
  const ukm::SourceId copy_source_id = ukm::AssignNewSourceId();
  const ukm::SourceId paste_source_id = ukm::AssignNewSourceId();
  const size_t content_hash = base::FastHash(base::as_byte_span(text));

  // Write the copied text to the clipboard so that OnPaste can read it.
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(text);
  }

  // 1. Trigger copy to clipboard.
  tracker_.OnCopy(tab1_, content_hash, copy_source_id);

  // Verify UKM Copy Event was recorded for WithAtMemory.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Clipboard_CopyPasteEvent_WithAtMemory::kEntryName);
  ASSERT_EQ(entries.size(), 1u);

  // Verify the CopyFromTabNonce metric exists and is non-zero.
  const int64_t* copy_nonce = ukm::TestAutoSetUkmRecorder::GetEntryMetric(
      entries[0], ukm::builders::Clipboard_CopyPasteEvent_WithAtMemory::
                      kCopyFromTabNonceName);
  ASSERT_NE(copy_nonce, nullptr);

  // Verify the PasteFromTabNonce metric does not exist yet.
  {
    const int64_t* paste_nonce = ukm::TestAutoSetUkmRecorder::GetEntryMetric(
        entries[0], ukm::builders::Clipboard_CopyPasteEvent_WithAtMemory::
                        kPasteFromTabNonceName);
    ASSERT_EQ(paste_nonce, nullptr);
  }

  // 2. Trigger paste in a different tab.
  EXPECT_TRUE(tracker_.OnPaste(tab2_, paste_source_id));

  // Flush the task environment so the async clipboard callback runs.
  task_environment_.RunUntilIdle();

  // Verify a second UKM Copy Event was recorded (associated with the paste).
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Clipboard_CopyPasteEvent_WithAtMemory::kEntryName);
  ASSERT_EQ(entries.size(), 2u);

  // Verify the PasteFromTabNonce metric exists and both nonces match.
  const int64_t* paste_nonce = ukm::TestAutoSetUkmRecorder::GetEntryMetric(
      entries[1], ukm::builders::Clipboard_CopyPasteEvent_WithAtMemory::
                      kPasteFromTabNonceName);
  ASSERT_NE(paste_nonce, nullptr);
  EXPECT_EQ(*copy_nonce, *paste_nonce);

  // Verify that the standard Clipboard.CopyPasteEvent was NOT recorded.
  EXPECT_EQ(
      ukm_recorder
          .GetEntriesByName(ukm::builders::Clipboard_CopyPasteEvent::kEntryName)
          .size(),
      0u);
}

}  // namespace autofill
