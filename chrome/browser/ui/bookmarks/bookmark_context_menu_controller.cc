// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_context_menu_controller.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/bookmark_ui_operations_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/undo/bookmark_undo_service.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;
using bookmarks::BookmarkNode;
using PermanentFolderType = BookmarkParentFolder::PermanentFolderType;

namespace {

constexpr UserMetricsAction kBookmarkBarOpenAll(
    "BookmarkBar_ContextMenu_OpenAll");
constexpr UserMetricsAction kBookmarkBarNewWindow(
    "BookmarkBar_ContextMenu_OpenAllInNewWindow");
constexpr UserMetricsAction kBookmarkBarIncognito(
    "BookmarkBar_ContextMenu_OpenAllIncognito");
constexpr UserMetricsAction kBookmarkBarOpenAllInNewTabGroup(
    "BookmarkBar_ContextMenu_OpenAllInNewTabGroup");
constexpr UserMetricsAction kBookmarkBarOpenSplitView(
    "BookmarkBar_ContextMenu_OpenSplitView");
constexpr UserMetricsAction kAppMenuBookmarksOpenAll(
    "WrenchMenu_Bookmarks_ContextMenu_OpenAll");
constexpr UserMetricsAction kAppMenuBookmarksNewWindow(
    "WrenchMenu_Bookmarks_ContextMenu_OpenAllInNewWindow");
constexpr UserMetricsAction kAppMenuBookmarksIncognito(
    "WrenchMenu_Bookmarks_ContextMenu_OpenAllIncognito");
constexpr UserMetricsAction kAppMenuBookmarksOpenAllInNewTabGroup(
    "WrenchMenu_Bookmarks_ContextMenu_OpenAllInNewTabGroup");
constexpr UserMetricsAction kAppMenuBookmarksOpenSplitView(
    "WrenchMenu_Bookmarks_ContextMenu_OpenSplitView");
constexpr UserMetricsAction kSidePanelBookmarksOpenAll(
    "SidePanel_Bookmarks_ContextMenu_OpenAll");
constexpr UserMetricsAction kSidePanelBookmarksNewWindow(
    "SidePanel_Bookmarks_ContextMenu_OpenAllInNewWindow");
constexpr UserMetricsAction kSidePanelBookmarksIncognito(
    "SidePanel_Bookmarks_ContextMenu_OpenAllIncognito");
constexpr UserMetricsAction kSidePanelBookmarksOpenAllInNewTabGroup(
    "SidePanel_Bookmarks_ContextMenu_OpenAllInNewTabGroup");
constexpr UserMetricsAction kSidePanelBookmarksOpenSplitView(
    "SidePanel_Bookmarks_ContextMenu_OpenSplitView");

const UserMetricsAction* GetActionForLocationAndDisposition(
    int command_id,
    BookmarkLaunchLocation location) {
  switch (location) {
    case BookmarkLaunchLocation::kAttachedBar:
      switch (command_id) {
        case IDC_BOOKMARK_BAR_OPEN_ALL:
          return &kBookmarkBarOpenAll;
        case IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO:
          return &kBookmarkBarIncognito;
        case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP:
          return &kBookmarkBarOpenAllInNewTabGroup;
        case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW:
          return &kBookmarkBarNewWindow;
        case IDC_BOOKMARK_BAR_OPEN_SPLIT_VIEW:
          return &kBookmarkBarOpenSplitView;
      }
    case BookmarkLaunchLocation::kAppMenu:
      switch (command_id) {
        case IDC_BOOKMARK_BAR_OPEN_ALL:
          return &kAppMenuBookmarksOpenAll;
        case IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO:
          return &kAppMenuBookmarksIncognito;
        case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP:
          return &kAppMenuBookmarksOpenAllInNewTabGroup;
        case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW:
          return &kAppMenuBookmarksNewWindow;
        case IDC_BOOKMARK_BAR_OPEN_SPLIT_VIEW:
          return &kAppMenuBookmarksOpenSplitView;
      }
    case BookmarkLaunchLocation::kSidePanelContextMenu:
      switch (command_id) {
        case IDC_BOOKMARK_BAR_OPEN_ALL:
          return &kSidePanelBookmarksOpenAll;
        case IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO:
          return &kSidePanelBookmarksIncognito;
        case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP:
          return &kSidePanelBookmarksOpenAllInNewTabGroup;
        case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW:
          return &kSidePanelBookmarksNewWindow;
        case IDC_BOOKMARK_BAR_OPEN_SPLIT_VIEW:
          return &kSidePanelBookmarksOpenSplitView;
      }
    default:
      return nullptr;
  }
}

// Check selection is not empty, nodes are not null nor repeated.
void CheckSelectionIsValid(
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>&
        selection) {
  CHECK(!selection.empty());
  // Nodes must be not null.
  CHECK(std::ranges::all_of(selection,
                            [](const BookmarkNode* node) { return node; }));

  // Check not repeated nodes.
  std::set<const BookmarkNode*> nodes_set(selection.begin(), selection.end());
  CHECK_EQ(selection.size(), nodes_set.size());
}

}  // namespace

