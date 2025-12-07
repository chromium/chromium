// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller.h"

#import "base/apple/foundation_util.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"  // IDC_BOOKMARK_MENU
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_observer.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/profile_metrics/browser_profile_type.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/window_open_disposition.h"
#import "ui/menus/cocoa/menu_controller.h"

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
    if ([item submenu] == submenu) {
      return item;
    }
  }
  return nil;
}

const BookmarkNode* GetNodeByUuid(const BookmarkModel* model,
                                  const base::Uuid& guid) {
  CHECK(model);
  const BookmarkNode* node = model->GetNodeByUuid(
      guid, BookmarkModel::NodeTypeForUuidLookup::kAccountNodes);
  if (!node) {
    node = model->GetNodeByUuid(
        guid, BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes);
  }
  return node;
}

void DoOpenBookmark(Profile* profile,
                    WindowOpenDisposition disposition,
                    const BookmarkNode* node) {
  DCHECK(profile);
  Browser* browser = chrome::FindTabbedBrowser(profile, true);
  if (!browser) {
    browser = Browser::Create(Browser::CreateParams(profile, true));
  }
  OpenURLParams params(node->url(), Referrer(), disposition,
                       ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
  browser->OpenURL(params, /*navigation_handle_callback=*/{});
  RecordBookmarkLaunch(BookmarkLaunchLocation::kTopMenu,
                       profile_metrics::GetBrowserProfileType(profile));
}

// Waits for the BookmarkMergedSurfaceServiceLoaded(), then calls
// DoOpenBookmark() on it.
//
// Owned by itself. Allocate with `new`.
class BookmarkRestorer : public BookmarkMergedSurfaceServiceObserver {
 public:
  BookmarkRestorer(Profile* profile,
                   WindowOpenDisposition disposition,
                   base::Uuid guid);
  ~BookmarkRestorer() override = default;

  // BookmarkMergedSurfaceServiceObserver:
  void BookmarkMergedSurfaceServiceLoaded() override;
  void BookmarkMergedSurfaceServiceBeingDeleted() override;
  void BookmarkNodeAdded(const BookmarkParentFolder& parent,
                         size_t index) override {}
  void BookmarkNodesRemoved(
      const BookmarkParentFolder& parent,
      const base::flat_set<const bookmarks::BookmarkNode*>& nodes) override {}
  void BookmarkNodeMoved(const BookmarkParentFolder& old_parent,
                         size_t old_index,
                         const BookmarkParentFolder& new_parent,
                         size_t new_index) override {}
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override {}
  void BookmarkNodeFaviconChanged(
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkParentFolderChildrenReordered(
      const BookmarkParentFolder& folder) override {}
  void BookmarkAllUserNodesRemoved() override {}

 private:
  const raw_ptr<Profile> profile_;
  const WindowOpenDisposition disposition_;
  const base::Uuid guid_;
  raw_ptr<BookmarkMergedSurfaceService> bookmark_service_;
  base::ScopedObservation<BookmarkMergedSurfaceService,
                          BookmarkMergedSurfaceServiceObserver>
      observation_{this};
};

BookmarkRestorer::BookmarkRestorer(Profile* profile,
                                   WindowOpenDisposition disposition,
                                   base::Uuid guid)
    : profile_(profile), disposition_(disposition), guid_(guid) {
  bookmark_service_ =
      BookmarkMergedSurfaceServiceFactory::GetForProfile(profile);
  CHECK(bookmark_service_);
  observation_.Observe(bookmark_service_);
}

void BookmarkRestorer::BookmarkMergedSurfaceServiceLoaded() {
  const BookmarkModel* model = bookmark_service_->bookmark_model();
  if (const BookmarkNode* node = GetNodeByUuid(model, guid_)) {
    DoOpenBookmark(profile_, disposition_, node);
  }
  delete this;
}

void BookmarkRestorer::BookmarkMergedSurfaceServiceBeingDeleted() {
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

  BookmarkMergedSurfaceService* bookmark_service =
      BookmarkMergedSurfaceServiceFactory::GetForProfile(profile);
  CHECK(bookmark_service);

  if (!bookmark_service->loaded()) {
    // Bookmark service hasn't loaded yet. Wait for
    // BookmarkMergedSurfaceServiceLoaded(), and *then* open it.
    std::ignore = new BookmarkRestorer(profile, disposition, std::move(guid));
    return;
  }

  const BookmarkNode* node =
      GetNodeByUuid(bookmark_service->bookmark_model(), guid);
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
  if (node->is_folder()) {
    return title;
  }
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
  Profile* profile = _bridge->GetProfile();
  if (!profile) {
    // Unfortunately, we can't update a menu with a dead profile.
    return;
  }

  // The root menu does not have a corresponding bookmark node.
  if (_bridge->IsMenuRoot(menu)) {
    _bridge->UpdateRootMenuIfInvalid();
    return;
  }

  // Find the bookmark node corresponding to this menu.
  const BookmarkModel* model =
      BookmarkMergedSurfaceServiceFactory::GetForProfile(profile)
          ->bookmark_model();
  NSMenuItem* item = GetItemWithSubmenu(menu);
  base::Uuid guid = _bridge->TagToGUID([item tag]);
  const BookmarkNode* node = GetNodeByUuid(model, guid);

  if (!(node && node->is_folder())) {
    // TODO(crbug.com/417269167): every non-root menu must correspond to a
    // bookmark folder by construction.
    SCOPED_CRASH_KEY_NUMBER("BookmarkMenuCocoaController", "NSMenuItem",
                            [item tag]);
    SCOPED_CRASH_KEY_STRING32("BookmarkMenuCocoaController", "NSMenuItem",
                              base::SysNSStringToUTF8([item title]));
    base::debug::DumpWithoutCrashing();
    return;
  }

  _bridge->UpdateNonRootMenu(menu, BookmarkParentFolder::FromFolderNode(node));
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
