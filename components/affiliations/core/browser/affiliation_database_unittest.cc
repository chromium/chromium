// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_database.h"

#include <stdint.h>

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace affiliations {

namespace {

const char kTestFacetURI1[] = "https://alpha.example.com";
const char kTestFacetURI2[] = "https://beta.example.com";
const char kTestFacetURI3[] = "https://gamma.example.com";
const char kTestFacetURI4[] = "https://delta.example.com";
const char kTestFacetURI5[] = "https://epsilon.example.com";
const char kTestFacetURI6[] = "https://zeta.example.com";
const char kTestFacetURI7[] = "https://theta.example.com";
const char kTestWebsiteName[] = "Example.com";
const char kTestMainDomain[] = "example.com";

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
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI1)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI2)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI3)),
  };
  return affiliation;
}

AffiliatedFacetsWithUpdateTime TestEquivalenceClass2() {
  AffiliatedFacetsWithUpdateTime affiliation;
  affiliation.last_update_time = base::Time::FromInternalValue(kTestTimeUs2);
  affiliation.facets = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI4)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI5)),
  };
  return affiliation;
}

AffiliatedFacetsWithUpdateTime TestEquivalenceClass3() {
  AffiliatedFacetsWithUpdateTime affiliation;
  affiliation.last_update_time = base::Time::FromInternalValue(kTestTimeUs3);
  affiliation.facets = {
      Facet(FacetURI::FromCanonicalSpec(kTestAndroidFacetURI),
            FacetBrandingInfo{kTestAndroidPlayName, GURL(kTestAndroidIconURL)}),
  };
  return affiliation;
}

}  // namespace

class AffiliationDatabaseTest : public testing::Test {
 public:
  AffiliationDatabaseTest() = default;

  AffiliationDatabaseTest(const AffiliationDatabaseTest&) = delete;
  AffiliationDatabaseTest& operator=(const AffiliationDatabaseTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    OpenDatabase();
  }

  void OpenDatabase() {
    db_ = std::make_unique<AffiliationDatabase>();
    ASSERT_TRUE(db_->Init(db_path()));
  }

  void CloseDatabase() { db_.reset(); }

  void StoreInitialTestData() {
    std::vector<AffiliatedFacetsWithUpdateTime> removed;
    db_->StoreAndRemoveConflicting(TestEquivalenceClass1(), GroupedFacets(),
                                   &removed);
    db_->StoreAndRemoveConflicting(TestEquivalenceClass2(), GroupedFacets(),
                                   &removed);
    db_->StoreAndRemoveConflicting(TestEquivalenceClass3(), GroupedFacets(),
                                   &removed);
  }

  AffiliationDatabase& db() { return *db_; }

  base::FilePath db_path() {
    return temp_directory_.GetPath().Append(
        FILE_PATH_LITERAL("Test Affiliation Database"));
  }

 private:
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<AffiliationDatabase> db_;
};

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
    db().StoreAndRemoveConflicting(updated, GroupedFacets(), &removed);
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
        Facet(FacetURI::FromCanonicalSpec(kTestFacetURI3)),
        Facet(FacetURI::FromCanonicalSpec(kTestFacetURI4)),
    };
    db().StoreAndRemoveConflicting(intersecting, GroupedFacets(), &removed);

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
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(sql::test::CorruptSizeInHeader(db_path()));

  std::vector<AffiliatedFacetsWithUpdateTime> removed;
  db().StoreAndRemoveConflicting(TestEquivalenceClass2(), GroupedFacets(),
                                 &removed);
  std::vector<AffiliatedFacetsWithUpdateTime> affiliations;
  db().GetAllAffiliationsAndBranding(&affiliations);
  EXPECT_EQ(0u, affiliations.size());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AffiliationDatabase.Error",
      sql::SqliteResultCode::kInterrupt, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AffiliationDatabase.StoreResult", 1);
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
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root_dir));
  base::FilePath sql_path_v1 = src_root_dir.AppendASCII("components")
                                   .AppendASCII("test")
                                   .AppendASCII("data")
                                   .AppendASCII("password_manager")
                                   .AppendASCII("affiliation_db_v1.sql");
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(), sql_path_v1));

  // Re-open the database, triggering the migration.
  OpenDatabase();

  // Check that migration was successful and existing data was untouched.
  EXPECT_EQ(6, db().GetDatabaseVersionForTesting());
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
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root_dir));
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

