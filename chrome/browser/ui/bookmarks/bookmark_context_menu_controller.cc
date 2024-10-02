// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_context_menu_controller.h"

#include <stddef.h>

#include <string>

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

constexpr UserMetricsAction kBookmarkBarNewBackgroundTab(
    "BookmarkBar_ContextMenu_OpenAll");
constexpr UserMetricsAction kBookmarkBarNewWindow(
    "BookmarkBar_ContextMenu_OpenAllInNewWindow");
constexpr UserMetricsAction kBookmarkBarIncognito(
    "BookmarkBar_ContextMenu_OpenAllIncognito");
constexpr UserMetricsAction kAppMenuBookmarksNewBackgroundTab(
    "WrenchMenu_Bookmarks_ContextMenu_OpenAll");
constexpr UserMetricsAction kAppMenuBookmarksNewWindow(
    "WrenchMenu_Bookmarks_ContextMenu_OpenAllInNewWindow");
constexpr UserMetricsAction kAppMenuBookmarksIncognito(
    "WrenchMenu_Bookmarks_ContextMenu_OpenAllIncognito");
constexpr UserMetricsAction kSidePanelBookmarksNewBackgroundTab(
    "SidePanel_Bookmarks_ContextMenu_OpenAll");
constexpr UserMetricsAction kSidePanelBookmarksNewWindow(
    "SidePanel_Bookmarks_ContextMenu_OpenAllInNewWindow");
constexpr UserMetricsAction kSidePanelBookmarksIncognito(
    "SidePanel_Bookmarks_ContextMenu_OpenAllIncognito");

const UserMetricsAction* GetActionForLocationAndDisposition(
    BookmarkLaunchLocation location,
    WindowOpenDisposition disposition) {
  switch (location) {
    case BookmarkLaunchLocation::kAttachedBar:
      switch (disposition) {
        case WindowOpenDisposition::NEW_BACKGROUND_TAB:
          return &kBookmarkBarNewBackgroundTab;
        case WindowOpenDisposition::NEW_WINDOW:
          return &kBookmarkBarNewWindow;
        case WindowOpenDisposition::OFF_THE_RECORD:
          return &kBookmarkBarIncognito;
        default:
          return nullptr;
      }
    case BookmarkLaunchLocation::kAppMenu:
      switch (disposition) {
        case WindowOpenDisposition::NEW_BACKGROUND_TAB:
          return &kAppMenuBookmarksNewBackgroundTab;
        case WindowOpenDisposition::NEW_WINDOW:
          return &kAppMenuBookmarksNewWindow;
        case WindowOpenDisposition::OFF_THE_RECORD:
          return &kAppMenuBookmarksIncognito;
        default:
          return nullptr;
      }
    case BookmarkLaunchLocation::kSidePanelContextMenu:
      switch (disposition) {
        case WindowOpenDisposition::NEW_BACKGROUND_TAB:
          return &kSidePanelBookmarksNewBackgroundTab;
        case WindowOpenDisposition::NEW_WINDOW:
          return &kSidePanelBookmarksNewWindow;
        case WindowOpenDisposition::OFF_THE_RECORD:
          return &kSidePanelBookmarksIncognito;
        default:
          return nullptr;
      }
    default:
      return nullptr;
  }
}

bool IsNodeManaged(bookmarks::ManagedBookmarkService* managed_service,
                   const BookmarkNode* node) {
  return managed_service && managed_service->IsNodeManaged(node);
}

}  // namespace

BookmarkContextMenuController::BookmarkContextMenuController(
    gfx::NativeWindow parent_window,
    BookmarkContextMenuControllerDelegate* delegate,
    Browser* browser,
    Profile* profile,
    BookmarkLaunchLocation opened_from,
    const BookmarkNode* parent,
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>&
        selection)
    : parent_window_(parent_window),
      delegate_(delegate),
      browser_(browser),
      profile_(profile),
      opened_from_(opened_from),
      parent_(parent),
      selection_(selection),
      bookmark_merged_surface_service_(
          BookmarkMergedSurfaceServiceFactory::GetForProfile(profile)),
      managed_bookmark_service_(
          ManagedBookmarkServiceFactory::GetForProfile(profile)) {
  DCHECK(profile_);
  DCHECK(bookmark_merged_surface_service_->loaded());
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  bookmark_merged_surface_service_->bookmark_model()->AddObserver(this);

  BuildMenu();
}