BookmarkContextMenuController::BookmarkContextMenuController(
    gfx::NativeWindow parent_window,
    BookmarkContextMenuControllerDelegate* delegate,
    Browser* browser,
    Profile* profile,
    BookmarkLaunchLocation opened_from,
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>&
        selection)
    : parent_window_(parent_window),
      delegate_(delegate),
      browser_(browser),
      profile_(profile),
      opened_from_(opened_from),
      selection_(selection),
      bookmark_service_(
          BookmarkMergedSurfaceServiceFactory::GetForProfile(profile)),
      new_nodes_parent_(GetParentForNewNodes(selection)) {
  DCHECK(profile_);
  DCHECK(bookmark_service_->loaded());
  CheckSelectionIsValid(selection);
  CHECK(new_nodes_parent_);
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  bookmark_service_->bookmark_model()->AddObserver(this);

  BuildMenu();
}

BookmarkContextMenuController::~BookmarkContextMenuController() {
  bookmark_service_->bookmark_model()->RemoveObserver(this);
}

// static
std::unique_ptr<BookmarkParentFolder>
BookmarkContextMenuController::GetParentForNewNodes(
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>&
        selection) {
  if (selection.empty()) {
    return nullptr;
  }

  const BookmarkNode* parent = nullptr;
  if (IsSelectionPermanentBookmarkFolder(selection)) {
    // Context menu is showing for a
    // `BookmarkParentFolder::PermanentFolderType`.
    parent = selection[0];
  } else if (selection.size() == 1 && selection[0]->is_folder()) {
    // If context menu is showing for a folder, new bookmarks will be added
    // within this folder.
    parent = selection[0];
  } else {
    parent = selection[0]->parent();
  }

  CHECK(!parent->is_root());
  return std::make_unique<BookmarkParentFolder>(
      BookmarkParentFolder::FromFolderNode(parent));
}

size_t BookmarkContextMenuController::GetIndexForNewNodes() const {
  // Add the node after the item for which the context menu is showing.
  if (selection_.size() == 1 && selection_[0]->is_url()) {
    CHECK(*new_nodes_parent_ ==
          BookmarkParentFolder::FromFolderNode(selection_[0]->parent()));
    std::optional<size_t> selection_index =
        bookmark_service_->GetIndexOf(selection_[0]);
    CHECK(selection_index.has_value());
    return selection_index.value() + 1;
  }
  return bookmark_service_->GetChildrenCount(*new_nodes_parent_);
}

