// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/history/core/browser/visit_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace history {
namespace {

struct VisitToTest {
  // Identifies the context.
  int context_id_int;
  int nav_entry_id;

  // Used when adding this to the tracker
  const char* url;
  const VisitID visit_id;

  // Used when finding the referrer
  const char* referrer;

  // the correct referring visit ID to compare to the computed one
  VisitID referring_visit_id;
};

void AddVisitToTracker(const VisitToTest& test_data, VisitTracker* tracker) {
  // Our host pointer is actually just an int, convert it (it will not get
  // dereferenced).
  ContextID context_id = test_data.context_id_int;

  // Check the referrer for this visit.
  VisitID ref_visit = tracker->GetLastVisit(context_id, test_data.nav_entry_id,
                                            GURL(test_data.referrer));
  EXPECT_EQ(test_data.referring_visit_id, ref_visit);

  // Now add this visit.
  tracker->AddVisit(context_id, test_data.nav_entry_id, GURL(test_data.url),
                    test_data.visit_id);
}

void RunTest(VisitTracker* tracker, VisitToTest* test, int test_count) {
  for (int i = 0; i < test_count; i++)
    AddVisitToTracker(test[i], tracker);
}

}  // namespace

// A simple test that makes sure we transition between main pages in the
// presence of back/forward.
TEST(VisitTracker, SimpleTransitions) {
  VisitToTest test_simple[] = {
      // Started here:
      {1, 1, "http://www.google.com/", 1, "", 0},
      // Clicked a link:
      {1, 2, "http://images.google.com/", 2, "http://www.google.com/", 1},
      // Went back, then clicked a link:
      {1, 3, "http://video.google.com/", 3, "http://www.google.com/", 1},
  };

  VisitTracker tracker;
  RunTest(&tracker, test_simple, std::size(test_simple));
}

// Test that referrer is properly computed when there are different frame
// navigations happening.
TEST(VisitTracker, Frames) {
  VisitToTest test_frames[] = {
      // Started here:
      {1, 1, "http://foo.com/", 1, "", 0},
      // Which had an auto-loaded subframe:
      {1, 1, "http://foo.com/ad.html", 2, "http://foo.com/", 1},
      // ...and another auto-loaded subframe:
      {1, 1, "http://foo.com/ad2.html", 3, "http://foo.com/", 1},
      // ...and the user navigated the first subframe to somwhere else
      {1, 2, "http://bar.com/", 4, "http://foo.com/ad.html", 2},
      // ...and then the second subframe somewhere else
      {1, 3, "http://fud.com/", 5, "http://foo.com/ad2.html", 3},
      // ...and then the main frame somewhere else.
      {1, 4, "http://www.google.com/", 6, "http://foo.com/", 1},
  };

  VisitTracker tracker;
  RunTest(&tracker, test_frames, std::size(test_frames));
}

// Test frame navigation to make sure that the referrer is properly computed
// when there are multiple processes navigating the same pages.
TEST(VisitTracker, MultiProcess) {
  VisitToTest test_processes[] = {
    // Process 1 and 2 start here:
    {1, 1, "http://foo.com/",           1, "",                       0},
    {2, 1, "http://foo.com/",           2, "",                       0},
    // They have some subframes:
    {1, 1, "http://foo.com/ad.html",    3, "http://foo.com/",        1},
    {2, 1, "http://foo.com/ad.html",    4, "http://foo.com/",        2},
    // Subframes are navigated:
    {1, 2, "http://bar.com/",           5, "http://foo.com/ad.html", 3},
    {2, 2, "http://bar.com/",           6, "http://foo.com/ad.html", 4},
    // Main frame is navigated:
    {1, 3, "http://www.google.com/",    7, "http://foo.com/",        1},
    {2, 3, "http://www.google.com/",    8, "http://foo.com/",        2},
  };

  VisitTracker tracker;
  RunTest(&tracker, test_processes, std::size(test_processes));
}

// Test that processes get removed properly.
TEST(VisitTracker, ProcessRemove) {
  // Simple navigation from one process.
  VisitToTest part1[] = {
      {1, 1, "http://www.google.com/", 1, "", 0},
      {1, 2, "http://images.google.com/", 2, "http://www.google.com/", 1},
  };

  VisitTracker tracker;
  RunTest(&tracker, part1, std::size(part1));

  // Say that context has been invalidated.
  tracker.ClearCachedDataForContextID(1);

  // Simple navigation from a new process with the same ID, it should not find
  // a referrer.
  VisitToTest part2[] = {
      {1, 1, "http://images.google.com/", 2, "http://www.google.com/", 0},
  };
  RunTest(&tracker, part2, std::size(part2));
}

TEST(VisitTracker, RemoveVisitById) {
  VisitToTest test_simple[] = {
      {1, 1, "http://www.google.com/", 2, "", 0},
      {1, 2, "http://www.google2.com/", 3, "", 0},
      {1, 3, "http://www.google3.com/", 4, "", 0},
  };

  VisitTracker tracker;
  RunTest(&tracker, test_simple, std::size(test_simple));

  // Remove the first visit.
  const VisitToTest& removed = test_simple[0];
  tracker.RemoveVisitById(removed.visit_id);

  // The first visit should no longer be in the tracker.
  EXPECT_EQ(0, tracker.GetLastVisit(removed.context_id_int,
                                    removed.nav_entry_id, GURL(removed.url)));
  // The second and third should still be present.
  EXPECT_EQ(test_simple[1].visit_id,
            tracker.GetLastVisit(test_simple[1].context_id_int,
                                 test_simple[1].nav_entry_id,
                                 GURL(test_simple[1].url)));
  EXPECT_EQ(test_simple[2].visit_id,
            tracker.GetLastVisit(test_simple[2].context_id_int,
                                 test_simple[2].nav_entry_id,
                                 GURL(test_simple[2].url)));

  // Add back the first one, reusing the id and verify it is present.
  AddVisitToTracker(removed, &tracker);
  EXPECT_EQ(removed.visit_id,
            tracker.GetLastVisit(removed.context_id_int, removed.nav_entry_id,
                                 GURL(removed.url)));

  // Remove the first one again, and verify state.
  tracker.RemoveVisitById(removed.visit_id);
  EXPECT_EQ(0, tracker.GetLastVisit(removed.context_id_int,
                                    removed.nav_entry_id, GURL(removed.url)));
  EXPECT_EQ(test_simple[1].visit_id,
            tracker.GetLastVisit(test_simple[1].context_id_int,
                                 test_simple[1].nav_entry_id,
                                 GURL(test_simple[1].url)));
  EXPECT_EQ(test_simple[2].visit_id,
            tracker.GetLastVisit(test_simple[2].context_id_int,
                                 test_simple[2].nav_entry_id,
                                 GURL(test_simple[2].url)));
}

TEST(VisitTracker, Clear) {
  VisitToTest test_simple[] = {
      {1, 1, "http://www.google.com/", 2, "", 0},
      {1, 2, "http://www.google2.com/", 3, "", 0},
      {1, 3, "http://www.google3.com/", 4, "", 0},
  };

  VisitTracker tracker;
  RunTest(&tracker, test_simple, std::size(test_simple));
  EXPECT_FALSE(tracker.IsEmpty());
  tracker.Clear();
  EXPECT_TRUE(tracker.IsEmpty());
}

}  // namespace history
