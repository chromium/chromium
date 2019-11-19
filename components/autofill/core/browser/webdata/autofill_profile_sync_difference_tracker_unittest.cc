// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_profile_sync_difference_tracker.h"

#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_profile_sync_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/model/model_error.h"
#include "components/webdata/common/web_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using base::ASCIIToUTF16;
using base::MockCallback;
using testing::ElementsAre;
using testing::IsEmpty;

// Some guids for testing.
const char kGuidA[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44A";
const char kGuidB[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
const char kHttpOrigin[] = "http://www.example.com/";
const char kHttpsOrigin[] = "https://www.example.com/";
const char kLocaleString[] = "en-US";
const base::Time kJune2017 = base::Time::FromDoubleT(1497552271);

}  // namespace

class AutofillProfileSyncDifferenceTrackerTestBase : public testing::Test {
 public:
  AutofillProfileSyncDifferenceTrackerTestBase() {}
  ~AutofillProfileSyncDifferenceTrackerTestBase() override {}

  void SetUp() override {
    // Fix a time for implicitly constructed use_dates in AutofillProfile.
    test_clock_.SetNow(kJune2017);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
  }

  void AddAutofillProfilesToTable(
      const std::vector<AutofillProfile>& profile_list) {
    for (const auto& profile : profile_list) {
      table_.AddAutofillProfile(profile);
    }
  }

  void IncorporateRemoteProfile(const AutofillProfile& profile) {
    EXPECT_EQ(base::nullopt, tracker()->IncorporateRemoteProfile(
                                 std::make_unique<AutofillProfile>(profile)));
  }

  std::vector<AutofillProfile> FlushAndReturnProfilesToUploadToSync() {
    EXPECT_EQ(base::nullopt,
              tracker()->FlushToLocal(
                  /*autofill_changes_callback=*/base::DoNothing()));

    std::vector<std::unique_ptr<AutofillProfile>> vector_of_unique_ptrs;
    EXPECT_EQ(base::nullopt,
              tracker()->FlushToSync(
                  /*profiles_to_upload_to_sync=*/&vector_of_unique_ptrs));

    // Copy all the elements by value so that we have a vector that is easier to
    // work with in the test.
    std::vector<AutofillProfile> vector_of_values;
    for (const std::unique_ptr<AutofillProfile>& entry :
         vector_of_unique_ptrs) {
      vector_of_values.push_back(*entry);
    }
    return vector_of_values;
  }

  std::vector<AutofillProfile> GetAllLocalData() {
    std::vector<std::unique_ptr<AutofillProfile>> vector_of_unique_ptrs;
    // Meant as an assertion but I cannot use ASSERT_TRUE in non-void function.
    EXPECT_TRUE(table()->GetAutofillProfiles(&vector_of_unique_ptrs));

    // Copy all the elements by value so that we have a vector that is easier to
    // work with in the test.
    std::vector<AutofillProfile> local_data;
    for (const std::unique_ptr<AutofillProfile>& entry :
         vector_of_unique_ptrs) {
      local_data.push_back(*entry);
    }
    return local_data;
  }

  virtual AutofillProfileSyncDifferenceTracker* tracker() = 0;

  AutofillTable* table() { return &table_; }

 private:
  autofill::TestAutofillClock test_clock_;
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  AutofillTable table_;
  WebDatabase db_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileSyncDifferenceTrackerTestBase);
};

class AutofillProfileSyncDifferenceTrackerTest
    : public AutofillProfileSyncDifferenceTrackerTestBase {
 public:
  AutofillProfileSyncDifferenceTrackerTest() : tracker_(table()) {}
  ~AutofillProfileSyncDifferenceTrackerTest() override {}

  AutofillProfileSyncDifferenceTracker* tracker() override { return &tracker_; }

 private:
  AutofillProfileSyncDifferenceTracker tracker_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileSyncDifferenceTrackerTest);
};

