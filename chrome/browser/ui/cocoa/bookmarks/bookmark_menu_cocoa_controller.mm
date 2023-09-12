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
  browser->OpenURL(params);
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
  void BookmarkModelBeingDeleted(BookmarkModel* model) override;
  void BookmarkModelLoaded(BookmarkModel* model, bool ids_reassigned) override;
  void BookmarkNodeMoved(BookmarkModel* model,
                         const BookmarkNode* old_parent,
                         size_t old_index,
                         const BookmarkNode* new_parent,
                         size_t new_index) override {}
  void BookmarkNodeAdded(BookmarkModel* model,
                         const BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override {}
  void BookmarkNodeRemoved(BookmarkModel* model,
                           const BookmarkNode* parent,
                           size_t old_index,
                           const BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override {}
  void BookmarkNodeChanged(BookmarkModel* model,
                           const BookmarkNode* node) override {}
  void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                  const BookmarkNode* node) override {}
  void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                     const BookmarkNode* node) override {}
  void BookmarkAllUserNodesRemoved(
      BookmarkModel* model,
      const std::set<GURL>& removed_urls) override {}

 private:
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation_{
      this};
  raw_ptr<Profile> profile_;
  WindowOpenDisposition disposition_;
  base::Uuid guid_;
};

BookmarkRestorer::BookmarkRestorer(Profile* profile,
                                   WindowOpenDisposition disposition,
                                   base::Uuid guid)
    : profile_(profile), disposition_(disposition), guid_(guid) {
  observation_.Observe(BookmarkModelFactory::GetForBrowserContext(profile));
}

void BookmarkRestorer::BookmarkModelBeingDeleted(BookmarkModel* model) {
  model->RemoveObserver(this);
  delete this;
}

void BookmarkRestorer::BookmarkModelLoaded(BookmarkModel* model,
                                           bool ids_reassigned) {
  model->RemoveObserver(this);
  if (const auto* node = model->GetNodeByUuid(guid_)) {
    DoOpenBookmark(profile_, disposition_, node);
  }
  delete this;
}

// Open the URL of the given BookmarkNode in the current tab. Waits for
// BookmarkModelLoaded() if needed (e.g. for a freshly-loaded profile).
void OpenBookmarkByGUID(WindowOpenDisposition disposition,
                        base::Uuid guid,
                        Profile* profile) {
  if (!profile)
    return;  // Failed to load profile, ignore.

  const auto* model = BookmarkModelFactory::GetForBrowserContext(profile);
  DCHECK(model);
  if (!model)
    return;  // Should never be reached.

  if (const auto* node = model->GetNodeByUuid(guid)) {
    // BookmarkModel already loaded this bookmark. Open it immediately.
    DoOpenBookmark(profile, disposition, node);
  } else {
    // BookmarkModel hasn't loaded yet. Wait for BookmarkModelLoaded(), and
    // *then* open it.
    DCHECK(!model->loaded());
    std::ignore = new BookmarkRestorer(profile, disposition, std::move(guid));
  }
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
  if (!profile)
    return;  // Unfortunately, we can't update a menu with a dead profile.
  const auto* model = BookmarkModelFactory::GetForBrowserContext(profile);
  base::Uuid guid = _bridge->TagToGUID([item tag]);
  const auto* node = model->GetNodeByUuid(guid);
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

// Return the GUID of the BookmarkNode that has the given id (called
// "identifier" here to avoid conflict with objc's concept of "id").
- (base::Uuid)guidForIdentifier:(int)identifier {
  return _bridge->TagToGUID(identifier);
}

- (IBAction)openBookmarkMenuItem:(id)sender {
  NSInteger tag = [sender tag];
  base::Uuid guid = [self guidForIdentifier:tag];
  [self openURLForGUID:std::move(guid)];
}

@end  // BookmarkMenuCocoaController
