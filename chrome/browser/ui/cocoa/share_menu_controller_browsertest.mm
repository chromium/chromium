// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/share_menu_controller.h"

#import "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/base/apple/url_conversions.h"
#include "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/events/test/cocoa_test_event_utils.h"

// Mock sharing service for sensing shared items.
@interface MockSharingService : NSSharingService
@property(nonatomic, strong) id sharedItem;
@end

@implementation MockSharingService

// The real one is backed by SHKSharingService parameters which
// don't appear to be present when inheriting from vanilla
// |NSSharingService|.
@synthesize subject;
@synthesize sharedItem = _sharedItem;

- (void)performWithItems:(NSArray*)items {
  self.sharedItem = items.firstObject;
}

@end

namespace {
MockSharingService* MakeMockSharingService() {
  return [[MockSharingService alloc]
       initWithTitle:@"Mock service"
               image:[NSImage imageNamed:NSImageNameAddTemplate]
      alternateImage:nil
             handler:^{
             }];
}
}  // namespace

class ShareMenuControllerTest : public InProcessBrowserTest {
 public:
  ShareMenuControllerTest() = default;

  void SetUpOnMainThread() override {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());

    url_ = embedded_test_server()->GetURL("/title2.html");
    ASSERT_TRUE(AddTabAtIndex(0, url_, ui::PAGE_TRANSITION_TYPED));
    controller_ = [[ShareMenuController alloc] init];
  }

 protected:
  // Create a menu item for |service| and trigger it using
  // the target/action of real menu items created by
  // |controller_|
  void PerformShare(NSSharingService* service) {
    NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Share"];

    [controller_ menuNeedsUpdate:menu];

    NSMenuItem* mock_menu_item = [[NSMenuItem alloc] initWithTitle:@"test"
                                                            action:nil
                                                     keyEquivalent:@""];
    mock_menu_item.representedObject = service;

    NSMenuItem* first_menu_item = [menu itemAtIndex:0];
    id target = first_menu_item.target;
    SEL action = first_menu_item.action;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    [target performSelector:action withObject:mock_menu_item];
#pragma clang diagnostic pop
  }
  GURL url_;
  ShareMenuController* __strong controller_;
};

IN_PROC_BROWSER_TEST_F(ShareMenuControllerTest, PopulatesMenu) {
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Share"];
  NSArray* sharing_services_for_url = [NSSharingService
      sharingServicesForItems:@[ [NSURL URLWithString:@"http://example.com"] ]];
  EXPECT_GT(sharing_services_for_url.count, 0U);

  [controller_ menuNeedsUpdate:menu];

  // -1 for reading list, +1 for "More..." if it's showing.
  // This cancels out, so only decrement if the "More..." item
  // isn't showing.
  NSInteger expected_count = sharing_services_for_url.count;
  EXPECT_EQ(menu.numberOfItems, expected_count);

  NSSharingService* reading_list_service = [NSSharingService
      sharingServiceNamed:NSSharingServiceNameAddToSafariReadingList];

  NSUInteger i = 0;
  // Ensure there's a menu item for each service besides reading list.
  for (NSSharingService* service in sharing_services_for_url) {
    if ([service isEqual:reading_list_service])
      continue;
    NSMenuItem* menu_item = [menu itemAtIndex:i];
    EXPECT_NSEQ(menu_item.representedObject, service);
    EXPECT_EQ(menu_item.target, static_cast<id>(controller_));
    ++i;
  }

  // Ensure the menu is cleared between updates.
  [controller_ menuNeedsUpdate:menu];
  EXPECT_EQ(menu.numberOfItems, expected_count);
}

IN_PROC_BROWSER_TEST_F(ShareMenuControllerTest, AddsMoreButton) {
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Share"];
  [controller_ menuNeedsUpdate:menu];

  NSInteger number_of_items = menu.numberOfItems;
  EXPECT_GT(number_of_items, 0);
  NSMenuItem* last_item = [menu itemAtIndex:number_of_items - 1];
  EXPECT_NSEQ(last_item.title, l10n_util::GetNSString(IDS_SHARING_MORE_MAC));
}

