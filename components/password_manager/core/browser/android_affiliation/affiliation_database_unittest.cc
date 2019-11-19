// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/android_affiliation/affiliation_database.h"

#include <stdint.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

const char kTestFacetURI1[] = "https://alpha.example.com";
const char kTestFacetURI2[] = "https://beta.example.com";
const char kTestFacetURI3[] = "https://gamma.example.com";
const char kTestFacetURI4[] = "https://delta.example.com";
const char kTestFacetURI5[] = "https://epsilon.example.com";
const char kTestFacetURI6[] = "https://zeta.example.com";

const char kTestAndroidFacetURI[] = "android://hash@com.example.android";
const char kTestAndroidPlayName[] = "Test Android App";
const char kTestAndroidIconURL[] = "https://example.com/icon.png";

const int64_t kTestTimeUs1 = 1000000;
const int64_t kTestTimeUs2 = 2000000;
const int64_t kTestTimeUs3 = 3000000;

void ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(
    const AffiliatedFacetsWithUpdateTime& expectation,
    const AffiliatedFacetsWithUpdateTime& reality) {
  EXPECT_EQ(expectation.last_update_time, reality.last_update_time);
  EXPECT_THAT(reality.facets,
              testing::UnorderedElementsAreArray(expectation.facets));
}

AffiliatedFacetsWithUpdateTime TestEquivalenceClass1() {
  AffiliatedFacetsWithUpdateTime affiliation;
  affiliation.last_update_time = base::Time::FromInternalValue(kTestTimeUs1);
  affiliation.facets = {
      {FacetURI::FromCanonicalSpec(kTestFacetURI1)},
      {FacetURI::FromCanonicalSpec(kTestFacetURI2)},
      {FacetURI::FromCanonicalSpec(kTestFacetURI3)},
  };
  return affiliation;
}

AffiliatedFacetsWithUpdateTime TestEquivalenceClass2() {
  AffiliatedFacetsWithUpdateTime affiliation;
  affiliation.last_update_time = base::Time::FromInternalValue(kTestTimeUs2);
  affiliation.facets = {
      {FacetURI::FromCanonicalSpec(kTestFacetURI4)},
      {FacetURI::FromCanonicalSpec(kTestFacetURI5)},
  };
  return affiliation;
}

AffiliatedFacetsWithUpdateTime TestEquivalenceClass3() {
  AffiliatedFacetsWithUpdateTime affiliation;
  affiliation.last_update_time = base::Time::FromInternalValue(kTestTimeUs3);
  affiliation.facets = {
      {FacetURI::FromCanonicalSpec(kTestAndroidFacetURI),
       FacetBrandingInfo{kTestAndroidPlayName, GURL(kTestAndroidIconURL)}},
  };
  return affiliation;
}

}  // namespace

class AffiliationDatabaseTest : public testing::Test {
 public:
  AffiliationDatabaseTest() {}
  ~AffiliationDatabaseTest() override {}

  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    OpenDatabase();
  }

  void OpenDatabase() {
    db_.reset(new AffiliationDatabase);
    ASSERT_TRUE(db_->Init(db_path()));
  }

  void CloseDatabase() { db_.reset(); }

  void StoreInitialTestData() {
    ASSERT_TRUE(db_->Store(TestEquivalenceClass1()));
    ASSERT_TRUE(db_->Store(TestEquivalenceClass2()));
    ASSERT_TRUE(db_->Store(TestEquivalenceClass3()));
  }

  AffiliationDatabase& db() { return *db_; }

  base::FilePath db_path() {
    return temp_directory_.GetPath().Append(
        FILE_PATH_LITERAL("Test Affiliation Database"));
  }

 private:
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<AffiliationDatabase> db_;

  DISALLOW_COPY_AND_ASSIGN(AffiliationDatabaseTest);
};

TEST_F(AffiliationDatabaseTest, Store) {
  LOG(ERROR) << "During this test, SQL errors (number 19) will be logged to "
                "the console. This is expected.";

  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());

  // Verify that duplicate equivalence classes are not allowed to be stored.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CONSTRAINT);
    AffiliatedFacetsWithUpdateTime duplicate = TestEquivalenceClass1();
    EXPECT_FALSE(db().Store(duplicate));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }

  // Verify that intersecting equivalence classes are not allowed to be stored.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CONSTRAINT);
    AffiliatedFacetsWithUpdateTime intersecting;
    intersecting.facets = {
        {FacetURI::FromCanonicalSpec(kTestFacetURI3)},
        {FacetURI::FromCanonicalSpec(kTestFacetURI4)},
    };
    EXPECT_FALSE(db().Store(intersecting));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
}

