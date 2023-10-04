// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_entry.h"

#include <memory>
#include "base/memory/scoped_refptr.h"

#include "base/test/simple_test_tick_clock.h"
#include "components/reading_list/core/proto/reading_list.pb.h"
#include "components/sync/protocol/reading_list_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kFirstBackoff = 10;
const int kSecondBackoff = 10;
const int kThirdBackoff = 60;
const int kFourthBackoff = 120;
const int kFifthBackoff = 120;

}  // namespace

TEST(ReadingListEntry, CompareIgnoreTitle) {
  auto e1 = base::MakeRefCounted<const ReadingListEntry>(
      GURL("http://example.com"), "bar", base::Time::FromTimeT(10));
  auto e2 = base::MakeRefCounted<const ReadingListEntry>(
      GURL("http://example.com"), "foo", base::Time::FromTimeT(20));

  EXPECT_EQ(*e1, *e2);
}

TEST(ReadingListEntry, CompareFailureIgnoreTitleAndCreationTime) {
  auto e1 = base::MakeRefCounted<const ReadingListEntry>(
      GURL("http://example.com"), "bar", base::Time::FromTimeT(10));
  auto e2 = base::MakeRefCounted<const ReadingListEntry>(
      GURL("http://example.org"), "bar", base::Time::FromTimeT(10));

  EXPECT_FALSE(*e1 == *e2);
}

TEST(ReadingListEntry, MovesAreEquals) {
  auto e1 = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com"), "bar", base::Time::FromTimeT(10));
  auto e2 = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com"), "bar", base::Time::FromTimeT(10));
  EXPECT_EQ(*e1, *e2);
  EXPECT_EQ(e1->Title(), e2->Title());
  EXPECT_EQ(e1->CreationTime(), e2->CreationTime());

  scoped_refptr<ReadingListEntry> e3(std::move(e1));

  EXPECT_EQ(*e3, *e2);
  EXPECT_EQ(e3->Title(), e2->Title());
  EXPECT_EQ(e3->CreationTime(), e2->CreationTime());
}

TEST(ReadingListEntry, ReadState) {
  auto e = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com"), "bar", base::Time::FromTimeT(10));
  EXPECT_FALSE(e->HasBeenSeen());
  EXPECT_FALSE(e->IsRead());
  e->SetRead(false, base::Time::FromTimeT(20));
  EXPECT_EQ(e->CreationTime(), 10 * base::Time::kMicrosecondsPerSecond);
  EXPECT_EQ(e->UpdateTime(), 10 * base::Time::kMicrosecondsPerSecond);
  EXPECT_EQ(e->UpdateTitleTime(), 10 * base::Time::kMicrosecondsPerSecond);
  EXPECT_TRUE(e->HasBeenSeen());
  EXPECT_FALSE(e->IsRead());
  e->SetRead(true, base::Time::FromTimeT(30));
  EXPECT_EQ(e->CreationTime(), 10 * base::Time::kMicrosecondsPerSecond);
  EXPECT_EQ(e->UpdateTime(), 30 * base::Time::kMicrosecondsPerSecond);
  EXPECT_EQ(e->UpdateTitleTime(), 10 * base::Time::kMicrosecondsPerSecond);
  EXPECT_TRUE(e->HasBeenSeen());
  EXPECT_TRUE(e->IsRead());
}

TEST(ReadingListEntry, SpecificsShouldBeValid) {
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com/"), "example title", base::Time::FromTimeT(10));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry->AsReadingListSpecifics();
  EXPECT_TRUE(ReadingListEntry::IsSpecificsValid(*specifics));
}

TEST(ReadingListEntry, SpecificsWithEmptyIdShouldBeNotValid) {
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com/"), "example title", base::Time::FromTimeT(10));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry->AsReadingListSpecifics();
  specifics->set_entry_id("");
  EXPECT_FALSE(ReadingListEntry::IsSpecificsValid(*specifics));
}

TEST(ReadingListEntry, SpecificsWithEmptyUrlShouldBeNotValid) {
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com/"), "example title", base::Time::FromTimeT(10));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry->AsReadingListSpecifics();
  specifics->set_url("");
  EXPECT_FALSE(ReadingListEntry::IsSpecificsValid(*specifics));
}