void BookmarkContextMenuController::BuildMenu() {
  if (selection_.size() == 1 && selection_[0]->is_url()) {
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL, IDS_BOOKMARK_BAR_OPEN_IN_NEW_TAB);
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW,
            IDS_BOOKMARK_BAR_OPEN_IN_NEW_WINDOW);
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO,
            IDS_BOOKMARK_BAR_OPEN_INCOGNITO);
    if (base::FeatureList::IsEnabled(features::kSideBySide)) {
      AddItem(IDC_BOOKMARK_BAR_OPEN_SPLIT_VIEW,
              IDS_BOOKMARK_BAR_OPEN_IN_SPLIT_VIEW);
    }
  } else {
    int count = bookmarks::OpenCount(selection_);
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL,
            l10n_util::GetPluralStringFUTF16(IDS_BOOKMARK_BAR_OPEN_ALL_COUNT,
                                             count));
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW,
            l10n_util::GetPluralStringFUTF16(
                IDS_BOOKMARK_BAR_OPEN_ALL_COUNT_NEW_WINDOW, count));

    int incognito_count = bookmarks::OpenCount(selection_, profile_);
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO,
            l10n_util::GetPluralStringFUTF16(
                IDS_BOOKMARK_BAR_OPEN_ALL_COUNT_INCOGNITO, incognito_count));

    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP,
            l10n_util::GetPluralStringFUTF16(
                IDS_BOOKMARK_BAR_OPEN_ALL_COUNT_NEW_TAB_GROUP, count));
  }

  AddSeparator();
  // Permanent folders representation should show the `Rename` option, that will
  // be disabled.
  if ((selection_.size() == 1 && selection_[0]->is_folder()) ||
      IsSelectionPermanentBookmarkFolder(selection_)) {
    AddItem(IDC_BOOKMARK_BAR_RENAME_FOLDER, IDS_BOOKMARK_BAR_RENAME_FOLDER);
  } else {
    AddItem(IDC_BOOKMARK_BAR_EDIT, IDS_BOOKMARK_BAR_EDIT);
  }

  AddSeparator();
  AddItem(IDC_CUT, IDS_CUT);
  AddItem(IDC_COPY, IDS_COPY);
  AddItem(IDC_PASTE, IDS_PASTE);

  AddSeparator();
  AddItem(IDC_BOOKMARK_BAR_REMOVE, IDS_BOOKMARK_BAR_REMOVE);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBookmarkUndo)) {
    AddItem(IDC_BOOKMARK_BAR_UNDO, IDS_BOOKMARK_BAR_UNDO);
    AddItem(IDC_BOOKMARK_BAR_REDO, IDS_BOOKMARK_BAR_REDO);
  }

  AddSeparator();
  AddItem(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK, IDS_BOOKMARK_BAR_ADD_NEW_BOOKMARK);
  AddItem(IDC_BOOKMARK_BAR_NEW_FOLDER, IDS_BOOKMARK_BAR_NEW_FOLDER);

  AddSeparator();
  AddItem(IDC_BOOKMARK_MANAGER, IDS_BOOKMARK_MANAGER);
  // Use the native host desktop type in tests.
  if (chrome::IsAppsShortcutEnabled(profile_)) {
    AddCheckboxItem(IDC_BOOKMARK_BAR_SHOW_APPS_SHORTCUT,
                    IDS_BOOKMARK_BAR_SHOW_APPS_SHORTCUT);
  }
  if (tab_groups::SavedTabGroupUtils::IsEnabledForProfile(profile_)) {
    AddCheckboxItem(IDC_BOOKMARK_BAR_TOGGLE_SHOW_TAB_GROUPS,
                    IDS_BOOKMARK_BAR_SHOW_TAB_GROUPS);
  }
  AddCheckboxItem(IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS,
                  IDS_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS_DEFAULT_NAME);
  AddCheckboxItem(IDC_BOOKMARK_BAR_ALWAYS_SHOW, IDS_SHOW_BOOKMARK_BAR);
}

void BookmarkContextMenuController::AddItem(int id, const std::u16string str) {
  menu_model_->AddItem(id, str);
}

void BookmarkContextMenuController::AddItem(int id, int localization_id) {
  menu_model_->AddItemWithStringId(id, localization_id);
}

void BookmarkContextMenuController::AddSeparator() {
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
}

void BookmarkContextMenuController::AddCheckboxItem(int id,
                                                    int localization_id) {
  menu_model_->AddCheckItemWithStringId(id, localization_id);
}

