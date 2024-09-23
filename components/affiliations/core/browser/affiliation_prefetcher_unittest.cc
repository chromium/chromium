// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_prefetcher.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/affiliations/core/browser/mock_affiliation_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace affiliations {
namespace {
using ::base::test::RunOnceCallback;
using ::testing::UnorderedElementsAreArray;

constexpr base::TimeDelta kInitializationDelayOnStartup = base::Seconds(30);
}  // namespace

class AffiliationPrefetcherTest : public testing::Test {
 protected:
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  MockAffiliationSource* AddSource() {
    auto source = std::make_unique<MockAffiliationSource>(prefetcher());
    MockAffiliationSource* src_ptr = source.get();
    prefetcher()->RegisterSource(std::move(source));
    return src_ptr;
  }

  MockAffiliationService* mock_affiliation_service() {
    return &mock_affiliation_service_;
  }

  AffiliationPrefetcher* prefetcher() { return &prefetcher_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::StrictMock<MockAffiliationService> mock_affiliation_service_;
  AffiliationPrefetcher prefetcher_{&mock_affiliation_service_};
};

// Verifies that affiliations are prefetched on start-up for a single source.
TEST_F(AffiliationPrefetcherTest, LoadSingleAffiliationSourceOnStartup) {
  std::vector<FacetURI> facets = {
      FacetURI::FromCanonicalSpec("https://foo.com"),
      FacetURI::FromCanonicalSpec("https://bar.com"),
  };

  MockAffiliationSource* src_ptr = AddSource();
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(*src_ptr, GetFacets).WillOnce(RunOnceCallback<0>(facets));
    EXPECT_CALL(*mock_affiliation_service(),
                KeepPrefetchForFacets(UnorderedElementsAreArray(facets)));
    EXPECT_CALL(*mock_affiliation_service(),
                TrimUnusedCache(UnorderedElementsAreArray(facets)));
  }
  FastForwardBy(kInitializationDelayOnStartup);
}

// Verifies that affiliations are prefetched on start-up for multiple sources on
// a single request.
TEST_F(AffiliationPrefetcherTest, LoadMultipleAffiliationSourcesOnStartup) {
  testing::MockFunction<void()> mock_handler;
  std::vector<FacetURI> facets1 = {
      FacetURI::FromCanonicalSpec("https://foo.com"),
      FacetURI::FromCanonicalSpec("https://bar.com"),
  };
  std::vector<FacetURI> facets2 = {
      FacetURI::FromCanonicalSpec("https://foo2.com"),
      FacetURI::FromCanonicalSpec("https://bar2.com"),
  };
  std::vector<FacetURI> result;
  result.insert(result.end(), facets1.begin(), facets1.end());
  result.insert(result.end(), facets2.begin(), facets2.end());

  MockAffiliationSource* src_ptr1 = AddSource();
  MockAffiliationSource* src_ptr2 = AddSource();
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(mock_handler, Call());
    EXPECT_CALL(*src_ptr1, GetFacets).WillOnce(RunOnceCallback<0>(facets1));
    EXPECT_CALL(*src_ptr2, GetFacets).WillOnce(RunOnceCallback<0>(facets2));

    EXPECT_CALL(*mock_affiliation_service(),
                KeepPrefetchForFacets(UnorderedElementsAreArray(result)));
    EXPECT_CALL(*mock_affiliation_service(),
                TrimUnusedCache(UnorderedElementsAreArray(result)));
  }
  // Tests that all expected calls happen `kInitializationDelayOnStartup` after
  // start-up.
  FastForwardBy(kInitializationDelayOnStartup - base::Seconds(1));
  mock_handler.Call();
  FastForwardBy(base::Seconds(1));
}

// Verifies that affiliations for duplicate facets are prefetched with
// multiplicity.
TEST_F(AffiliationPrefetcherTest, DuplicateFacetsArePrefetchWithMultiplicity) {
  std::vector<FacetURI> facets = {
      FacetURI::FromCanonicalSpec("https://foo.com"),
      FacetURI::FromCanonicalSpec("https://foo.com"),
  };

  MockAffiliationSource* src_ptr = AddSource();
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(*src_ptr, GetFacets).WillOnce(RunOnceCallback<0>(facets));
    EXPECT_CALL(*mock_affiliation_service(), KeepPrefetchForFacets(facets));
    EXPECT_CALL(*mock_affiliation_service(), TrimUnusedCache(facets));
  }
  FastForwardBy(kInitializationDelayOnStartup);
}

// Verifies that affiliations are prefetched if new facets are added by sources
// after initialization. It also checks that invalid facets are filtered out.
TEST_F(AffiliationPrefetcherTest,
       PrefetchAffiliationsForFacetAddedAfterInitialization) {
  MockAffiliationSource* src_ptr = AddSource();
  FastForwardBy(kInitializationDelayOnStartup);
  RunUntilIdle();

  FacetURI facet = FacetURI::FromPotentiallyInvalidSpec("https://foo.com");
  FacetURI invalid_facet =
      FacetURI::FromPotentiallyInvalidSpec("invalid facet");
  EXPECT_CALL(*mock_affiliation_service(), Prefetch(facet, base::Time::Max()));
  EXPECT_CALL(*mock_affiliation_service(),
              Prefetch(invalid_facet, base::Time::Max()))
      .Times(0);
  src_ptr->AddFacet(facet);
  src_ptr->AddFacet(invalid_facet);
}