IN_PROC_BROWSER_TEST_F(ShareMenuControllerTest, ActionPerformsShare) {
  MockSharingService* service = MakeMockSharingService();
  EXPECT_FALSE(service.sharedItem);

  PerformShare(service);

  EXPECT_NSEQ(service.sharedItem, net::NSURLWithGURL(url_));
  // Title of chrome/test/data/title2.html
  EXPECT_NSEQ(service.subject, @"Title Of Awesomeness");
  EXPECT_EQ(service.delegate,
            static_cast<id<NSSharingServiceDelegate>>(controller_));
}

IN_PROC_BROWSER_TEST_F(ShareMenuControllerTest, SharingDelegate) {
  NSURL* url = [NSURL URLWithString:@"http://google.com"];
  NSSharingService* service = [[NSSharingService alloc]
       initWithTitle:@"Mock service"
               image:[NSImage imageNamed:NSImageNameAddTemplate]
      alternateImage:nil
             handler:^{
               // Verify inside the block since everything is cleared after the
               // share.

               // Extra service since the service param on the delegate
               // methods is nonnull and circular references could get hairy.
               MockSharingService* mockService = MakeMockSharingService();

               NSWindow* browser_window =
                   browser()->window()->GetNativeWindow().GetNativeNSWindow();
               EXPECT_NSNE([controller_ sharingService:mockService
                               sourceFrameOnScreenForShareItem:url],
                           NSZeroRect);
               NSSharingContentScope scope = NSSharingContentScopeItem;
               EXPECT_NSEQ([controller_ sharingService:mockService
                               sourceWindowForShareItems:@[ url ]
                                     sharingContentScope:&scope],
                           browser_window);
               EXPECT_EQ(scope, NSSharingContentScopeFull);
               NSRect contentRect;
               EXPECT_TRUE([controller_ sharingService:mockService
                           transitionImageForShareItem:url
                                           contentRect:&contentRect]);
             }];

  PerformShare(service);
}

IN_PROC_BROWSER_TEST_F(ShareMenuControllerTest, Histograms) {
  base::HistogramTester tester;
  const std::string histogram_name = "Mac.FileMenuNativeShare";

  tester.ExpectTotalCount(histogram_name, 0);

  MockSharingService* service = MakeMockSharingService();

  [controller_ sharingService:service didShareItems:@[]];
  tester.ExpectBucketCount(histogram_name, true, 1);
  tester.ExpectTotalCount(histogram_name, 1);

  [controller_ sharingService:service didShareItems:@[]];
  tester.ExpectBucketCount(histogram_name, true, 2);
  tester.ExpectTotalCount(histogram_name, 2);

  [controller_
           sharingService:service
      didFailToShareItems:@[]
                    error:[NSError errorWithDomain:@"" code:0 userInfo:nil]];
  tester.ExpectTotalCount(histogram_name, 3);
  tester.ExpectBucketCount(histogram_name, false, 1);
}

IN_PROC_BROWSER_TEST_F(ShareMenuControllerTest, MenuHasKeyEquivalent) {
  // If this method isn't implemented, |menuNeedsUpdate:| is called any time
  // *any* hotkey is used
  ASSERT_TRUE([controller_ respondsToSelector:@selector
                           (menuHasKeyEquivalent:forEvent:target:action:)]);

  // Ensure that calling |menuHasKeyEquivalent:...| the first time populates the
  // menu.
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Share"];
  EXPECT_EQ(menu.numberOfItems, 0);
  NSEvent* event = cocoa_test_event_utils::KeyEventWithKeyCode(
      'i', 'i', NSEventTypeKeyDown,
      NSEventModifierFlagCommand | NSEventModifierFlagShift);
  id ignored_target;
  SEL ignored_action;
  EXPECT_FALSE([controller_ menuHasKeyEquivalent:menu
                                        forEvent:event
                                          target:&ignored_target
                                          action:&ignored_action]);
  EXPECT_GT([menu numberOfItems], 0);

  NSMenuItem* item = [menu itemAtIndex:0];
  // |menuHasKeyEquivalent:....| shouldn't populate the menu after the first
  // time.
  [controller_ menuHasKeyEquivalent:menu
                           forEvent:event
                             target:&ignored_target
                             action:&ignored_action];
  EXPECT_EQ(item, [menu itemAtIndex:0]);  // Pointer equality intended.
}