BookmarkContextMenuController::~BookmarkContextMenuController() {
  bookmark_merged_surface_service_->bookmark_model()->RemoveObserver(this);
}

void BookmarkContextMenuController::BuildMenu() {
  if (selection_.size() == 1 && selection_[0]->is_url()) {
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL, IDS_BOOKMARK_BAR_OPEN_IN_NEW_TAB);
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW,
            IDS_BOOKMARK_BAR_OPEN_IN_NEW_WINDOW);
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO,
            IDS_BOOKMARK_BAR_OPEN_INCOGNITO);
  } else {
    int count = chrome::OpenCount(parent_window_, selection_);
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL,
            l10n_util::GetPluralStringFUTF16(IDS_BOOKMARK_BAR_OPEN_ALL_COUNT,
                                             count));
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW,
            l10n_util::GetPluralStringFUTF16(
                IDS_BOOKMARK_BAR_OPEN_ALL_COUNT_NEW_WINDOW, count));

    int incognito_count =
        chrome::OpenCount(parent_window_, selection_, profile_);
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO,
            l10n_util::GetPluralStringFUTF16(
                IDS_BOOKMARK_BAR_OPEN_ALL_COUNT_INCOGNITO, incognito_count));

    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP,
            l10n_util::GetPluralStringFUTF16(
                IDS_BOOKMARK_BAR_OPEN_ALL_COUNT_NEW_TAB_GROUP, count));
  }

  AddSeparator();
  if (selection_.size() == 1 && selection_[0]->is_folder()) {
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
  if (tab_groups::SavedTabGroupUtils::IsEnabledForProfile(profile_) &&
      tab_groups::IsTabGroupsSaveUIUpdateEnabled()) {
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
    case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW: {
      WindowOpenDisposition initial_disposition;
      if (id == IDC_BOOKMARK_BAR_OPEN_ALL ||
          id == IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP) {
        initial_disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
      } else if (id == IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW) {
        initial_disposition = WindowOpenDisposition::NEW_WINDOW;
      } else {
        initial_disposition = WindowOpenDisposition::OFF_THE_RECORD;
      }
      const UserMetricsAction* const action =
          GetActionForLocationAndDisposition(opened_from_, initial_disposition);
      if (action) {
        base::RecordAction(*action);
      }

      chrome::OpenAllIfAllowed(browser_, selection_, initial_disposition,
                               id == IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP);
      break;
    }

    case IDC_BOOKMARK_BAR_RENAME_FOLDER:
    case IDC_BOOKMARK_BAR_EDIT:
      base::RecordAction(UserMetricsAction("BookmarkBar_ContextMenu_Edit"));
      RecordBookmarkEdited(opened_from_);

      if (selection_.size() != 1) {
        NOTREACHED_IN_MIGRATION();
        break;
      }

      BookmarkEditor::Show(parent_window_, profile_,
                           BookmarkEditor::EditDetails::EditNode(selection_[0]),
                           selection_[0]->is_url() ? BookmarkEditor::SHOW_TREE
                                                   : BookmarkEditor::NO_TREE);
      break;

    case IDC_BOOKMARK_BAR_ADD_TO_BOOKMARKS_BAR: {
      base::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_AddToBookmarkBar"));
      for (const bookmarks::BookmarkNode* node : selection_) {
        bookmark_merged_surface_service_->Move(
            node, BookmarkParentFolder::BookmarkBarFolder(),
            bookmark_merged_surface_service_->GetChildrenCount(
                BookmarkParentFolder::BookmarkBarFolder()));
      }
      break;
    }

    case IDC_BOOKMARK_BAR_REMOVE_FROM_BOOKMARKS_BAR: {
      base::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_RemoveFromBookmarkBar"));
      for (const bookmarks::BookmarkNode* node : selection_) {
        bookmark_merged_surface_service_->Move(
            node, BookmarkParentFolder::OtherFolder(),
            bookmark_merged_surface_service_->GetChildrenCount(
                BookmarkParentFolder::OtherFolder()));
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
          bookmark_merged_surface_service_->bookmark_model());
      for (const bookmarks::BookmarkNode* node : selection_) {
        bookmark_merged_surface_service_->bookmark_model()->Remove(
            node, bookmarks::metrics::BookmarkEditSource::kUser, FROM_HERE);
      }
      selection_.clear();
      break;
    }

    case IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK: {
      base::RecordAction(UserMetricsAction("BookmarkBar_ContextMenu_Add"));

      size_t index;
      const BookmarkNode* parent =
          bookmarks::GetParentForNewNodes(parent_, selection_, &index);
      GURL url;
      std::u16string title;
      if (!chrome::GetURLAndTitleToBookmark(
              browser_->tab_strip_model()->GetActiveWebContents(), &url,
              &title)) {
        break;
      }
      BookmarkEditor::Show(parent_window_, profile_,
                           BookmarkEditor::EditDetails::AddNodeInFolder(
                               parent, index, url, title),
                           BookmarkEditor::SHOW_TREE);
      break;
    }

    case IDC_BOOKMARK_BAR_NEW_FOLDER: {
      base::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_NewFolder"));

      size_t index;
      const BookmarkNode* parent =
          bookmarks::GetParentForNewNodes(parent_, selection_, &index);
      BookmarkEditor::Show(
          parent_window_, profile_,
          BookmarkEditor::EditDetails::AddFolder(parent, index),
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
      if (selection_.size() != 1) {
        chrome::ShowBookmarkManager(browser_);
      } else if (selection_[0]->is_folder()) {
        chrome::ShowBookmarkManagerForNode(browser_, selection_[0]->id());
      } else if (parent_) {
        chrome::ShowBookmarkManagerForNode(browser_, parent_->id());
      } else {
        chrome::ShowBookmarkManager(browser_);
      }
      break;
    }

    case IDC_CUT:
      bookmarks::CopyToClipboard(
          bookmark_merged_surface_service_->bookmark_model(), selection_, true,
          bookmarks::metrics::BookmarkEditSource::kUser,
          profile_->IsOffTheRecord());
      break;

    case IDC_COPY:
      bookmarks::CopyToClipboard(
          bookmark_merged_surface_service_->bookmark_model(), selection_, false,
          bookmarks::metrics::BookmarkEditSource::kUser,
          profile_->IsOffTheRecord());
      break;

    case IDC_PASTE: {
      // TODO(b/369304373): Update `PasteFromClipboard` to accept/handle a
      // `BookmarkParentFolder::PermanentFolderType` for merged surfaces.
      size_t index;
      const BookmarkNode* paste_target =
          bookmarks::GetParentForNewNodes(parent_, selection_, &index);
      if (!paste_target) {
        return;
      }

      bookmarks::PasteFromClipboard(
          bookmark_merged_surface_service_->bookmark_model(), paste_target,
          index);
      break;
    }

    default:
      NOTREACHED_IN_MIGRATION();
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

  NOTREACHED_IN_MIGRATION();
  return std::u16string();
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

  // TODO(b/369304373): Update to handle `selection_` containing 2 permanent
  // nodes.
  bool is_root_node =
      selection_.size() == 1 && selection_[0]->is_permanent_node();
  bool can_edit =
      prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled) &&
      chrome::CanAllBeEditedByUser(managed_bookmark_service_, selection_);
  policy::IncognitoModeAvailability incognito_avail =
      IncognitoModePrefs::GetAvailability(prefs);

  switch (command_id) {
    case IDC_BOOKMARK_BAR_OPEN_INCOGNITO:
      return !profile_->IsOffTheRecord() &&
             incognito_avail != policy::IncognitoModeAvailability::kDisabled;

    case IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO:
      return chrome::HasBookmarkURLsAllowedInIncognitoMode(selection_,
                                                           profile_) &&
             !profile_->IsOffTheRecord() &&
             incognito_avail != policy::IncognitoModeAvailability::kDisabled;
    case IDC_BOOKMARK_BAR_OPEN_ALL:
    case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP:
      return chrome::HasBookmarkURLs(selection_);
    case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW:
      return chrome::HasBookmarkURLs(selection_) &&
             incognito_avail != policy::IncognitoModeAvailability::kForced;

    case IDC_BOOKMARK_BAR_RENAME_FOLDER:
    case IDC_BOOKMARK_BAR_EDIT:
      return selection_.size() == 1 && !is_root_node && can_edit;

    case IDC_BOOKMARK_BAR_ADD_TO_BOOKMARKS_BAR:
      for (const bookmarks::BookmarkNode* node : selection_) {
        if (node->is_permanent_node() ||
            bookmark_merged_surface_service_->IsPermanentNodeOfType(
                node->parent(), PermanentFolderType::kBookmarkBarNode)) {
          return false;
        }
      }
      return can_edit && !IsNodeManaged(managed_bookmark_service_, parent_);
    case IDC_BOOKMARK_BAR_REMOVE_FROM_BOOKMARKS_BAR:
      for (const bookmarks::BookmarkNode* node : selection_) {
        if (node->is_permanent_node() ||
            !bookmark_merged_surface_service_->IsPermanentNodeOfType(
                node->parent(), PermanentFolderType::kBookmarkBarNode)) {
          return false;
        }
      }
      return can_edit && !IsNodeManaged(managed_bookmark_service_, parent_);

    case IDC_BOOKMARK_BAR_UNDO:
      return can_edit && BookmarkUndoServiceFactory::GetForProfile(profile_)
                                 ->undo_manager()
                                 ->undo_count() > 0;

    case IDC_BOOKMARK_BAR_REDO:
      return can_edit && BookmarkUndoServiceFactory::GetForProfile(profile_)
                                 ->undo_manager()
                                 ->redo_count() > 0;

    case IDC_BOOKMARK_BAR_REMOVE:
      return !selection_.empty() && !is_root_node && can_edit;

    case IDC_BOOKMARK_BAR_NEW_FOLDER:
    case IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK:
      return can_edit && !IsNodeManaged(managed_bookmark_service_, parent_) &&
             bookmarks::GetParentForNewNodes(parent_, selection_, nullptr);

    case IDC_BOOKMARK_BAR_ALWAYS_SHOW:
      return !prefs->IsManagedPreference(bookmarks::prefs::kShowBookmarkBar);

    case IDC_BOOKMARK_BAR_SHOW_APPS_SHORTCUT:
      return !prefs->IsManagedPreference(
          bookmarks::prefs::kShowAppsShortcutInBookmarkBar);

    case IDC_COPY:
    case IDC_CUT:
      return !selection_.empty() && !is_root_node &&
             (command_id == IDC_COPY || can_edit);

    case IDC_PASTE:
      // Paste to selection from the Bookmark Bar, to parent_ everywhere else
      return can_edit &&
             ((!selection_.empty() &&
               bookmarks::CanPasteFromClipboard(
                   bookmark_merged_surface_service_->bookmark_model(),
                   selection_[0])) ||
              bookmarks::CanPasteFromClipboard(
                  bookmark_merged_surface_service_->bookmark_model(), parent_));
  }
  return true;
}

bool BookmarkContextMenuController::IsCommandIdVisible(int command_id) const {
  if (command_id == IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS) {
    // The option to hide the Managed Bookmarks folder is only available if
    // there are any managed bookmarks configured at all.
    return !managed_bookmark_service_->managed_node()->children().empty();
  }

  return true;
}

void BookmarkContextMenuController::BookmarkModelChanged() {
  if (delegate_) {
    delegate_->CloseMenu();
  }
}
