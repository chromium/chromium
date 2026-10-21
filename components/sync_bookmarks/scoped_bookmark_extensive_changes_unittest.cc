// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/scoped_bookmark_extensive_changes.h"

#include <memory>

#include "base/scoped_observation.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/test/mock_bookmark_model_observer.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_bookmarks {

namespace {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkModelObserver;
using bookmarks::MockBookmarkModelObserver;
using testing::InSequence;
using testing::Mock;
using testing::NiceMock;
using testing::Test;

class ScopedBookmarkExtensiveChangesTest : public Test {
 protected:
  std::unique_ptr<BookmarkModel> bookmark_model_ =
      bookmarks::TestBookmarkClient::CreateModel();
};

// Verifies that `ScopedBookmarkExtensiveChanges` begins extensive changes on
// construction and ends extensive changes when destroyed.
TEST_F(ScopedBookmarkExtensiveChangesTest,
       ShouldBeginAndEndExtensiveChangesInScope) {
  NiceMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(bookmark_model_.get());

  EXPECT_FALSE(bookmark_model_->IsDoingExtensiveChanges());

  {
    EXPECT_CALL(observer, ExtensiveBookmarkChangesBeginning());
    ScopedBookmarkExtensiveChanges extensive_changes(*bookmark_model_);
    Mock::VerifyAndClearExpectations(&observer);
    EXPECT_TRUE(bookmark_model_->IsDoingExtensiveChanges());

    EXPECT_CALL(observer, ExtensiveBookmarkChangesEnded());
  }

  EXPECT_FALSE(bookmark_model_->IsDoingExtensiveChanges());
}

// Verifies that nested `ScopedBookmarkExtensiveChanges` scopes keep extensive
// changes active until the outermost scope exits.
TEST_F(ScopedBookmarkExtensiveChangesTest, ShouldSupportNestedScopes) {
  NiceMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(bookmark_model_.get());

  {
    InSequence seq;
    EXPECT_CALL(observer, ExtensiveBookmarkChangesBeginning()).Times(1);
    EXPECT_CALL(observer, ExtensiveBookmarkChangesEnded()).Times(1);
  }

  {
    ScopedBookmarkExtensiveChanges outer_changes(*bookmark_model_);
    EXPECT_TRUE(bookmark_model_->IsDoingExtensiveChanges());

    {
      ScopedBookmarkExtensiveChanges inner_changes(*bookmark_model_);
      EXPECT_TRUE(bookmark_model_->IsDoingExtensiveChanges());
    }

    // Extensive changes must remain active after the inner scope exits.
    EXPECT_TRUE(bookmark_model_->IsDoingExtensiveChanges());
  }

  EXPECT_FALSE(bookmark_model_->IsDoingExtensiveChanges());
}

}  // namespace

}  // namespace sync_bookmarks
