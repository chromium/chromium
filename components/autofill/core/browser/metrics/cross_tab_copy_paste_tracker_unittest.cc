// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/cross_tab_copy_paste_tracker.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/sessions/core/session_id.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/test_clipboard.h"

namespace autofill {
namespace {

using ::testing::IsEmpty;
using ::testing::SizeIs;

class CrossTabCopyPasteTrackerTest : public testing::Test {
 protected:
  void SetUp() override { ui::TestClipboard::CreateForCurrentThread(); }

  void TearDown() override {
    ui::TestClipboard::DestroyClipboardForCurrentThread();
  }

  void WriteToClipboard(const std::u16string& text) {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(text);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  CrossTabCopyPasteTracker tracker_;
  SessionID tab1_ = SessionID::FromSerializedValue(1);
  SessionID tab2_ = SessionID::FromSerializedValue(2);
};

TEST_F(CrossTabCopyPasteTrackerTest, SameTabDoesNotRecordPasteUkm) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const std::u16string text = u"Test text";
  WriteToClipboard(text);

  tracker_.OnCopy(tab1_, base::FastHash(base::as_byte_span(text)),
                  ukm::AssignNewSourceId());
  tracker_.OnPaste(tab1_, ukm::AssignNewSourceId());

  // Only the copy event should be recorded.
  EXPECT_THAT(ukm_recorder.GetEntriesByName(
                  ukm::builders::Clipboard_CopyPasteEvent::kEntryName),
              SizeIs(1));
}

TEST_F(CrossTabCopyPasteTrackerTest, SequenceTooSlowDoesNotRecordPasteUkm) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const std::u16string text = u"Test text";
  WriteToClipboard(text);

  tracker_.OnCopy(tab1_, base::FastHash(base::as_byte_span(text)),
                  ukm::AssignNewSourceId());
  task_environment_.FastForwardBy(base::Minutes(11));
  tracker_.OnPaste(tab2_, ukm::AssignNewSourceId());

  // Only the copy event should be recorded.
  EXPECT_THAT(ukm_recorder.GetEntriesByName(
                  ukm::builders::Clipboard_CopyPasteEvent::kEntryName),
              SizeIs(1));
}

TEST_F(CrossTabCopyPasteTrackerTest, NewCopyResetsTimer) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const std::u16string text = u"Test text";
  const size_t content_hash = base::FastHash(base::as_byte_span(text));
  WriteToClipboard(text);

  tracker_.OnCopy(tab1_, content_hash, ukm::AssignNewSourceId());
  task_environment_.FastForwardBy(base::Minutes(11));
  tracker_.OnCopy(tab1_, content_hash, ukm::AssignNewSourceId());
  tracker_.OnPaste(tab2_, ukm::AssignNewSourceId());

  // Two copy events plus one paste event matching the second copy should be
  // recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Clipboard_CopyPasteEvent::kEntryName);
  ASSERT_THAT(entries, SizeIs(3));
  const int64_t* second_copy_nonce =
      ukm::TestAutoSetUkmRecorder::GetEntryMetric(
          entries[1],
          ukm::builders::Clipboard_CopyPasteEvent::kCopyFromTabNonceName);
  const int64_t* paste_nonce = ukm::TestAutoSetUkmRecorder::GetEntryMetric(
      entries[2],
      ukm::builders::Clipboard_CopyPasteEvent::kPasteFromTabNonceName);
  ASSERT_NE(second_copy_nonce, nullptr);
  ASSERT_NE(paste_nonce, nullptr);
  EXPECT_EQ(*second_copy_nonce, *paste_nonce);
}

TEST_F(CrossTabCopyPasteTrackerTest, PasteWithoutCopyDoesNotRecordUkm) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  tracker_.OnPaste(tab2_, ukm::AssignNewSourceId());

  EXPECT_THAT(ukm_recorder.GetEntriesByName(
                  ukm::builders::Clipboard_CopyPasteEvent::kEntryName),
              IsEmpty());
}

// Verifies that a valid copy-paste sequence records the `CopyFromTabNonce`
// UKM metric on copy and the matching `PasteFromTabNonce` UKM metric on paste.
TEST_F(CrossTabCopyPasteTrackerTest, RecordsCopyPasteUkmMetric) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const std::u16string text = u"Test text copied to clipboard";
  const ukm::SourceId copy_source_id = ukm::AssignNewSourceId();
  const ukm::SourceId paste_source_id = ukm::AssignNewSourceId();
  const size_t content_hash = base::FastHash(base::as_byte_span(text));

  WriteToClipboard(text);

  // 1. Trigger copy to clipboard.
  tracker_.OnCopy(tab1_, content_hash, copy_source_id);

  // Verify UKM Copy Event was recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Clipboard_CopyPasteEvent::kEntryName);
  ASSERT_THAT(entries, SizeIs(1));

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
  tracker_.OnPaste(tab2_, paste_source_id);

  // Verify a second UKM Copy Event was recorded (associated with the paste).
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Clipboard_CopyPasteEvent::kEntryName);
  ASSERT_THAT(entries, SizeIs(2));

  // Verify the PasteFromTabNonce metric exists and both nonces match.
  const int64_t* paste_nonce = ukm::TestAutoSetUkmRecorder::GetEntryMetric(
      entries[1],
      ukm::builders::Clipboard_CopyPasteEvent::kPasteFromTabNonceName);
  ASSERT_NE(paste_nonce, nullptr);
  EXPECT_EQ(*copy_nonce, *paste_nonce);
}

}  // namespace
}  // namespace autofill