TEST_F(AffiliationDatabaseTest, InitializeFromVersion3) {
  // Close and delete the current database and create it from scratch with the
  // SQLite statement stored in affiliation_db_v3.sql.
  CloseDatabase();
  AffiliationDatabase::Delete(db_path());
  base::FilePath src_root_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root_dir));
  base::FilePath sql_path_v3 = src_root_dir.AppendASCII("components")
                                   .AppendASCII("test")
                                   .AppendASCII("data")
                                   .AppendASCII("password_manager")
                                   .AppendASCII("affiliation_db_v3.sql");
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(), sql_path_v3));

  // Expect the migration to be a no-op that does not modify the existing data.
  OpenDatabase();
  std::vector<GroupedFacets> groupings = db().GetAllGroups();
  ASSERT_EQ(3u, groupings.size());
  std::vector<Facet> group1 = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI1)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI2)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI3))};
  EXPECT_THAT(groupings[0].facets, testing::UnorderedElementsAreArray(group1));
  EXPECT_THAT(groupings[0].branding_info,
              testing::Eq(FacetBrandingInfo{kTestWebsiteName,
                                            GURL(kTestAndroidIconURL)}));

  std::vector<Facet> group2 = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI4)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI5)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI7))};
  EXPECT_THAT(groupings[1].facets, testing::UnorderedElementsAreArray(group2));
  EXPECT_THAT(groupings[1].branding_info, testing::Eq(FacetBrandingInfo()));
  std::vector<Facet> group3 = {
      Facet(FacetURI::FromCanonicalSpec(kTestAndroidFacetURI))};
  EXPECT_THAT(groupings[2].facets, testing::UnorderedElementsAreArray(group3));
  EXPECT_THAT(groupings[2].branding_info,
              testing::Eq(FacetBrandingInfo{kTestAndroidPlayName,
                                            GURL(kTestAndroidIconURL)}));
}

TEST_F(AffiliationDatabaseTest, InitializeFromVersion4) {
  // Close and delete the current database and create it from scratch with the
  // SQLite statement stored in affiliation_db_v3.sql.
  CloseDatabase();
  AffiliationDatabase::Delete(db_path());
  base::FilePath src_root_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root_dir));
  base::FilePath sql_path_v3 = src_root_dir.AppendASCII("components")
                                   .AppendASCII("test")
                                   .AppendASCII("data")
                                   .AppendASCII("password_manager")
                                   .AppendASCII("affiliation_db_v4.sql");
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(), sql_path_v3));

  // Expect the migration to be a no-op that does not modify the existing data.
  OpenDatabase();
  std::vector<GroupedFacets> groupings = db().GetAllGroups();
  ASSERT_EQ(3u, groupings.size());
  std::vector<Facet> group1 = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI1), FacetBrandingInfo(),
            GURL(), kTestMainDomain),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI2), FacetBrandingInfo(),
            GURL(), kTestMainDomain),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI3), FacetBrandingInfo(),
            GURL(), kTestMainDomain)};
  EXPECT_THAT(groupings[0].facets, testing::UnorderedElementsAreArray(group1));
  EXPECT_THAT(groupings[0].branding_info,
              testing::Eq(FacetBrandingInfo{kTestWebsiteName,
                                            GURL(kTestAndroidIconURL)}));

  std::vector<Facet> group2 = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI4)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI5)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI7))};
  EXPECT_THAT(groupings[1].facets, testing::UnorderedElementsAreArray(group2));
  EXPECT_THAT(groupings[1].branding_info, testing::Eq(FacetBrandingInfo()));
  std::vector<Facet> group3 = {
      Facet(FacetURI::FromCanonicalSpec(kTestAndroidFacetURI))};
  EXPECT_THAT(groupings[2].facets, testing::UnorderedElementsAreArray(group3));
  EXPECT_THAT(groupings[2].branding_info,
              testing::Eq(FacetBrandingInfo{kTestAndroidPlayName,
                                            GURL(kTestAndroidIconURL)}));
}