TEST(ReadingListEntry, SpecificsWithUnequalEntryIdandUrlShouldBeNotValid) {
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com/"), "example title", base::Time::FromTimeT(10));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry->AsReadingListSpecifics();
  specifics->set_entry_id("http://UnequalEntryIdAndUrl.com/");
  EXPECT_FALSE(ReadingListEntry::IsSpecificsValid(*specifics));
}

TEST(ReadingListEntry, SpecificsWithInvalidUrlShouldBeNotValid) {
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com/"), "example title", base::Time::FromTimeT(10));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry->AsReadingListSpecifics();
  specifics->set_url("InvalidUrl");
  EXPECT_FALSE(ReadingListEntry::IsSpecificsValid(*specifics));
}

TEST(ReadingListEntry, SpecificsWithTilteContainsNonUTF8ShouldBeNotValid) {
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com/"), "example title", base::Time::FromTimeT(10));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry->AsReadingListSpecifics();
  specifics->set_title("\xFC\x9C\xBF\x80\xBF\x80");
  EXPECT_FALSE(ReadingListEntry::IsSpecificsValid(*specifics));
}

TEST(ReadingListEntry, UpdateTitle) {
  auto e = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com"), "bar", base::Time::FromTimeT(10));
  EXPECT_EQ("bar", e->Title());
  // Getters are in microseconds.
  EXPECT_EQ(e->CreationTime(), 10 * base::Time::kMicrosecondsPerSecond);
  EXPECT_EQ(e->UpdateTitleTime(), 10 * base::Time::kMicrosecondsPerSecond);

  e->SetTitle("foo", base::Time::FromTimeT(15));
  EXPECT_EQ(e->UpdateTitleTime(), 15 * base::Time::kMicrosecondsPerSecond);
  EXPECT_EQ("foo", e->Title());
}

TEST(ReadingListEntry, DistilledInfo) {
  auto e = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com"), "bar", base::Time::FromTimeT(10));

  EXPECT_TRUE(e->DistilledPath().empty());

  const base::FilePath distilled_path(FILE_PATH_LITERAL("distilled/page.html"));
  const GURL distilled_url("http://example.com/distilled");
  int64_t size = 50;
  int64_t time = 100;
  e->SetDistilledInfo(distilled_path, distilled_url, size,
                      base::Time::FromTimeT(time));
  EXPECT_EQ(distilled_path, e->DistilledPath());
  EXPECT_EQ(distilled_url, e->DistilledURL());
  EXPECT_EQ(size, e->DistillationSize());
  EXPECT_EQ(e->DistillationTime(), time * base::Time::kMicrosecondsPerSecond);
}

TEST(ReadingListEntry, DistilledState) {
  auto e = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com"), "bar", base::Time::FromTimeT(10));

  EXPECT_EQ(ReadingListEntry::WAITING, e->DistilledState());

  e->SetDistilledState(ReadingListEntry::DISTILLATION_ERROR);
  EXPECT_EQ(ReadingListEntry::DISTILLATION_ERROR, e->DistilledState());

  const base::FilePath distilled_path(FILE_PATH_LITERAL("distilled/page.html"));
  const GURL distilled_url("http://example.com/distilled");
  e->SetDistilledInfo(distilled_path, distilled_url, 50,
                      base::Time::FromTimeT(100));
  EXPECT_EQ(ReadingListEntry::PROCESSED, e->DistilledState());
}

