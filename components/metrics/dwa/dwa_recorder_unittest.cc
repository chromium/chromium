// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_recorder.h"

#include "components/metrics/dwa/dwa_entry_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics::dwa {
class DwaRecorderTest : public testing::Test {
 public:
  DwaRecorderTest() = default;
  ~DwaRecorderTest() override = default;

  void SetUp() override {
    recorder_ = DwaRecorder::Get();
    recorder_->Purge();
    recorder_->EnableRecording();
  }

  DwaRecorder* GetRecorder() { return recorder_; }

 private:
  raw_ptr<DwaRecorder> recorder_;
};

TEST_F(DwaRecorderTest, ValidateHasEntriesWhenEntryIsAdded) {
  ::dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.SetContent("adtech.com");
  builder.SetMetric("Length", 5);
  builder.Record(GetRecorder());

  EXPECT_TRUE(GetRecorder()->HasEntries());
}

TEST_F(DwaRecorderTest, ValidateEntriesWhenRecordingIsDisabled) {
  GetRecorder()->DisableRecording();

  ::dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.SetContent("adtech.com");
  builder.SetMetric("Length", 5);
  builder.Record(GetRecorder());

  EXPECT_FALSE(GetRecorder()->HasEntries());
}

TEST_F(DwaRecorderTest, ValidateOnPageLoadCreatesPageLoadEvents) {
  ::dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.SetContent("adtech.com");
  builder.SetMetric("Length", 5);
  builder.Record(GetRecorder());
  EXPECT_TRUE(GetRecorder()->HasEntries());
  EXPECT_TRUE(GetRecorder()->TakePageLoadEvents().empty());

  GetRecorder()->OnPageLoad();
  EXPECT_FALSE(GetRecorder()->HasEntries());
  // TODO(b/369464150): Change this to EXPECT_FALSE when BuildDwaEvent is
  // implemented.
  EXPECT_TRUE(GetRecorder()->TakePageLoadEvents().empty());
}

TEST_F(DwaRecorderTest,
       ValidateOnPageLoadDoesNotCreatePageLoadEventsWhenEntriesIsEmpty) {
  EXPECT_FALSE(GetRecorder()->HasEntries());
  GetRecorder()->OnPageLoad();
  EXPECT_TRUE(GetRecorder()->TakePageLoadEvents().empty());
}

}  // namespace metrics::dwa