TEST_F(AffiliationDatabaseTest, InitializeFromVersion5) {
  // Close and delete the current database and create it from scratch with the
  // SQLite statement stored in affiliation_db_v3.sql.
  CloseDatabase();
  AffiliationDatabase::Delete(db_path());
  base::FilePath src_root_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root_dir));
  base::FilePath sql_path_v5 = src_root_dir.AppendASCII("components")
                                   .AppendASCII("test")
                                   .AppendASCII("data")
                                   .AppendASCII("password_manager")
                                   .AppendASCII("affiliation_db_v5.sql");
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(), sql_path_v5));

  // Expect the migration to be a no-op that does not modify the existing data.
  OpenDatabase();

  GroupedFacets group1;
  group1.facets = {Facet(FacetURI::FromCanonicalSpec(kTestFacetURI1),
                         FacetBrandingInfo(), GURL(), kTestMainDomain),
                   Facet(FacetURI::FromCanonicalSpec(kTestFacetURI2),
                         FacetBrandingInfo(), GURL(), kTestMainDomain),
                   Facet(FacetURI::FromCanonicalSpec(kTestFacetURI3),
                         FacetBrandingInfo(), GURL(), kTestMainDomain)};
  group1.branding_info =
      FacetBrandingInfo{kTestWebsiteName, GURL(kTestAndroidIconURL)};
  GroupedFacets group2;
  group2.facets = {Facet(FacetURI::FromCanonicalSpec(kTestFacetURI4)),
                   Facet(FacetURI::FromCanonicalSpec(kTestFacetURI5)),
                   Facet(FacetURI::FromCanonicalSpec(kTestFacetURI7))};
  GroupedFacets group3;
  group3.facets = {Facet(FacetURI::FromCanonicalSpec(kTestAndroidFacetURI))};
  group3.branding_info =
      FacetBrandingInfo{kTestAndroidPlayName, GURL(kTestAndroidIconURL)};

  EXPECT_THAT(db().GetAllGroups(),
              testing::UnorderedElementsAre(group1, group2, group3));

  EXPECT_THAT(
      db().GetPSLExtensions(),
      testing::UnorderedElementsAre("app.com", "example.com", "news.com"));
}

TEST_F(AffiliationDatabaseTest, ClearUnusedCache) {
  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());

  OpenDatabase();

  std::vector<AffiliatedFacetsWithUpdateTime> affiliations;
  db().GetAllAffiliationsAndBranding(&affiliations);
  ASSERT_EQ(3u, affiliations.size());

  db().RemoveMissingFacetURI({FacetURI::FromCanonicalSpec(kTestFacetURI1),
                              FacetURI::FromCanonicalSpec(kTestFacetURI4)});

  db().GetAllAffiliationsAndBranding(&affiliations);
  ASSERT_EQ(2u, affiliations.size());

  ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(TestEquivalenceClass1(),
                                                        affiliations[0]);
  ExpectEquivalenceClassesIncludingBrandingInfoAreEqual(TestEquivalenceClass2(),
                                                        affiliations[1]);
}