// Tests that the the time until next try increase exponentially when the state
// changes from non-error to error.
TEST(ReadingListEntry, TimeUntilNextTry) {
  base::SimpleTestTickClock clock;
  std::unique_ptr<net::BackoffEntry> backoff =
      std::make_unique<net::BackoffEntry>(&ReadingListEntry::kBackoffPolicy,
                                          &clock);

  auto e = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com"), "bar", base::Time::FromTimeT(10),
      std::move(backoff));

  // Allow twice the jitter as test is not instantaneous.
  double fuzzing = 2 * ReadingListEntry::kBackoffPolicy.jitter_factor;

  EXPECT_EQ(0, e->TimeUntilNextTry().InSeconds());

  // First error.
  e->SetDistilledState(ReadingListEntry::DISTILLATION_ERROR);
  int nextTry = e->TimeUntilNextTry().InMinutes();
  EXPECT_NEAR(kFirstBackoff, nextTry, kFirstBackoff * fuzzing);
  e->SetDistilledState(ReadingListEntry::WILL_RETRY);
  EXPECT_EQ(nextTry, e->TimeUntilNextTry().InMinutes());

  e->SetDistilledState(ReadingListEntry::PROCESSING);
  EXPECT_EQ(nextTry, e->TimeUntilNextTry().InMinutes());

  // Second error.
  e->SetDistilledState(ReadingListEntry::WILL_RETRY);
  nextTry = e->TimeUntilNextTry().InMinutes();
  EXPECT_NEAR(kSecondBackoff, nextTry, kSecondBackoff * fuzzing);
  e->SetDistilledState(ReadingListEntry::DISTILLATION_ERROR);
  EXPECT_EQ(nextTry, e->TimeUntilNextTry().InMinutes());

  e->SetDistilledState(ReadingListEntry::PROCESSING);
  EXPECT_EQ(nextTry, e->TimeUntilNextTry().InMinutes());

  // Third error.
  e->SetDistilledState(ReadingListEntry::WILL_RETRY);
  EXPECT_NEAR(kThirdBackoff, e->TimeUntilNextTry().InMinutes(),
              kThirdBackoff * fuzzing);

  // Fourth error.
  e->SetDistilledState(ReadingListEntry::PROCESSING);
  e->SetDistilledState(ReadingListEntry::DISTILLATION_ERROR);
  EXPECT_NEAR(kFourthBackoff, e->TimeUntilNextTry().InMinutes(),
              kFourthBackoff * fuzzing);

  // Fifth error.
  e->SetDistilledState(ReadingListEntry::PROCESSING);
  e->SetDistilledState(ReadingListEntry::DISTILLATION_ERROR);
  EXPECT_NEAR(kFifthBackoff, e->TimeUntilNextTry().InMinutes(),
              kFifthBackoff * fuzzing);
}

// Tests that if the time until next try is in the past, 0 is returned.
TEST(ReadingListEntry, TimeUntilNextTryInThePast) {
  // Setup.
  base::SimpleTestTickClock clock;
  std::unique_ptr<net::BackoffEntry> backoff =
      std::make_unique<net::BackoffEntry>(&ReadingListEntry::kBackoffPolicy,
                                          &clock);
  auto e = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com"), "bar", base::Time::FromTimeT(10),
      std::move(backoff));
  double fuzzing = ReadingListEntry::kBackoffPolicy.jitter_factor;

  e->SetDistilledState(ReadingListEntry::DISTILLATION_ERROR);
  ASSERT_NEAR(kFirstBackoff, e->TimeUntilNextTry().InMinutes(),
              kFirstBackoff * fuzzing);

  // Action.
  clock.Advance(base::Minutes(kFirstBackoff * 2));

  // Test.
  EXPECT_EQ(0, e->TimeUntilNextTry().InMilliseconds());
}

// Tests that if the entry gets a distilled URL, 0 is returned.
TEST(ReadingListEntry, ResetTimeUntilNextTry) {
  // Setup.
  base::SimpleTestTickClock clock;
  std::unique_ptr<net::BackoffEntry> backoff =
      std::make_unique<net::BackoffEntry>(&ReadingListEntry::kBackoffPolicy,
                                          &clock);
  auto e = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com"), "bar", base::Time::FromTimeT(10),
      std::move(backoff));
  double fuzzing = ReadingListEntry::kBackoffPolicy.jitter_factor;

  e->SetDistilledState(ReadingListEntry::DISTILLATION_ERROR);
  ASSERT_NEAR(kFirstBackoff, e->TimeUntilNextTry().InMinutes(),
              kFirstBackoff * fuzzing);

  // Action.
  const base::FilePath distilled_path(FILE_PATH_LITERAL("distilled/page.html"));
  const GURL distilled_url("http://example.com/distilled");
  e->SetDistilledInfo(distilled_path, distilled_url, 50,
                      base::Time::FromTimeT(100));

  // Test.
  EXPECT_EQ(0, e->TimeUntilNextTry().InSeconds());
  e->SetDistilledState(ReadingListEntry::DISTILLATION_ERROR);
  ASSERT_NEAR(kFirstBackoff, e->TimeUntilNextTry().InMinutes(),
              kFirstBackoff * fuzzing);
}

