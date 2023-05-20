// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/view_demotion.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/test/proto_printer.h"
#include "components/feed/core/v2/test/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {
using feedstore::CreateDocView;
using testing::UnorderedElementsAre;

class ViewDemotionTest : public testing::Test {
 public:
  ViewDemotionTest() {
    feed::Config config;
    config.max_docviews_to_send = 5;
    SetFeedConfigForTesting(config);
  }
  void SetUp() override {
    // Make sure time isn't 0, so we can have events in the past.
    task_environment_.AdvanceClock(base::Hours(10000));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ViewDemotionTest, OnlyNewAreCounted) {
  base::Time now = base::Time::Now();

  auto view1 = CreateDocView(10, now - base::Hours(71));
  // Too old.
  auto view2 = CreateDocView(11, now - base::Hours(73));
  // From the future.
  auto view3 = CreateDocView(12, now + base::Hours(1));
  DocViewDigest digest = internal::CreateDigest({view1, view2, view3});
  EXPECT_THAT(digest.doc_view_counts,
              UnorderedElementsAre(DocViewCount{view1.docid(), 1}));
  EXPECT_THAT(digest.old_doc_views,
              UnorderedElementsAre(EqualsProto(view2), EqualsProto(view3)));
}

TEST_F(ViewDemotionTest, MultipleViewsOfOneDocument) {
  base::Time now = base::Time::Now();
  // 2/3 views are fresh.
  auto view1 = CreateDocView(10, now - base::Hours(71));
  auto view2 = CreateDocView(10, now - base::Hours(1));
  auto view3 = CreateDocView(10, now - base::Hours(73));
  DocViewDigest digest = internal::CreateDigest({view1, view2, view3});
  EXPECT_THAT(digest.doc_view_counts,
              UnorderedElementsAre(DocViewCount{view1.docid(), 2}));
  EXPECT_THAT(digest.old_doc_views, UnorderedElementsAre(EqualsProto(view3)));
}

TEST_F(ViewDemotionTest, TotalViewsOfFewDocsCanExceedLimit) {
  base::Time now = base::Time::Now();
  DocViewDigest digest = internal::CreateDigest({
      CreateDocView(10, now - base::Hours(1)),
      CreateDocView(10, now - base::Hours(2)),
      CreateDocView(10, now - base::Hours(3)),
      CreateDocView(10, now - base::Hours(4)),
      CreateDocView(10, now - base::Hours(5)),
      CreateDocView(10, now - base::Hours(6)),
      CreateDocView(10, now - base::Hours(7)),
  });
  EXPECT_THAT(digest.doc_view_counts,
              UnorderedElementsAre(DocViewCount{10, 7}));
  EXPECT_THAT(digest.old_doc_views, testing::IsEmpty());
}

TEST_F(ViewDemotionTest, TotalDocumentsExceedsLimit) {
  base::HistogramTester histograms;

  base::Time now = base::Time::Now();
  std::vector<feedstore::DocView> views = {
      CreateDocView(9, now - base::Hours(8)),
      CreateDocView(10, now - base::Hours(1)),
      CreateDocView(11, now - base::Hours(2)),
      CreateDocView(12, now - base::Hours(3)),
      CreateDocView(13, now - base::Hours(4)),
      CreateDocView(14, now - base::Hours(5)),
      CreateDocView(15, now - base::Hours(6)),
      CreateDocView(16, now - base::Hours(7)),
  };
  DocViewDigest digest = internal::CreateDigest(views);
  // max_docviews_to_send = 5
  EXPECT_THAT(digest.doc_view_counts,
              UnorderedElementsAre(DocViewCount{10, 1}, DocViewCount{11, 1},
                                   DocViewCount{12, 1}, DocViewCount{13, 1},
                                   DocViewCount{14, 1}));

  EXPECT_THAT(digest.old_doc_views,
              UnorderedElementsAre(EqualsProto(views[0]), EqualsProto(views[6]),
                                   EqualsProto(views[7])));

  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.DroppedDocumentViewCount", 3, 1);
}

}  // namespace
}  // namespace feed