void BookmarkContextMenuController::ExecuteCommand(int id, int event_flags) {
  if (delegate_) {
    delegate_->WillExecuteCommand(id, selection_);
  }

  base::WeakPtr<BookmarkContextMenuController> ref(weak_factory_.GetWeakPtr());

  switch (id) {
    case IDC_BOOKMARK_BAR_OPEN_ALL:
    case IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO:
    case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP:
    case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW:
    case IDC_BOOKMARK_BAR_OPEN_SPLIT_VIEW: {
      WindowOpenDisposition initial_disposition;
      if (id == IDC_BOOKMARK_BAR_OPEN_ALL ||
          id == IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP ||
          id == IDC_BOOKMARK_BAR_OPEN_SPLIT_VIEW) {
        initial_disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
      } else if (id == IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW) {
        initial_disposition = WindowOpenDisposition::NEW_WINDOW;
      } else {
        initial_disposition = WindowOpenDisposition::OFF_THE_RECORD;
      }
      const UserMetricsAction* const action =
          GetActionForLocationAndDisposition(id, opened_from_);
      if (action) {
        base::RecordAction(*action);
      }

      bookmarks::OpenAllBookmarksContext context =
          bookmarks::OpenAllBookmarksContext::kNone;
      if (id == IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP) {
        context = bookmarks::OpenAllBookmarksContext::kInGroup;
      } else if (id == IDC_BOOKMARK_BAR_OPEN_SPLIT_VIEW) {
        context = bookmarks::OpenAllBookmarksContext::kInSplit;
      }
      bookmarks::OpenAllIfAllowed(browser_, selection_, initial_disposition,
                                  context);
      break;
    }

    case IDC_BOOKMARK_BAR_RENAME_FOLDER:
    case IDC_BOOKMARK_BAR_EDIT:
      base::RecordAction(UserMetricsAction("BookmarkBar_ContextMenu_Edit"));
      RecordBookmarkEdited(opened_from_);

      if (selection_.size() != 1) {
        NOTREACHED();
      }

      BookmarkEditor::Show(parent_window_, profile_,
                           BookmarkEditor::EditDetails::EditNode(selection_[0]),
                           selection_[0]->is_url() ? BookmarkEditor::SHOW_TREE
                                                   : BookmarkEditor::NO_TREE);
      break;

    case IDC_BOOKMARK_BAR_MOVE:
      base::RecordAction(UserMetricsAction("BookmarkBar_ContextMenu_Move"));

      BookmarkEditor::Show(parent_window_, profile_,
                           BookmarkEditor::EditDetails::MoveNodes(
                               bookmark_service_->bookmark_model(), selection_),
                           BookmarkEditor::SHOW_TREE);
      break;

    case IDC_BOOKMARK_BAR_ADD_TO_BOOKMARKS_BAR: {
      base::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_AddToBookmarkBar"));
      for (const bookmarks::BookmarkNode* node : selection_) {
        bookmark_service_->Move(node, BookmarkParentFolder::BookmarkBarFolder(),
                                bookmark_service_->GetChildrenCount(
                                    BookmarkParentFolder::BookmarkBarFolder()),
                                browser_);
      }
      break;
    }

    case IDC_BOOKMARK_BAR_REMOVE_FROM_BOOKMARKS_BAR: {
      base::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_RemoveFromBookmarkBar"));
      for (const bookmarks::BookmarkNode* node : selection_) {
        bookmark_service_->Move(node, BookmarkParentFolder::OtherFolder(),
                                bookmark_service_->GetChildrenCount(
                                    BookmarkParentFolder::OtherFolder()),
                                browser_);
      }
      break;
    }

    case IDC_BOOKMARK_BAR_UNDO: {
      base::RecordAction(UserMetricsAction("BookmarkBar_ContextMenu_Undo"));
      BookmarkUndoServiceFactory::GetForProfile(profile_)
          ->undo_manager()
          ->Undo();
      break;
    }

    case IDC_BOOKMARK_BAR_REDO: {
      base::RecordAction(UserMetricsAction("BookmarkBar_ContextMenu_Redo"));
      BookmarkUndoServiceFactory::GetForProfile(profile_)
          ->undo_manager()
          ->Redo();
      break;
    }

    case IDC_BOOKMARK_BAR_REMOVE: {
      base::RecordAction(UserMetricsAction("BookmarkBar_ContextMenu_Remove"));
      RecordBookmarkRemoved(opened_from_);

      bookmarks::ScopedGroupBookmarkActions group_remove(
          bookmark_service_->bookmark_model());
      for (const bookmarks::BookmarkNode* node : selection_) {
        bookmark_service_->bookmark_model()->Remove(
            node, bookmarks::metrics::BookmarkEditSource::kUser, FROM_HERE);
      }
      selection_.clear();
      break;
    }

    case IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK: {
      base::RecordAction(UserMetricsAction("BookmarkBar_ContextMenu_Add"));

      GURL url;
      std::u16string title;
      if (!chrome::GetURLAndTitleToBookmark(
              browser_->tab_strip_model()->GetActiveWebContents(), &url,
              &title)) {
        break;
      }
      BookmarkEditor::Show(parent_window_, profile_,
                           BookmarkEditor::EditDetails::AddNodeInFolder(
                               BookmarkUIOperationsHelperMergedSurfaces(
                                   bookmark_service_, new_nodes_parent_.get())
                                   .GetDefaultParentForNonMergedSurfaces(),
                               GetIndexForNewNodes(), url, title),
                           BookmarkEditor::SHOW_TREE);
      break;
    }

    case IDC_BOOKMARK_BAR_NEW_FOLDER: {
      base::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_NewFolder"));

      BookmarkEditor::Show(parent_window_, profile_,
                           BookmarkEditor::EditDetails::AddFolder(
                               BookmarkUIOperationsHelperMergedSurfaces(
                                   bookmark_service_, new_nodes_parent_.get())
                                   .GetDefaultParentForNonMergedSurfaces(),
                               GetIndexForNewNodes()),
                           BookmarkEditor::SHOW_TREE);
      break;
    }

    case IDC_BOOKMARK_BAR_ALWAYS_SHOW:
      chrome::ToggleBookmarkBarWhenVisible(profile_);
      break;

    case IDC_BOOKMARK_BAR_SHOW_APPS_SHORTCUT: {
      PrefService* prefs = profile_->GetPrefs();
      prefs->SetBoolean(
          bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
          !prefs->GetBoolean(bookmarks::prefs::kShowAppsShortcutInBookmarkBar));
      break;
    }

    case IDC_BOOKMARK_BAR_TOGGLE_SHOW_TAB_GROUPS: {
      base::RecordAction(base::UserMetricsAction(
          "BookmarkBar_ContextMenu_ToggleShowSavedTabGroups"));
      PrefService* prefs = profile_->GetPrefs();
      prefs->SetBoolean(
          bookmarks::prefs::kShowTabGroupsInBookmarkBar,
          !prefs->GetBoolean(bookmarks::prefs::kShowTabGroupsInBookmarkBar));
      break;
    }

    case IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS: {
      PrefService* prefs = profile_->GetPrefs();
      prefs->SetBoolean(
          bookmarks::prefs::kShowManagedBookmarksInBookmarkBar,
          !prefs->GetBoolean(
              bookmarks::prefs::kShowManagedBookmarksInBookmarkBar));
      break;
    }

    case IDC_BOOKMARK_MANAGER: {
      const bookmarks::BookmarkNode* node_to_focus =
          ComputeNodeToFocusForBookmarkManager();
      if (node_to_focus) {
        chrome::ShowBookmarkManagerForNode(browser_, node_to_focus->id());
      } else {
        chrome::ShowBookmarkManager(browser_);
      }
      break;
    }

    case IDC_CUT:
      BookmarkUIOperationsHelperMergedSurfaces::CutToClipboard(
          bookmark_service_->bookmark_model(), selection_,
          bookmarks::metrics::BookmarkEditSource::kUser,
          profile_->IsOffTheRecord());
      break;

    case IDC_COPY:
      BookmarkUIOperationsHelperMergedSurfaces::CopyToClipboard(
          bookmark_service_->bookmark_model(), selection_,
          bookmarks::metrics::BookmarkEditSource::kUser,
          profile_->IsOffTheRecord());
      break;

    case IDC_PASTE: {
      BookmarkUIOperationsHelperMergedSurfaces(bookmark_service_,
                                               new_nodes_parent_.get())
          .PasteFromClipboard(GetIndexForNewNodes());
      break;
    }

    default:
      NOTREACHED();
  }

  // It's possible executing the command resulted in deleting |this|.
  if (!ref) {
    return;
  }

  if (delegate_) {
    delegate_->DidExecuteCommand(id);
  }
}

