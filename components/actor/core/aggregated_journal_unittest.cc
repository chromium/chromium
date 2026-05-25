// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/core/aggregated_journal.h"

#include "base/test/task_environment.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

namespace {

class MockJournalObserver : public AggregatedJournal::Observer {
 public:
  explicit MockJournalObserver(AggregatedJournal& journal)
      : journal_(journal.GetSafeRef()) {
    journal.AddObserver(this);
  }
  ~MockJournalObserver() override { journal_->RemoveObserver(this); }

  MOCK_METHOD(void,
              WillAddJournalEntry,
              (const AggregatedJournal::Entry& entry),
              (override));

 private:
  base::SafeRef<AggregatedJournal> journal_;
};

TEST(AggregatedJournalTest, Log) {
  base::test::TaskEnvironment task_environment;
  AggregatedJournal journal;
  MockJournalObserver observer(journal);
  EXPECT_CALL(observer, WillAddJournalEntry(testing::_)).Times(1);

  journal.Log(GURL(), TaskId(0), "Test",
              JournalDetailsBuilder().Add("details", "Nothing").Build());
}

TEST(AggregatedJournalTest, LogProto) {
  base::test::TaskEnvironment task_environment;
  AggregatedJournal journal;
  MockJournalObserver observer(journal);
  EXPECT_CALL(observer, WillAddJournalEntry(testing::_)).Times(1);

  optimization_guide::proto::Actions actions_proto;
  journal.LogProto(GURL(), TaskId(0),
                   MakeGlicExperimentalTriggeringTrackUUID("test_context"),
                   "GlicProtoEvent", /*details=*/{}, actions_proto);
}

}  // namespace

}  // namespace actor