// Verifies that affiliations are dropped if facets are removed by sources after
// initialization.
TEST_F(AffiliationPrefetcherTest,
       PrefetchAffiliationsForFacetRemovedAfterInitialization) {
  MockAffiliationSource* src_ptr = AddSource();
  FastForwardBy(kInitializationDelayOnStartup);
  RunUntilIdle();

  FacetURI facet = FacetURI::FromCanonicalSpec("https://foo.com");
  EXPECT_CALL(*mock_affiliation_service(),
              CancelPrefetch(facet, base::Time::Max()));

  src_ptr->RemoveFacet(facet);
}

// Verifies that affiliations are refetched for all sources if a new source is
// registered `kInitializationDelayOnStartup` seconds after start-up.
TEST_F(AffiliationPrefetcherTest, TestSourceRegisteredLater) {
  std::vector<FacetURI> facets1 = {
      FacetURI::FromCanonicalSpec("https://foo.com"),
      FacetURI::FromCanonicalSpec("https://bar.com"),
  };

  MockAffiliationSource* src_ptr1 = AddSource();
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(*src_ptr1, GetFacets).WillOnce(RunOnceCallback<0>(facets1));
    EXPECT_CALL(*mock_affiliation_service(),
                KeepPrefetchForFacets(UnorderedElementsAreArray(facets1)));
    EXPECT_CALL(*mock_affiliation_service(),
                TrimUnusedCache(UnorderedElementsAreArray(facets1)));
  }
  FastForwardBy(kInitializationDelayOnStartup);

  std::vector<FacetURI> facets2 = {
      FacetURI::FromCanonicalSpec("https://foo2.com"),
      FacetURI::FromCanonicalSpec("https://bar2.com"),
  };

  std::unique_ptr<MockAffiliationSource> src_ptr2 =
      std::make_unique<MockAffiliationSource>(nullptr);

  std::vector<FacetURI> result;
  result.insert(result.end(), facets1.begin(), facets1.end());
  result.insert(result.end(), facets2.begin(), facets2.end());
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(*src_ptr1, GetFacets).WillOnce(RunOnceCallback<0>(facets1));
    EXPECT_CALL(*src_ptr2, GetFacets).WillOnce(RunOnceCallback<0>(facets2));
    EXPECT_CALL(*mock_affiliation_service(),
                KeepPrefetchForFacets(UnorderedElementsAreArray(result)));
    EXPECT_CALL(*mock_affiliation_service(),
                TrimUnusedCache(UnorderedElementsAreArray(result)));
  }
  prefetcher()->RegisterSource(std::move(src_ptr2));
  RunUntilIdle();
}

// Verifies that affiliations are refetched if a new source is registered while
// other sources are being initialized.
TEST_F(AffiliationPrefetcherTest, TestSourcesRegisteredAfterDelay) {
  EXPECT_CALL(*mock_affiliation_service(),
              KeepPrefetchForFacets(testing::IsEmpty()));
  EXPECT_CALL(*mock_affiliation_service(), TrimUnusedCache(testing::IsEmpty()));
  FastForwardBy(kInitializationDelayOnStartup);
  RunUntilIdle();

  std::vector<FacetURI> facets1 = {
      FacetURI::FromCanonicalSpec("https://foo.com"),
      FacetURI::FromCanonicalSpec("https://bar.com"),
  };
  std::vector<FacetURI> facets2 = {
      FacetURI::FromCanonicalSpec("https://foo2.com"),
      FacetURI::FromCanonicalSpec("https://bar2.com"),
  };
  std::vector<FacetURI> result;
  result.insert(result.end(), facets1.begin(), facets1.end());
  result.insert(result.end(), facets2.begin(), facets2.end());

  std::unique_ptr<MockAffiliationSource> src_ptr1 =
      std::make_unique<MockAffiliationSource>(nullptr);
  std::unique_ptr<MockAffiliationSource> src_ptr2 =
      std::make_unique<MockAffiliationSource>(nullptr);

  base::OnceCallback<void(std::vector<FacetURI>)> first_callback;
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(*src_ptr1, GetFacets)
        // Stores the first callback and awaits until a new source has been
        // registered in the meantime.
        .WillOnce(MoveArg<0>(&first_callback))
        // Returns facets for the first source after the final `Initialize` call
        // is triggered
        .WillOnce(RunOnceCallback<0>(facets1));

    EXPECT_CALL(*src_ptr2, GetFacets).WillOnce(RunOnceCallback<0>(facets2));
    EXPECT_CALL(*mock_affiliation_service(),
                KeepPrefetchForFacets(UnorderedElementsAreArray(result)));
    EXPECT_CALL(*mock_affiliation_service(),
                TrimUnusedCache(UnorderedElementsAreArray(result)));
  }

  prefetcher()->RegisterSource(std::move(src_ptr1));
  prefetcher()->RegisterSource(std::move(src_ptr2));
  // Once the first_callback is resolved, `OnResultFromAllSourcesReceived`
  // handles the case where a new source was registered and calls `Initialize`
  // again.
  std::move(first_callback).Run(facets1);

  RunUntilIdle();
}

// Verifies that the affiliations cache is reset if no sources are registered
// `kInitializationDelayOnStartup` seconds after start-up.
TEST_F(AffiliationPrefetcherTest, TestNoSourcesAfterStartup) {
  EXPECT_CALL(*mock_affiliation_service(),
              KeepPrefetchForFacets(testing::IsEmpty()));
  EXPECT_CALL(*mock_affiliation_service(), TrimUnusedCache(testing::IsEmpty()));
  FastForwardBy(kInitializationDelayOnStartup);
  RunUntilIdle();
}

}  // namespace affiliations