bool BookmarkContextMenuController::IsItemForCommandIdDynamic(
    int command_id) const {
  return command_id == IDC_BOOKMARK_BAR_UNDO ||
         command_id == IDC_BOOKMARK_BAR_REDO ||
         command_id == IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS;
}

std::u16string BookmarkContextMenuController::GetLabelForCommandId(
    int command_id) const {
  if (command_id == IDC_BOOKMARK_BAR_UNDO) {
    return BookmarkUndoServiceFactory::GetForProfile(profile_)
        ->undo_manager()
        ->GetUndoLabel();
  }
  if (command_id == IDC_BOOKMARK_BAR_REDO) {
    return BookmarkUndoServiceFactory::GetForProfile(profile_)
        ->undo_manager()
        ->GetRedoLabel();
  }
  if (command_id == IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS) {
    bookmarks::ManagedBookmarkService* managed =
        ManagedBookmarkServiceFactory::GetForProfile(profile_);
    return l10n_util::GetStringFUTF16(IDS_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS,
                                      managed->managed_node()->GetTitle());
  }

  NOTREACHED();
}

bool BookmarkContextMenuController::IsCommandIdChecked(int command_id) const {
  PrefService* prefs = profile_->GetPrefs();
  if (command_id == IDC_BOOKMARK_BAR_ALWAYS_SHOW) {
    return prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar);
  }
  if (command_id == IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS) {
    return prefs->GetBoolean(
        bookmarks::prefs::kShowManagedBookmarksInBookmarkBar);
  }
  if (command_id == IDC_BOOKMARK_BAR_TOGGLE_SHOW_TAB_GROUPS) {
    return prefs->GetBoolean(bookmarks::prefs::kShowTabGroupsInBookmarkBar);
  }

  DCHECK_EQ(IDC_BOOKMARK_BAR_SHOW_APPS_SHORTCUT, command_id);
  return prefs->GetBoolean(bookmarks::prefs::kShowAppsShortcutInBookmarkBar);
}

