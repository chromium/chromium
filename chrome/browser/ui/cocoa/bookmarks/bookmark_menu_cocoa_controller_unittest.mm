// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller.h"
#include "components/bookmarks/browser/bookmark_node.h"

#include <string>

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

@interface FakeBookmarkMenuController : BookmarkMenuCocoaController {
 @public
  const BookmarkNode* _nodes[2];
  BOOL _opened[2];
}
- (instancetype)initWithProfile:(Profile*)profile;
@end

@implementation FakeBookmarkMenuController

- (instancetype)initWithProfile:(Profile*)profile {
  if ((self = [super init])) {
    std::u16string empty;
    BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);
    const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
    _nodes[0] = model->AddURL(bookmark_bar, 0, empty, GURL("http://0.com"));
    _nodes[1] = model->AddURL(bookmark_bar, 1, empty, GURL("http://1.com"));
  }
  return self;
}

- (base::Uuid)guidForIdentifier:(int)identifier {
  if ((identifier < 0) || (identifier >= 2))
    return base::Uuid();
  DCHECK(_nodes[identifier]);
  return _nodes[identifier]->uuid();
}

- (void)openURLForGUID:(base::Uuid)guid {
  base::span<const BookmarkNode*> nodes = base::make_span(_nodes);
  auto it = base::ranges::find_if(nodes, [&guid](const BookmarkNode* node) {
    return node->uuid() == guid;
  });
  ASSERT_NE(it, nodes.end());

  std::string url = (*it)->url().possibly_invalid_spec();
  if (url.find("http://0.com") != std::string::npos)
    _opened[0] = YES;
  if (url.find("http://1.com") != std::string::npos)
    _opened[1] = YES;
}

@end  // FakeBookmarkMenuController

class BookmarkMenuCocoaControllerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    bookmarks::test::WaitForBookmarkModelToLoad(
        BookmarkModelFactory::GetForBrowserContext(profile()));
    controller_ =
        [[FakeBookmarkMenuController alloc] initWithProfile:profile()];
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{BookmarkModelFactory::GetInstance(),
             BookmarkModelFactory::GetDefaultFactory()}};
  }

  FakeBookmarkMenuController* controller() { return controller_; }

 private:
  CocoaTestHelper cocoa_test_helper_;
  FakeBookmarkMenuController* __strong controller_;
};

TEST_F(BookmarkMenuCocoaControllerTest, TestOpenItem) {
  FakeBookmarkMenuController* c = controller();
  NSMenuItem* item = [[NSMenuItem alloc] init];
  for (int i = 0; i < 2; i++) {
    [item setTag:i];
    ASSERT_EQ(c->_opened[i], NO);
    [c openBookmarkMenuItem:item];
    ASSERT_NE(c->_opened[i], NO);
  }
}
