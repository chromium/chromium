// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// gMock matchers to check bookmark nodes or hierarchies thereof.
//
// All matchers work with either raw or smart pointer to BookmarkNode. Usage
// example:
//   EXPECT_THAT(
//       bookmark_bar->children(), ElementsAre(
//           IsUrlBookmark(u"Title", GURL("http://url.com")),
//           IsFolder(u"Folder title")));
//
// The arguments passed to the matchers can themselves be matchers, so there is
// flexibility to express what the test is trying to verify. The example below
// doesn't verify the title and verifies the URL with a string literal:
//   EXPECT_THAT(
//       bookmark_bar->children(), ElementsAre(
//           IsUrlBookmark(_, "http://url.com/"),
//           IsFolder(_)));
//
// This example checks that the bookmark bar node has two children nodes: a URL
// and a folder. Additional attributes can be specified to these matchers,
// including complex hierarchy checks:
//   EXPECT_THAT(
//       bookmark_bar->children(), ElementsAre(
//           IsUrlBookmark(u"Title", _),
//           IsFolder(u"Folder title",
//               IsUrlBookmark(u"Title 2", _),
//               IsUrlBookmark(u"Title 3", _))));

#ifndef COMPONENTS_BOOKMARKS_TEST_TEST_MATCHERS_H_
#define COMPONENTS_BOOKMARKS_TEST_TEST_MATCHERS_H_

#include "components/bookmarks/browser/bookmark_node.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace bookmarks {
namespace test {

MATCHER(IsFolder, "") {
  if (!arg) {
    *result_listener << "Expected folder but got null bookmark node.";
    return false;
  }
  if (!arg->is_folder()) {
    *result_listener << "Expected folder but got URL bookmark with URL "
                     << arg->url();
    return false;
  }
  return true;
}

MATCHER_P(IsFolder, title, "") {
  return testing::ExplainMatchResult(IsFolder(), arg, result_listener) &&
         testing::ExplainMatchResult(
             testing::Property(&BookmarkNode::GetTitle, title), *arg,
             result_listener);
}

MATCHER_P2(IsFolder, title, children_matcher, "") {
  return testing::ExplainMatchResult(IsFolder(title), arg, result_listener) &&
         testing::ExplainMatchResult(children_matcher, arg->children(),
                                     result_listener);
}

MATCHER(IsUrlBookmark, "") {
  if (!arg) {
    *result_listener << "Expected URL bookmark but got null bookmark node.";
    return false;
  }
  if (!arg->is_url()) {
    *result_listener << "Expected URL bookmark but got folder with title "
                     << arg->GetTitle();
    return false;
  }
  return true;
}

MATCHER_P(IsUrlBookmark, title, "") {
  return testing::ExplainMatchResult(IsUrlBookmark(), arg, result_listener) &&
         testing::ExplainMatchResult(
             testing::Property(&BookmarkNode::GetTitle, title), *arg,
             result_listener);
}

MATCHER_P2(IsUrlBookmark, title, url, "") {
  return testing::ExplainMatchResult(IsUrlBookmark(title), arg,
                                     result_listener) &&
         testing::ExplainMatchResult(testing::Property(&BookmarkNode::url, url),
                                     *arg, result_listener);
}

MATCHER_P(HasUuid, uuid, "") {
  if (!arg) {
    *result_listener << "Got null bookmark node.";
    return false;
  }
  return testing::ExplainMatchResult(uuid, arg->uuid(), result_listener);
}

MATCHER_P3(IsFolderWithUuid, title, uuid, children_matcher, "") {
  return testing::ExplainMatchResult(IsFolder(title, children_matcher), arg,
                                     result_listener) &&
         testing::ExplainMatchResult(HasUuid(uuid), arg, result_listener);
}

MATCHER_P3(IsUrlBookmarkWithUuid, title, url, uuid, "") {
  return testing::ExplainMatchResult(IsUrlBookmark(title, url), arg,
                                     result_listener) &&
         testing::ExplainMatchResult(HasUuid(uuid), arg, result_listener);
}

}  // namespace test
}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_TEST_TEST_MATCHERS_H_