bool BookmarkContextMenuController::IsCommandIdEnabled(int command_id) const {
  PrefService* prefs = profile_->GetPrefs();

  bool is_any_node_permanent = std::ranges::any_of(
      selection_,
      [](const BookmarkNode* node) { return node->is_permanent_node(); });
  bool can_edit =
      prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled) &&
      std::ranges::all_of(selection_, [&](const BookmarkNode* node) {
        return !bookmark_service_->IsNodeManaged(node);
      });

  policy::IncognitoModeAvailability incognito_avail =
      IncognitoModePrefs::GetAvailability(prefs);

  switch (command_id) {
    case IDC_BOOKMARK_BAR_OPEN_INCOGNITO:
      return !profile_->IsOffTheRecord() &&
             incognito_avail != policy::IncognitoModeAvailability::kDisabled;

    case IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO:
      return bookmarks::HasBookmarkURLsAllowedInIncognitoMode(selection_) &&
             !profile_->IsOffTheRecord() &&
             incognito_avail != policy::IncognitoModeAvailability::kDisabled;
    case IDC_BOOKMARK_BAR_OPEN_ALL:
    case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP:
      return bookmarks::HasBookmarkURLs(selection_);
    case IDC_BOOKMARK_BAR_OPEN_SPLIT_VIEW: {
      tabs::TabInterface* active_tab =
          browser_ ? browser_->GetActiveTabInterface() : nullptr;
      return bookmarks::HasBookmarkURLs(selection_) &&
             base::FeatureList::IsEnabled(features::kSideBySide) &&
             active_tab && !active_tab->IsSplit();
    }
    case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW:
      return bookmarks::HasBookmarkURLs(selection_) &&
             incognito_avail != policy::IncognitoModeAvailability::kForced;

    case IDC_BOOKMARK_BAR_RENAME_FOLDER:
    case IDC_BOOKMARK_BAR_EDIT:
      return selection_.size() == 1 && !is_any_node_permanent && can_edit;

    case IDC_BOOKMARK_BAR_MOVE:
      return !is_any_node_permanent && can_edit;

    case IDC_BOOKMARK_BAR_ADD_TO_BOOKMARKS_BAR:
      for (const bookmarks::BookmarkNode* node : selection_) {
        if (node->is_permanent_node() ||
            node->parent()->type() == BookmarkNode::BOOKMARK_BAR) {
          return false;
        }
      }
      return can_edit;
    case IDC_BOOKMARK_BAR_REMOVE_FROM_BOOKMARKS_BAR:
      for (const bookmarks::BookmarkNode* node : selection_) {
        if (node->is_permanent_node() ||
            node->parent()->type() != BookmarkNode::BOOKMARK_BAR) {
          return false;
        }
      }
      return can_edit;

    case IDC_BOOKMARK_BAR_UNDO:
      return can_edit && BookmarkUndoServiceFactory::GetForProfile(profile_)
                                 ->undo_manager()
                                 ->undo_count() > 0;

    case IDC_BOOKMARK_BAR_REDO:
      return can_edit && BookmarkUndoServiceFactory::GetForProfile(profile_)
                                 ->undo_manager()
                                 ->redo_count() > 0;

    case IDC_BOOKMARK_BAR_REMOVE:
      return !selection_.empty() && !is_any_node_permanent && can_edit;

    case IDC_BOOKMARK_BAR_NEW_FOLDER:
    case IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK:
      return can_edit;

    case IDC_BOOKMARK_BAR_ALWAYS_SHOW:
      return !prefs->IsManagedPreference(bookmarks::prefs::kShowBookmarkBar);

    case IDC_BOOKMARK_BAR_SHOW_APPS_SHORTCUT:
      return !prefs->IsManagedPreference(
          bookmarks::prefs::kShowAppsShortcutInBookmarkBar);

    case IDC_COPY:
    case IDC_CUT:
      return !selection_.empty() && !is_any_node_permanent &&
             (command_id == IDC_COPY || can_edit);

    case IDC_PASTE:
      return can_edit && BookmarkUIOperationsHelperMergedSurfaces(
                             bookmark_service_, new_nodes_parent_.get())
                             .CanPasteFromClipboard();
  }
  return true;
}