TEST_F(AffiliationDatabaseTest, GetAllAffiliationsAndBranding) {
  std::vector<AffiliatedFacetsWithUpdateTime> affiliations;

  // Empty database should not return any equivalence classes.
  db().GetAllAffiliationsAndBranding(&affiliations);
  EXPECT_EQ(0u, affiliations.size());

  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());

  // The test data should be returned in order.
  db().GetAllAffiliationsAndBranding(&affiliations);
  ASSERT_EQ(3u, affiliations.size());
  ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(TestEquivalenceClass1(),
                                                        affiliations[0]);
  ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(TestEquivalenceClass2(),
                                                        affiliations[1]);
  ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(TestEquivalenceClass3(),
                                                        affiliations[2]);
}

TEST_F(AffiliationDatabaseTest, GetAffiliationForFacet) {
  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());

  // Verify that querying any element in the first equivalence class yields that
  // class.
  for (const auto& facet : TestEquivalenceClass1().facets) {
    AffiliatedFacetsWithUpdateTime affiliation;
    EXPECT_TRUE(
        db().GetAffiliationsAndBrandingForFacetURI(facet.uri, &affiliation));
    ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(
        TestEquivalenceClass1(), affiliation);
  }

  // Verify that querying the sole element in the third equivalence class yields
  // that class.
  {
    AffiliatedFacetsWithUpdateTime affiliation;
    EXPECT_TRUE(db().GetAffiliationsAndBrandingForFacetURI(
        FacetURI::FromCanonicalSpec(kTestAndroidFacetURI), &affiliation));
    ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(
        TestEquivalenceClass3(), affiliation);
  }

  // Verify that querying a facet not in the database yields no result.
  {
    AffiliatedFacetsWithUpdateTime affiliation;
    EXPECT_FALSE(db().GetAffiliationsAndBrandingForFacetURI(
        FacetURI::FromCanonicalSpec(kTestFacetURI6), &affiliation));
    ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(
        AffiliatedFacetsWithUpdateTime(), affiliation);
  }
}

TEST_F(AffiliationDatabaseTest, StoreAndRemoveConflicting) {
  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());

  AffiliatedFacetsWithUpdateTime updated = TestEquivalenceClass1();
  updated.last_update_time = base::Time::FromInternalValue(4000000);

  // Verify that duplicate equivalence classes are now allowed to be stored, and
  // the last update timestamp is updated.
  {
    std::vector<AffiliatedFacetsWithUpdateTime> removed;
    db().StoreAndRemoveConflicting(updated, &removed);
    EXPECT_EQ(0u, removed.size());

    AffiliatedFacetsWithUpdateTime affiliation;
    EXPECT_TRUE(db().GetAffiliationsAndBrandingForFacetURI(
        FacetURI::FromCanonicalSpec(kTestFacetURI1), &affiliation));
    ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(updated, affiliation);
  }

  // Verify that intersecting equivalence classes are now allowed to be stored,
  // the conflicting classes are removed, but unaffected classes are retained.
  {
    AffiliatedFacetsWithUpdateTime intersecting;
    std::vector<AffiliatedFacetsWithUpdateTime> removed;
    intersecting.last_update_time = base::Time::FromInternalValue(5000000);
    intersecting.facets = {
        {FacetURI::FromCanonicalSpec(kTestFacetURI3)},
        {FacetURI::FromCanonicalSpec(kTestFacetURI4)},
    };
    db().StoreAndRemoveConflicting(intersecting, &removed);

    ASSERT_EQ(2u, removed.size());
    ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(updated, removed[0]);
    ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(
        TestEquivalenceClass2(), removed[1]);

    std::vector<AffiliatedFacetsWithUpdateTime> affiliations;
    db().GetAllAffiliationsAndBranding(&affiliations);
    ASSERT_EQ(2u, affiliations.size());
    ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(
        TestEquivalenceClass3(), affiliations[0]);
    ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(intersecting,
                                                          affiliations[1]);
  }
}

// Verify that an existing DB can be reopened, and data is retained.
TEST_F(AffiliationDatabaseTest, DBRetainsDataAfterReopening) {
  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());

  CloseDatabase();
  OpenDatabase();

  // The test data should be returned in order.
  std::vector<AffiliatedFacetsWithUpdateTime> affiliations;
  db().GetAllAffiliationsAndBranding(&affiliations);
  ASSERT_EQ(3u, affiliations.size());
  ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(TestEquivalenceClass1(),
                                                        affiliations[0]);
  ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(TestEquivalenceClass2(),
                                                        affiliations[1]);
  ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(TestEquivalenceClass3(),
                                                        affiliations[2]);
}