// Tests that the failed download counter is incremented when the state change
// from non-error to error.
TEST(ReadingListEntry, FailedDownloadCounter) {
  auto e = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com"), "bar", base::Time::FromTimeT(10));

  EXPECT_EQ(0, e->FailedDownloadCounter());

  e->SetDistilledState(ReadingListEntry::DISTILLATION_ERROR);
  EXPECT_EQ(1, e->FailedDownloadCounter());
  e->SetDistilledState(ReadingListEntry::WILL_RETRY);
  EXPECT_EQ(1, e->FailedDownloadCounter());

  e->SetDistilledState(ReadingListEntry::PROCESSING);
  EXPECT_EQ(1, e->FailedDownloadCounter());

  e->SetDistilledState(ReadingListEntry::WILL_RETRY);
  EXPECT_EQ(2, e->FailedDownloadCounter());
  e->SetDistilledState(ReadingListEntry::DISTILLATION_ERROR);
  EXPECT_EQ(2, e->FailedDownloadCounter());
}

// Tests that the reading list entry is correctly encoded to
// sync_pb::ReadingListSpecifics.
TEST(ReadingListEntry, AsReadingListSpecifics) {
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com"), "bar", base::Time::FromTimeT(10));
  int64_t creation_time_us = entry->UpdateTime();

  std::unique_ptr<sync_pb::ReadingListSpecifics> pb_entry(
      entry->AsReadingListSpecifics());
  EXPECT_EQ(pb_entry->entry_id(), "http://example.com/");
  EXPECT_EQ(pb_entry->url(), "http://example.com/");
  EXPECT_EQ(pb_entry->title(), "bar");
  EXPECT_EQ(pb_entry->creation_time_us(), creation_time_us);
  EXPECT_EQ(pb_entry->update_time_us(), entry->UpdateTime());
  EXPECT_EQ(pb_entry->status(), sync_pb::ReadingListSpecifics::UNSEEN);

  entry->SetRead(true, base::Time::FromTimeT(15));
  // Getters are in microseconds.
  EXPECT_EQ(entry->CreationTime(), 10 * base::Time::kMicrosecondsPerSecond);
  EXPECT_EQ(entry->UpdateTime(), 15 * base::Time::kMicrosecondsPerSecond);
  std::unique_ptr<sync_pb::ReadingListSpecifics> updated_pb_entry(
      entry->AsReadingListSpecifics());
  EXPECT_EQ(updated_pb_entry->creation_time_us(), creation_time_us);
  EXPECT_EQ(updated_pb_entry->update_time_us(), entry->UpdateTime());
  EXPECT_EQ(updated_pb_entry->status(), sync_pb::ReadingListSpecifics::READ);
}

// Tests that the reading list entry is correctly parsed from
// sync_pb::ReadingListSpecifics.
TEST(ReadingListEntry, FromReadingListValidSpecifics) {
  std::unique_ptr<sync_pb::ReadingListSpecifics> pb_entry =
      std::make_unique<sync_pb::ReadingListSpecifics>();
  pb_entry->set_entry_id("http://example.com/");
  pb_entry->set_url("http://example.com/");
  pb_entry->set_title("title");
  pb_entry->set_creation_time_us(1);
  pb_entry->set_update_time_us(2);
  pb_entry->set_update_title_time_us(3);
  pb_entry->set_status(sync_pb::ReadingListSpecifics::UNREAD);

  ASSERT_TRUE(ReadingListEntry::IsSpecificsValid(*pb_entry));
  scoped_refptr<ReadingListEntry> entry(
      ReadingListEntry::FromReadingListValidSpecifics(
          *pb_entry, base::Time::FromTimeT(10)));
  EXPECT_EQ(entry->URL().spec(), "http://example.com/");
  EXPECT_EQ(entry->Title(), "title");
  EXPECT_EQ(entry->CreationTime(), 1);
  EXPECT_EQ(entry->UpdateTime(), 2);
  EXPECT_EQ(entry->UpdateTitleTime(), 3);
  EXPECT_EQ(entry->FailedDownloadCounter(), 0);
}

