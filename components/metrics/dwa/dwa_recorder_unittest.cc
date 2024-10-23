// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_recorder.h"

#include "base/test/scoped_feature_list.h"
#include "components/metrics/dwa/dwa_entry_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics::dwa {
namespace {

class DwaRecorderTestBase : public testing::Test {
 public:
  explicit DwaRecorderTestBase(bool enable_feature) {
    if (enable_feature) {
      scoped_feature_list_.InitAndEnableFeature(kDwaFeature);
    } else {
      scoped_feature_list_.InitAndDisableFeature(kDwaFeature);
    }
    recorder_ = DwaRecorder::Get();
    recorder_->Purge();
    recorder_->EnableRecording();
  }
  ~DwaRecorderTestBase() override = default;

  DwaRecorder* GetRecorder() { return recorder_; }

 private:
  raw_ptr<DwaRecorder> recorder_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class DwaRecorderEnabledTest : public DwaRecorderTestBase {
 public:
  DwaRecorderEnabledTest() : DwaRecorderTestBase(/*enable_feature=*/true) {}
};

class DwaRecorderDisabledTest : public DwaRecorderTestBase {
 public:
  DwaRecorderDisabledTest() : DwaRecorderTestBase(/*enable_feature=*/false) {}
};

TEST_F(DwaRecorderEnabledTest, ValidateHasEntriesWhenEntryIsAdded) {
  ::dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.SetContent("adtech.com");
  builder.SetMetric("Length", 5);
  builder.Record(GetRecorder());

  EXPECT_TRUE(GetRecorder()->HasEntries());
}

TEST_F(DwaRecorderEnabledTest, ValidateEntriesWhenRecordingIsDisabled) {
  GetRecorder()->DisableRecording();

  ::dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.SetContent("adtech.com");
  builder.SetMetric("Length", 5);
  builder.Record(GetRecorder());

  EXPECT_FALSE(GetRecorder()->HasEntries());
}

TEST_F(DwaRecorderEnabledTest, ValidateOnPageLoadCreatesPageLoadEvents) {
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

TEST_F(DwaRecorderEnabledTest,
       ValidateOnPageLoadDoesNotCreatePageLoadEventsWhenEntriesIsEmpty) {
  EXPECT_FALSE(GetRecorder()->HasEntries());
  GetRecorder()->OnPageLoad();
  EXPECT_TRUE(GetRecorder()->TakePageLoadEvents().empty());
}

TEST_F(DwaRecorderDisabledTest, FeatureDisabled) {
  ::dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.SetContent("adtech.com");
  builder.SetMetric("Length", 5);
  builder.Record(GetRecorder());

  EXPECT_FALSE(GetRecorder()->HasEntries());
}

}  // namespace
}  // namespace metrics::dwa