TEST_F(AffiliationDatabaseTest, StoreAndRemoveConflictingUpdatesGrouping) {
  AffiliatedFacetsWithUpdateTime affiliation;
  affiliation.last_update_time = base::Time::FromInternalValue(kTestTimeUs1);
  affiliation.facets = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI1)),
  };

  GroupedFacets group;
  group.branding_info =
      FacetBrandingInfo{kTestAndroidPlayName, GURL(kTestAndroidIconURL)};
  group.facets = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI1)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI2)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI3)),
  };

  OpenDatabase();
  std::vector<AffiliatedFacetsWithUpdateTime> removed;
  db().StoreAndRemoveConflicting(affiliation, group, &removed);

  std::vector<GroupedFacets> groupings = db().GetAllGroups();
  EXPECT_EQ(1u, groupings.size());
  EXPECT_THAT(groupings[0].facets,
              testing::UnorderedElementsAreArray(group.facets));
  EXPECT_THAT(groupings[0].branding_info, testing::Eq(group.branding_info));

  db().StoreAndRemoveConflicting(affiliation, GroupedFacets(), &removed);

  EXPECT_EQ(0u, db().GetAllGroups().size());
}

TEST_F(AffiliationDatabaseTest, GetMatchingGroupOneMatch) {
  AffiliatedFacetsWithUpdateTime affiliation;
  affiliation.facets = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI1)),
  };

  GroupedFacets group;
  group.branding_info =
      FacetBrandingInfo{kTestAndroidPlayName, GURL(kTestAndroidIconURL)};
  group.facets = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI1)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI2)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI3)),
  };

  OpenDatabase();
  std::vector<AffiliatedFacetsWithUpdateTime> removed;
  db().StoreAndRemoveConflicting(affiliation, group, &removed);

  EXPECT_EQ(group, db().GetGroup(FacetURI::FromCanonicalSpec(kTestFacetURI1)));
  EXPECT_EQ(group, db().GetGroup(FacetURI::FromCanonicalSpec(kTestFacetURI2)));
  EXPECT_EQ(group, db().GetGroup(FacetURI::FromCanonicalSpec(kTestFacetURI3)));
}

// Test scenario when grouping info are stored for two facets, meaning they are
// grouped but aren't affiliated.
TEST_F(AffiliationDatabaseTest, GetMatchingGroupTwoMatches) {
  AffiliatedFacetsWithUpdateTime affiliation1;
  affiliation1.facets = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI1)),
  };

  AffiliatedFacetsWithUpdateTime affiliation2;
  affiliation2.facets = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI2)),
  };

  GroupedFacets group;
  group.branding_info =
      FacetBrandingInfo{kTestAndroidPlayName, GURL(kTestAndroidIconURL)};
  group.facets = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI1)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI2)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI3)),
  };

  OpenDatabase();
  std::vector<AffiliatedFacetsWithUpdateTime> removed;
  db().StoreAndRemoveConflicting(affiliation1, group, &removed);
  db().StoreAndRemoveConflicting(affiliation2, group, &removed);

  EXPECT_EQ(group, db().GetGroup(FacetURI::FromCanonicalSpec(kTestFacetURI1)));
  EXPECT_EQ(group, db().GetGroup(FacetURI::FromCanonicalSpec(kTestFacetURI2)));
  EXPECT_EQ(group, db().GetGroup(FacetURI::FromCanonicalSpec(kTestFacetURI3)));
}

// Test scenario when grouping info are stored for unrelated facets.
TEST_F(AffiliationDatabaseTest, GetMatchingGroupNoMatches) {
  OpenDatabase();
  std::vector<AffiliatedFacetsWithUpdateTime> removed;

  AffiliatedFacetsWithUpdateTime affiliation;
  affiliation.facets = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI1)),
  };
  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI1)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI2)),
  };
  db().StoreAndRemoveConflicting(affiliation, group, &removed);

  affiliation.facets = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI3)),
  };
  group.branding_info =
      FacetBrandingInfo{kTestAndroidPlayName, GURL(kTestAndroidIconURL)};
  group.facets = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI3)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI4)),
  };
  db().StoreAndRemoveConflicting(affiliation, group, &removed);

  GroupedFacets expected_group;
  expected_group.facets = {Facet(FacetURI::FromCanonicalSpec(kTestFacetURI5))};
  EXPECT_EQ(expected_group,
            db().GetGroup(FacetURI::FromCanonicalSpec(kTestFacetURI5)));
}

}  // namespace affiliations