// Tests that the reading list entry is correctly encoded to
// reading_list::ReadingListLocal.
TEST(ReadingListEntry, AsReadingListLocal) {
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com/"), "foo", base::Time::FromTimeT(10));
  int64_t creation_time_us = entry->UpdateTime();
  entry->SetTitle("bar", base::Time::FromTimeT(20));
  entry->MarkEntryUpdated(base::Time::FromTimeT(30));

  std::unique_ptr<reading_list::ReadingListLocal> pb_entry(
      entry->AsReadingListLocal(base::Time::FromTimeT(40)));
  EXPECT_EQ(pb_entry->entry_id(), "http://example.com/");
  EXPECT_EQ(pb_entry->url(), "http://example.com/");
  EXPECT_EQ(pb_entry->title(), "bar");
  EXPECT_EQ(pb_entry->creation_time_us(), creation_time_us);
  EXPECT_EQ(pb_entry->update_time_us(), entry->UpdateTime());
  EXPECT_EQ(pb_entry->status(), reading_list::ReadingListLocal::UNSEEN);
  EXPECT_EQ(pb_entry->distillation_state(),
            reading_list::ReadingListLocal::WAITING);
  EXPECT_EQ(pb_entry->distilled_path(), "");
  EXPECT_EQ(pb_entry->failed_download_counter(), 0);
  EXPECT_NE(pb_entry->backoff(), "");

  entry->SetDistilledState(ReadingListEntry::WILL_RETRY);
  std::unique_ptr<reading_list::ReadingListLocal> will_retry_pb_entry(
      entry->AsReadingListLocal(base::Time::FromTimeT(50)));
  EXPECT_EQ(will_retry_pb_entry->distillation_state(),
            reading_list::ReadingListLocal::WILL_RETRY);
  EXPECT_EQ(will_retry_pb_entry->failed_download_counter(), 1);

  const base::FilePath distilled_path(FILE_PATH_LITERAL("distilled/page.html"));
  const GURL distilled_url("http://example.com/distilled");
  int64_t size = 50;
  entry->SetDistilledInfo(distilled_path, distilled_url, size,
                          base::Time::FromTimeT(100));

  entry->SetRead(true, base::Time::FromTimeT(20));
  entry->MarkEntryUpdated(base::Time::FromTimeT(30));

  EXPECT_NE(entry->UpdateTime(), creation_time_us);
  std::unique_ptr<reading_list::ReadingListLocal> distilled_pb_entry(
      entry->AsReadingListLocal(base::Time::FromTimeT(40)));
  EXPECT_EQ(distilled_pb_entry->creation_time_us(), creation_time_us);
  EXPECT_EQ(distilled_pb_entry->update_time_us(), entry->UpdateTime());
  EXPECT_NE(distilled_pb_entry->backoff(), "");
  EXPECT_EQ(distilled_pb_entry->status(), reading_list::ReadingListLocal::READ);
  EXPECT_EQ(distilled_pb_entry->distillation_state(),
            reading_list::ReadingListLocal::PROCESSED);
  EXPECT_EQ(distilled_pb_entry->distilled_path(), "distilled/page.html");
  EXPECT_EQ(distilled_pb_entry->failed_download_counter(), 0);
  EXPECT_EQ(distilled_pb_entry->distillation_time_us(),
            entry->DistillationTime());
  EXPECT_EQ(distilled_pb_entry->distillation_size(), entry->DistillationSize());
}

