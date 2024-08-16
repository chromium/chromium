// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/cycle/sync_cycle_snapshot.h"

#include "base/i18n/rtl.h"
#include "base/test/icu_test_util.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using base::ExpectDictBooleanValue;
using base::ExpectDictIntegerValue;
using base::ExpectDictStringValue;
using base::ExpectDictValue;

class SyncCycleSnapshotTest : public testing::Test {};

TEST_F(SyncCycleSnapshotTest, SyncCycleSnapshotToValue) {
  // Formatting of "poll_interval" value depends on the current locale.
  // Expectations below use English (US) formatting.
  base::test::ScopedRestoreICUDefaultLocale restore_locale;
  base::i18n::SetICUDefaultLocale("en_US");

  ModelNeutralState model_neutral;
  model_neutral.num_successful_commits = 5;
  model_neutral.num_successful_bookmark_commits = 10;
  model_neutral.num_updates_downloaded_total = 100;
  model_neutral.num_tombstone_updates_downloaded_total = 200;

  ProgressMarkerMap download_progress_markers;
  download_progress_markers[BOOKMARKS] = "\xef\xb7\xa4";
  download_progress_markers[APPS] = "apps";
  base::Value::Dict expected_download_progress_markers_value =
      ProgressMarkerMapToValueDict(download_progress_markers);

  const std::string kBirthday = "test_birthday";
  const std::string kBagOfChips = "bagofchips\1";
  const bool kIsSilenced = true;
  const int kNumServerConflicts = 1057;
  SyncCycleSnapshot snapshot(
      kBirthday, kBagOfChips, model_neutral, download_progress_markers,
      kIsSilenced, kNumServerConflicts, false, base::Time::Now(),
      base::Time::Now(), sync_pb::SyncEnums::UNKNOWN_ORIGIN,
      /*poll_interval=*/base::Minutes(30),
      /*has_remaining_local_changes=*/false);
  base::Value::Dict dict(snapshot.ToValue());
  EXPECT_EQ(14u, dict.size());
  ExpectDictStringValue(kBirthday, dict, "birthday");
  // Base64-encoded version of |kBagOfChips|.
  ExpectDictStringValue("YmFnb2ZjaGlwcwE=", dict, "bagOfChips");
  ExpectDictIntegerValue(model_neutral.num_successful_commits, dict,
                         "numSuccessfulCommits");
  ExpectDictIntegerValue(model_neutral.num_successful_bookmark_commits, dict,
                         "numSuccessfulBookmarkCommits");
  ExpectDictIntegerValue(model_neutral.num_updates_downloaded_total, dict,
                         "numUpdatesDownloadedTotal");
  ExpectDictIntegerValue(model_neutral.num_tombstone_updates_downloaded_total,
                         dict, "numTombstoneUpdatesDownloadedTotal");
  ExpectDictValue(expected_download_progress_markers_value, dict,
                  "downloadProgressMarkers");
  ExpectDictBooleanValue(kIsSilenced, dict, "isSilenced");
  ExpectDictIntegerValue(kNumServerConflicts, dict, "numServerConflicts");
  ExpectDictBooleanValue(false, dict, "notificationsEnabled");
  ExpectDictBooleanValue(false, dict, "hasRemainingLocalChanges");
  ExpectDictStringValue("0h 30m", dict, "poll_interval");
  // poll_finish_time includes the local time zone, so simply verify its
  // existence.
  EXPECT_TRUE(dict.FindString("poll_finish_time"));
}

}  // namespace
}  // namespace syncer