TEST_F(AutofillProfileSyncDifferenceTrackerTest,
       IncorporateRemoteProfileShouldOverwriteProfileWithSameKey) {
  AutofillProfile local = AutofillProfile(kGuidA, kHttpOrigin);
  local.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  AddAutofillProfilesToTable({local});

  // The remote profile is completely different but it has the same key.
  AutofillProfile remote = AutofillProfile(kGuidA, kHttpsOrigin);
  remote.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));

  IncorporateRemoteProfile(remote);

  // Nothing gets uploaded to sync and the remote profile wins.
  EXPECT_THAT(FlushAndReturnProfilesToUploadToSync(), IsEmpty());
  EXPECT_THAT(GetAllLocalData(), ElementsAre(remote));
}

TEST_F(AutofillProfileSyncDifferenceTrackerTest,
       IncorporateRemoteProfileShouldOverwriteUnverifiedProfileByVerified) {
  AutofillProfile local = AutofillProfile(kGuidA, kHttpsOrigin);
  local.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  AddAutofillProfilesToTable({local});

  // The remote profile has the same key but it is not verified.
  AutofillProfile remote = AutofillProfile(kGuidA, kSettingsOrigin);
  remote.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));

  IncorporateRemoteProfile(remote);

  // Nothing gets uploaded to sync and the local profile wins.
  EXPECT_THAT(FlushAndReturnProfilesToUploadToSync(), IsEmpty());
  EXPECT_THAT(GetAllLocalData(), ElementsAre(remote));
}

TEST_F(AutofillProfileSyncDifferenceTrackerTest,
       IncorporateRemoteProfileShouldNotOverwriteVerifiedProfileByUnverified) {
  AutofillProfile local = AutofillProfile(kGuidA, kSettingsOrigin);
  local.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  AddAutofillProfilesToTable({local});

  // The remote profile has the same key but it is not verified.
  AutofillProfile remote = AutofillProfile(kGuidA, kHttpsOrigin);
  remote.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));

  IncorporateRemoteProfile(remote);

  // Nothing gets uploaded to sync and the local profile wins.
  EXPECT_THAT(FlushAndReturnProfilesToUploadToSync(), IsEmpty());
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local));
}

TEST_F(AutofillProfileSyncDifferenceTrackerTest,
       IncorporateRemoteProfileShouldNotOverwriteFullNameByEmptyString) {
  AutofillProfile local = AutofillProfile(kGuidA, kHttpOrigin);
  local.SetRawInfo(NAME_FULL, ASCIIToUTF16("John"));
  AddAutofillProfilesToTable({local});

  // The remote profile has the same key.
  AutofillProfile remote = AutofillProfile(kGuidA, kHttpsOrigin);
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("2 2st st"));

  AutofillProfile merged(remote);
  merged.SetRawInfo(NAME_FULL, ASCIIToUTF16("John"));

  IncorporateRemoteProfile(remote);

  // Nothing gets uploaded to sync and the remote profile wins except for the
  // full name.
  EXPECT_THAT(FlushAndReturnProfilesToUploadToSync(), IsEmpty());
  EXPECT_THAT(GetAllLocalData(), ElementsAre(merged));
}

TEST_F(AutofillProfileSyncDifferenceTrackerTest,
       IncorporateRemoteProfileShouldMergeIdenticalProfilesWithDifferentKeys) {
  AutofillProfile local = AutofillProfile(kGuidA, kHttpOrigin);
  local.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  AddAutofillProfilesToTable({local});

  // The remote profile is identical to the local one, except that the guids and
  // origins are different.
  AutofillProfile remote = AutofillProfile(kGuidB, kHttpsOrigin);
  remote.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));

  IncorporateRemoteProfile(remote);

  // Nothing gets uploaded to sync and the remote profile wins.
  EXPECT_THAT(FlushAndReturnProfilesToUploadToSync(), IsEmpty());
  EXPECT_THAT(GetAllLocalData(), ElementsAre(remote));
}

TEST_F(
    AutofillProfileSyncDifferenceTrackerTest,
    IncorporateRemoteProfileShouldMergeIdenticalProfilesWithDifferentKeysButKeepVerifiedOrigin) {
  AutofillProfile local = AutofillProfile(kGuidA, kSettingsOrigin);
  local.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  AddAutofillProfilesToTable({local});

  // The remote profile has the same key.
  AutofillProfile remote = AutofillProfile(kGuidB, kHttpsOrigin);
  remote.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));

  AutofillProfile merged(remote);
  merged.set_origin(kSettingsOrigin);

  IncorporateRemoteProfile(remote);

  // Nothing gets uploaded to sync and the remote profile wins except for the
  // full name.
  EXPECT_THAT(FlushAndReturnProfilesToUploadToSync(), ElementsAre(merged));
  EXPECT_THAT(GetAllLocalData(), ElementsAre(merged));
}