// Tests that the reading list entry is correctly parsed from
// sync_pb::ReadingListLocal.
TEST(ReadingListEntry, FromReadingListLocal) {
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com/"), "title", base::Time::FromTimeT(10));
  entry->SetDistilledState(ReadingListEntry::DISTILLATION_ERROR);

  std::unique_ptr<reading_list::ReadingListLocal> pb_entry(
      entry->AsReadingListLocal(base::Time::FromTimeT(10)));
  int64_t now = 12345;

  pb_entry->set_entry_id("http://example.com/");
  pb_entry->set_url("http://example.com/");
  pb_entry->set_title("title");
  pb_entry->set_creation_time_us(1);
  pb_entry->set_update_time_us(2);
  pb_entry->set_update_title_time_us(3);
  pb_entry->set_status(reading_list::ReadingListLocal::UNREAD);
  pb_entry->set_distillation_state(reading_list::ReadingListLocal::WAITING);
  pb_entry->set_failed_download_counter(2);
  pb_entry->set_distillation_time_us(now);
  pb_entry->set_distillation_size(50);

  scoped_refptr<ReadingListEntry> waiting_entry(
      ReadingListEntry::FromReadingListLocal(*pb_entry,
                                             base::Time::FromTimeT(20)));
  EXPECT_EQ(waiting_entry->URL().spec(), "http://example.com/");
  EXPECT_EQ(waiting_entry->Title(), "title");
  EXPECT_EQ(waiting_entry->UpdateTime(), 2);
  EXPECT_EQ(waiting_entry->UpdateTitleTime(), 3);
  EXPECT_EQ(waiting_entry->FailedDownloadCounter(), 2);
  EXPECT_EQ(waiting_entry->DistilledState(), ReadingListEntry::WAITING);
  EXPECT_EQ(waiting_entry->DistilledPath(), base::FilePath());
  EXPECT_EQ(waiting_entry->DistillationSize(), 50);
  EXPECT_EQ(waiting_entry->DistillationTime(), now);
  // Allow twice the jitter as test is not instantaneous.
  double fuzzing = 2 * ReadingListEntry::kBackoffPolicy.jitter_factor;
  int nextTry = waiting_entry->TimeUntilNextTry().InMinutes();
  EXPECT_NEAR(kFirstBackoff, nextTry, kFirstBackoff * fuzzing);
}

// Tests the merging of two ReadingListEntry.
// Additional merging tests are done in
// ReadingListSyncBridgeTest.CompareEntriesForSync
TEST(ReadingListEntry, MergeWithEntry) {
  auto local_entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com/"), "title", base::Time::FromTimeT(10));
  local_entry->SetDistilledState(ReadingListEntry::DISTILLATION_ERROR);
  local_entry->SetTitle("title updated", base::Time::FromTimeT(30));
  int64_t local_update_time_us = local_entry->UpdateTime();

  auto sync_entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com/"), "title2", base::Time::FromTimeT(20));
  sync_entry->SetDistilledState(ReadingListEntry::DISTILLATION_ERROR);
  int64_t sync_update_time_us = sync_entry->UpdateTime();
  EXPECT_NE(local_update_time_us, sync_update_time_us);
  local_entry->MergeWithEntry(*sync_entry);
  EXPECT_EQ(local_entry->URL().spec(), "http://example.com/");
  EXPECT_EQ(local_entry->Title(), "title updated");
  EXPECT_EQ(local_entry->UpdateTitleTime(),
            30 * base::Time::kMicrosecondsPerSecond);
  EXPECT_FALSE(local_entry->HasBeenSeen());
  EXPECT_EQ(local_entry->UpdateTime(), sync_update_time_us);
  EXPECT_EQ(local_entry->FailedDownloadCounter(), 1);
  EXPECT_EQ(local_entry->DistilledState(),
            ReadingListEntry::DISTILLATION_ERROR);
  // Allow twice the jitter as test is not instantaneous.
  double fuzzing = 2 * ReadingListEntry::kBackoffPolicy.jitter_factor;
  int nextTry = local_entry->TimeUntilNextTry().InMinutes();
  EXPECT_NEAR(kFirstBackoff, nextTry, kFirstBackoff * fuzzing);
}