// Verify that when it is discovered during opening that a DB is corrupt, it
// gets razed, and then an empty (but again usable) DB is produced.
TEST_F(AffiliationDatabaseTest, CorruptDBIsRazedThenOpened) {
  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());

  CloseDatabase();
  ASSERT_TRUE(sql::test::CorruptSizeInHeader(db_path()));
  ASSERT_NO_FATAL_FAILURE(OpenDatabase());

  std::vector<AffiliatedFacetsWithUpdateTime> affiliations;
  db().GetAllAffiliationsAndBranding(&affiliations);
  EXPECT_EQ(0u, affiliations.size());

  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());
  db().GetAllAffiliationsAndBranding(&affiliations);
  EXPECT_EQ(3u, affiliations.size());
}

// Verify that when the DB becomes corrupt after it has been opened, it gets
// poisoned so that operations fail silently without side effects.
TEST_F(AffiliationDatabaseTest, CorruptDBGetsPoisoned) {
  ASSERT_TRUE(db().Store(TestEquivalenceClass1()));

  ASSERT_TRUE(sql::test::CorruptSizeInHeader(db_path()));

  EXPECT_FALSE(db().Store(TestEquivalenceClass2()));
  std::vector<AffiliatedFacetsWithUpdateTime> affiliations;
  db().GetAllAffiliationsAndBranding(&affiliations);
  EXPECT_EQ(0u, affiliations.size());
}

// Verify that all files get deleted.
TEST_F(AffiliationDatabaseTest, Delete) {
  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());
  CloseDatabase();

  AffiliationDatabase::Delete(db_path());
  EXPECT_TRUE(base::IsDirectoryEmpty(db_path().DirName()));
}

TEST_F(AffiliationDatabaseTest, MigrateFromVersion1) {
  // Close and delete the current database and create it from scratch with the
  // SQLite statement stored in affiliation_db_v1.sql.
  CloseDatabase();
  AffiliationDatabase::Delete(db_path());
  base::FilePath src_root_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_root_dir));
  base::FilePath sql_path_v1 = src_root_dir.AppendASCII("components")
                                   .AppendASCII("test")
                                   .AppendASCII("data")
                                   .AppendASCII("password_manager")
                                   .AppendASCII("affiliation_db_v1.sql");
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(), sql_path_v1));

  // Re-open the database, triggering the migration.
  OpenDatabase();

  // Check that migration was successful and existing data was untouched.
  EXPECT_EQ(2, db().GetDatabaseVersionForTesting());
  std::vector<AffiliatedFacetsWithUpdateTime> affiliations;
  db().GetAllAffiliationsAndBranding(&affiliations);
  ASSERT_EQ(3u, affiliations.size());

  // There was no branding information in version 1, thus we expect it to be
  // empty after the migration.
  const auto WithoutBrandingInfo =
      [](AffiliatedFacetsWithUpdateTime affiliation) {
        for (auto& facet : affiliation.facets)
          facet.branding_info = FacetBrandingInfo();

        return affiliation;
      };

  ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(
      WithoutBrandingInfo(TestEquivalenceClass1()), affiliations[0]);
  ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(
      WithoutBrandingInfo(TestEquivalenceClass2()), affiliations[1]);
  ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(
      WithoutBrandingInfo(TestEquivalenceClass3()), affiliations[2]);
}

TEST_F(AffiliationDatabaseTest, InitializeFromVersion2) {
  // Close and delete the current database and create it from scratch with the
  // SQLite statement stored in affiliation_db_v2.sql.
  CloseDatabase();
  AffiliationDatabase::Delete(db_path());
  base::FilePath src_root_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_root_dir));
  base::FilePath sql_path_v2 = src_root_dir.AppendASCII("components")
                                   .AppendASCII("test")
                                   .AppendASCII("data")
                                   .AppendASCII("password_manager")
                                   .AppendASCII("affiliation_db_v2.sql");
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(), sql_path_v2));

  // Expect the migration to be a no-op that does not modify the existing data.
  OpenDatabase();
  std::vector<AffiliatedFacetsWithUpdateTime> affiliations;
  db().GetAllAffiliationsAndBranding(&affiliations);
  ASSERT_EQ(3u, affiliations.size());
  ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(TestEquivalenceClass1(),
                                                        affiliations[0]);
  ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(TestEquivalenceClass2(),
                                                        affiliations[1]);
  ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(TestEquivalenceClass3(),
                                                        affiliations[2]);
  EXPECT_EQ(TestEquivalenceClass3().facets[0].branding_info,
            affiliations[2].facets[0].branding_info);
}

}  // namespace password_manager