TEST_F(AutofillProfileSyncDifferenceTrackerTest,
       FlushToLocalShouldNotCallbackWhenNotNeeded) {
  MockCallback<base::OnceClosure> autofill_changes_callback;

  EXPECT_CALL(autofill_changes_callback, Run()).Times(0);
  EXPECT_EQ(base::nullopt,
            tracker()->FlushToLocal(autofill_changes_callback.Get()));
}

TEST_F(AutofillProfileSyncDifferenceTrackerTest,
       FlushToLocalShouldCallbackWhenProfileDeleted) {
  AutofillProfile local = AutofillProfile(kGuidA, kSettingsOrigin);
  AddAutofillProfilesToTable({local});

  tracker()->IncorporateRemoteDelete(kGuidA);

  MockCallback<base::OnceClosure> autofill_changes_callback;
  EXPECT_CALL(autofill_changes_callback, Run()).Times(1);
  EXPECT_EQ(base::nullopt,
            tracker()->FlushToLocal(autofill_changes_callback.Get()));

  // On top of that, the profile should also get deleted.
  EXPECT_THAT(GetAllLocalData(), IsEmpty());
}

TEST_F(AutofillProfileSyncDifferenceTrackerTest,
       FlushToLocalShouldCallbackWhenProfileAdded) {
  AutofillProfile remote = AutofillProfile(kGuidA, kSettingsOrigin);
  IncorporateRemoteProfile(remote);

  MockCallback<base::OnceClosure> autofill_changes_callback;
  EXPECT_CALL(autofill_changes_callback, Run()).Times(1);
  EXPECT_EQ(base::nullopt,
            tracker()->FlushToLocal(autofill_changes_callback.Get()));

  // On top of that, the profile should also get added.
  EXPECT_THAT(GetAllLocalData(), ElementsAre(remote));
}

TEST_F(AutofillProfileSyncDifferenceTrackerTest,
       FlushToLocalShouldCallbackWhenProfileUpdated) {
  AutofillProfile local = AutofillProfile(kGuidA, kHttpsOrigin);
  AddAutofillProfilesToTable({local});

  AutofillProfile remote = AutofillProfile(kGuidA, kHttpsOrigin);
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  IncorporateRemoteProfile(remote);

  MockCallback<base::OnceClosure> autofill_changes_callback;
  EXPECT_CALL(autofill_changes_callback, Run()).Times(1);
  EXPECT_EQ(base::nullopt,
            tracker()->FlushToLocal(autofill_changes_callback.Get()));

  // On top of that, the profile with key kGuidA should also get updated.
  EXPECT_THAT(GetAllLocalData(), ElementsAre(remote));
}

class AutofillProfileInitialSyncDifferenceTrackerTest
    : public AutofillProfileSyncDifferenceTrackerTestBase {
 public:
  AutofillProfileInitialSyncDifferenceTrackerTest()
      : initial_tracker_(table()) {}
  ~AutofillProfileInitialSyncDifferenceTrackerTest() override {}

  void MergeSimilarEntriesForInitialSync() {
    initial_tracker_.MergeSimilarEntriesForInitialSync(kLocaleString);
  }

  AutofillProfileSyncDifferenceTracker* tracker() override {
    return &initial_tracker_;
  }

 private:
  AutofillProfileInitialSyncDifferenceTracker initial_tracker_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileInitialSyncDifferenceTrackerTest);
};

