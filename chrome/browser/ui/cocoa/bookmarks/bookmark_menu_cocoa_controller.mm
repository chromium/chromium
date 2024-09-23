// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller.h"

#import "base/apple/foundation_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"  // IDC_BOOKMARK_MENU
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/profile_metrics/browser_profile_type.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/menu_controller.h"
#include "ui/base/window_open_disposition.h"

using base::UserMetricsAction;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using content::OpenURLParams;
using content::Referrer;

namespace {

// Returns the NSMenuItem in |submenu|'s supermenu that holds |submenu|.
NSMenuItem* GetItemWithSubmenu(NSMenu* submenu) {
  NSArray* parent_items = [[submenu supermenu] itemArray];
  for (NSMenuItem* item in parent_items) {
    if ([item submenu] == submenu)
      return item;
  }
  return nil;
}

void DoOpenBookmark(Profile* profile,
                    WindowOpenDisposition disposition,
                    const BookmarkNode* node) {
  DCHECK(profile);
  Browser* browser = chrome::FindTabbedBrowser(profile, true);
  if (!browser)
    browser = Browser::Create(Browser::CreateParams(profile, true));
  OpenURLParams params(node->url(), Referrer(), disposition,
                       ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
  browser->OpenURL(params, /*navigation_handle_callback=*/{});
  RecordBookmarkLaunch(BookmarkLaunchLocation::kTopMenu,
                       profile_metrics::GetBrowserProfileType(profile));
}

// Waits for the BookmarkModelLoaded(), then calls DoOpenBookmark() on it.
//
// Owned by itself. Allocate with `new`.
class BookmarkRestorer : public bookmarks::BookmarkModelObserver {
 public:
  BookmarkRestorer(Profile* profile,
                   WindowOpenDisposition disposition,
                   base::Uuid guid);
  ~BookmarkRestorer() override = default;

  // bookmarks::BookmarkModelObserver:
  void BookmarkModelBeingDeleted() override;
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkNodeMoved(const BookmarkNode* old_parent,
                         size_t old_index,
                         const BookmarkNode* new_parent,
                         size_t new_index) override {}
  void BookmarkNodeAdded(const BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override {}
  void BookmarkNodeRemoved(const BookmarkNode* parent,
                           size_t old_index,
                           const BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override {}
  void BookmarkNodeChanged(const BookmarkNode* node) override {}
  void BookmarkNodeFaviconChanged(const BookmarkNode* node) override {}
  void BookmarkNodeChildrenReordered(const BookmarkNode* node) override {}
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override {}

 private:
  const raw_ptr<Profile> profile_;
  const WindowOpenDisposition disposition_;
  const base::Uuid guid_;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation_{
      this};
};

BookmarkRestorer::BookmarkRestorer(Profile* profile,
                                   WindowOpenDisposition disposition,
                                   base::Uuid guid)
    : profile_(profile), disposition_(disposition), guid_(guid) {
  observation_.Observe(BookmarkModelFactory::GetForBrowserContext(profile));
}

void BookmarkRestorer::BookmarkModelBeingDeleted() {
  delete this;
}

void BookmarkRestorer::BookmarkModelLoaded(bool ids_reassigned) {
  const BookmarkModel* model = observation_.GetSource();
  if (const BookmarkNode* node = model->GetNodeByUuid(
          guid_, BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes)) {
    DoOpenBookmark(profile_, disposition_, node);
  }
  delete this;
}

// Open the URL of the given BookmarkNode in the current tab. Waits for
// BookmarkModelLoaded() if needed (e.g. for a freshly-loaded profile).
void OpenBookmarkByGUID(WindowOpenDisposition disposition,
                        base::Uuid guid,
                        Profile* profile) {
  if (!profile) {
    // Failed to load profile, ignore.
    return;
  }

  const BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  CHECK(model);

  if (!model->loaded()) {
    // BookmarkModel hasn't loaded yet. Wait for BookmarkModelLoaded(), and
    // *then* open it.
    std::ignore = new BookmarkRestorer(profile, disposition, std::move(guid));
    return;
  }

  const BookmarkNode* node = model->GetNodeByUuid(
      guid, BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes);
  if (!node) {
    // Bookmark not known, ignore.
    return;
  }

  // BookmarkModel already loaded and the bookmark is known. Open it
  // immediately.
  DoOpenBookmark(profile, disposition, node);
}

}  // namespace

@implementation BookmarkMenuCocoaController {
  raw_ptr<BookmarkMenuBridge, AcrossTasksDanglingUntriaged>
      _bridge;  // Weak. Owns |self|.
}

+ (NSString*)tooltipForNode:(const BookmarkNode*)node {
  NSString* title = base::SysUTF16ToNSString(node->GetTitle());
  if (node->is_folder())
    return title;
  std::string urlString = node->url().possibly_invalid_spec();
  NSString* url = base::SysUTF8ToNSString(urlString);
  return cocoa_l10n_util::TooltipForURLAndTitle(url, title);
}

- (instancetype)initWithBridge:(BookmarkMenuBridge*)bridge {
  if ((self = [super init])) {
    _bridge = bridge;
    DCHECK(_bridge);
  }
  return self;
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  return ![AppController.sharedController keyWindowIsModal];
}

// NSMenu delegate method: called just before menu is displayed.
- (void)menuNeedsUpdate:(NSMenu*)menu {
  NSMenuItem* item = GetItemWithSubmenu(menu);
  Profile* profile = _bridge->GetProfile();
  if (!profile) {
    // Unfortunately, we can't update a menu with a dead profile.
    return;
  }

  const BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  base::Uuid guid = _bridge->TagToGUID([item tag]);
  const BookmarkNode* node = model->GetNodeByUuid(
      guid, BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes);
  _bridge->UpdateMenu(menu, node, /*recurse=*/false);
}

- (BOOL)menuHasKeyEquivalent:(NSMenu*)menu
                    forEvent:(NSEvent*)event
                      target:(id*)target
                      action:(SEL*)action {
  // Note it is OK to return NO if there's already an item in |menu| that
  // handles |event|.
  return NO;
}

// Open the URL of the given BookmarkNode in the current tab. If the Profile
// is not loaded in memory, load it first.
- (void)openURLForGUID:(base::Uuid)guid {
  WindowOpenDisposition disposition =
      ui::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  if (Profile* profile = _bridge->GetProfile()) {
    OpenBookmarkByGUID(disposition, std::move(guid), profile);
  } else {
    // Both BookmarkMenuBridge and BookmarkMenuCocoaController could get
    // destroyed before RunInSafeProfileHelper finishes. The callback needs to
    // be self-contained.
    app_controller_mac::RunInProfileSafely(
        _bridge->GetProfileDir(),
        base::BindOnce(&OpenBookmarkByGUID, disposition, std::move(guid)),
        app_controller_mac::kIgnoreOnFailure);
  }
}

- (IBAction)openBookmarkMenuItem:(id)sender {
  NSInteger tag = [sender tag];
  base::Uuid guid = _bridge->TagToGUID(tag);
  [self openURLForGUID:std::move(guid)];
}

+ (void)openBookmarkByGUID:(base::Uuid)guid
                 inProfile:(Profile*)profile
           withDisposition:(WindowOpenDisposition)disposition {
  OpenBookmarkByGUID(disposition, guid, profile);
}

@end  // BookmarkMenuCocoaController