bool BookmarkContextMenuController::IsCommandIdVisible(int command_id) const {
  if (command_id == IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS) {
    // The option to hide the Managed Bookmarks folder is only available if
    // there are any managed bookmarks configured at all.
    return bookmark_service_->GetChildrenCount(
        BookmarkParentFolder::ManagedFolder());
  }

  return true;
}

void BookmarkContextMenuController::BookmarkModelChanged() {
  if (delegate_) {
    delegate_->CloseMenu();
  }
}

const bookmarks::BookmarkNode*
BookmarkContextMenuController::ComputeNodeToFocusForBookmarkManager() const {
  // If the selection is a permanent folder then query the merged service to
  // deduce which type of permanent node should be focused.
  if (IsSelectionPermanentBookmarkFolder(selection_)) {
    CHECK(new_nodes_parent_);
    return BookmarkUIOperationsHelperMergedSurfaces(bookmark_service_,
                                                    new_nodes_parent_.get())
        .GetDefaultParentForNonMergedSurfaces();
  }

  // If the selection is not specific to a single selection and not tied to a
  // pair of permanent folders then we cannot deduce a node to focus.
  if (selection_.size() != 1) {
    return nullptr;
  }

  // If the previously computed parent is a non permanent folder we can return
  // it directly.
  CHECK(new_nodes_parent_);
  if (new_nodes_parent_->HoldsNonPermanentFolder()) {
    return new_nodes_parent_->as_non_permanent_folder();
  }

  // The selected value is not a folder and the computed parent is a merged
  // permanent node, the direct parent of the node selected should be focused,
  // without relying on the merged service logic, since it may not return the
  // real parent of the selected node.
  CHECK(!selection_[0]->is_folder());
  CHECK(selection_[0]->parent()->is_permanent_node());
  return selection_[0]->parent();
}

bool IsSelectionPermanentBookmarkFolder(
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>&
        selection) {
  if (selection.size() == 1) {
    return selection[0]->is_permanent_node();
  }

  if (selection.size() == 2) {
    return selection[0]->is_permanent_node() &&
           selection[1]->is_permanent_node() &&
           selection[0]->type() == selection[1]->type();
  }
  return false;
}