TEST_F(AutofillProfileInitialSyncDifferenceTrackerTest,
       MergeSimilarEntriesForInitialSyncShouldSyncUpChanges) {
  AutofillProfile local = AutofillProfile(kGuidA, kHttpOrigin);
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  local.set_use_count(27);
  AddAutofillProfilesToTable({local});

  // The remote profile matches the local one (except for origin and use count).
  AutofillProfile remote = AutofillProfile(kGuidB, kHttpsOrigin);
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  remote.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Frobbers, Inc."));
  remote.set_use_count(13);

  // The remote profile wins (as regards the storage key).
  AutofillProfile merged(remote);
  // The local origin wins when merging.
  merged.set_origin(kHttpOrigin);
  // Merging two profile takes their max use count.
  merged.set_use_count(27);

  IncorporateRemoteProfile(remote);
  MergeSimilarEntriesForInitialSync();

  // The merged profile needs to get uploaded back to sync and stored locally.
  EXPECT_THAT(FlushAndReturnProfilesToUploadToSync(), ElementsAre(merged));
  EXPECT_THAT(GetAllLocalData(), ElementsAre(merged));
}

TEST_F(AutofillProfileInitialSyncDifferenceTrackerTest,
       MergeSimilarEntriesForInitialSyncShouldNotSyncUpWhenNotNeeded) {
  AutofillProfile local = AutofillProfile(kGuidA, kHttpOrigin);
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  local.set_use_count(13);
  AddAutofillProfilesToTable({local});

  // The remote profile matches the local one and has some additional data.
  AutofillProfile remote = AutofillProfile(kGuidB, kHttpOrigin);
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  remote.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Frobbers, Inc."));
  // Merging two profile takes their max use count, so use count of 27 is taken.
  remote.set_use_count(27);

  IncorporateRemoteProfile(remote);
  MergeSimilarEntriesForInitialSync();

  // Nothing gets uploaded to sync and the remote profile wins.
  EXPECT_THAT(FlushAndReturnProfilesToUploadToSync(), IsEmpty());
  EXPECT_THAT(GetAllLocalData(), ElementsAre(remote));
}

TEST_F(AutofillProfileInitialSyncDifferenceTrackerTest,
       MergeSimilarEntriesForInitialSyncNotMatchNonsimilarEntries) {
  AutofillProfile local = AutofillProfile(kGuidA, kHttpOrigin);
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  local.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Frobbers, Inc."));
  AddAutofillProfilesToTable({local});

  // The remote profile has a different street address.
  AutofillProfile remote = AutofillProfile(kGuidB, kHttpOrigin);
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("2 2st st"));
  remote.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Frobbers, Inc."));

  IncorporateRemoteProfile(remote);
  MergeSimilarEntriesForInitialSync();

  // The local profile gets uploaded (due to initial sync) and the remote
  // profile gets stored locally.
  EXPECT_THAT(FlushAndReturnProfilesToUploadToSync(), ElementsAre(local));
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local, remote));
}

TEST_F(AutofillProfileInitialSyncDifferenceTrackerTest,
       MergeSimilarEntriesForInitialSyncDoesNotMatchLocalVerifiedEntry) {
  // The local entry is verified, should not get merged.
  AutofillProfile local = AutofillProfile(kGuidA, kSettingsOrigin);
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  AddAutofillProfilesToTable({local});

  // The remote profile is similar to the local one.
  AutofillProfile remote = AutofillProfile(kGuidB, kHttpOrigin);
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  remote.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Frobbers, Inc."));

  IncorporateRemoteProfile(remote);
  MergeSimilarEntriesForInitialSync();

  // The local profile gets uploaded (due to initial sync) and the remote
  // profile gets stored locally.
  EXPECT_THAT(FlushAndReturnProfilesToUploadToSync(), ElementsAre(local));
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local, remote));
}

TEST_F(AutofillProfileInitialSyncDifferenceTrackerTest,
       MergeSimilarEntriesForInitialSyncDoesNotMatchRemoteVerifiedEntry) {
  AutofillProfile local = AutofillProfile(kGuidA, kHttpOrigin);
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  AddAutofillProfilesToTable({local});

  // The remote profile is similar to the local one but is verified and thus it
  // should not get merged.
  AutofillProfile remote = AutofillProfile(kGuidB, kSettingsOrigin);
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  remote.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Frobbers, Inc."));

  IncorporateRemoteProfile(remote);
  MergeSimilarEntriesForInitialSync();

  // The local profile gets uploaded (due to initial sync) and the remote
  // profile gets stored locally.
  EXPECT_THAT(FlushAndReturnProfilesToUploadToSync(), ElementsAre(local));
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local, remote));
}

}  // namespace autofill
