// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_context_menu_controller.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/metrics/user_metrics.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/undo/bookmark_undo_service.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;
using bookmarks::BookmarkNode;
using content::PageNavigator;

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

const UserMetricsAction* GetActionForLocationAndDisposition(
    BookmarkLaunchLocation location,
    WindowOpenDisposition disposition) {
  switch (location) {
    case BOOKMARK_LAUNCH_LOCATION_ATTACHED_BAR:
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
      break;
    case BOOKMARK_LAUNCH_LOCATION_APP_MENU:
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
    default:
      return nullptr;
  }
}

}  // namespace

BookmarkContextMenuController::BookmarkContextMenuController(
    gfx::NativeWindow parent_window,
    BookmarkContextMenuControllerDelegate* delegate,
    Browser* browser,
    Profile* profile,
    PageNavigator* navigator,
    BookmarkLaunchLocation opened_from,
    const BookmarkNode* parent,
    const std::vector<const BookmarkNode*>& selection)
    : parent_window_(parent_window),
      delegate_(delegate),
      browser_(browser),
      profile_(profile),
      navigator_(navigator),
      opened_from_(opened_from),
      parent_(parent),
      selection_(selection),
      model_(BookmarkModelFactory::GetForBrowserContext(profile)) {
  DCHECK(profile_);
  DCHECK(model_->loaded());
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  model_->AddObserver(this);

  BuildMenu();
}

BookmarkContextMenuController::~BookmarkContextMenuController() {
  if (model_)
    model_->RemoveObserver(this);
}

void BookmarkContextMenuController::BuildMenu() {
  if (selection_.size() == 1 && selection_[0]->is_url()) {
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL,
            IDS_BOOKMARK_BAR_OPEN_IN_NEW_TAB);
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
  AddCheckboxItem(IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS,
                  IDS_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS_DEFAULT_NAME);
  AddCheckboxItem(IDC_BOOKMARK_BAR_ALWAYS_SHOW, IDS_SHOW_BOOKMARK_BAR);
}

void BookmarkContextMenuController::AddItem(int id, const base::string16 str) {
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
  if (delegate_)
    delegate_->WillExecuteCommand(id, selection_);

  base::WeakPtr<BookmarkContextMenuController> ref(weak_factory_.GetWeakPtr());

  switch (id) {
    case IDC_BOOKMARK_BAR_OPEN_ALL:
    case IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO:
    case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW: {
      WindowOpenDisposition initial_disposition;
      if (id == IDC_BOOKMARK_BAR_OPEN_ALL) {
        initial_disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
      } else if (id == IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW) {
        initial_disposition = WindowOpenDisposition::NEW_WINDOW;
      } else {
        initial_disposition = WindowOpenDisposition::OFF_THE_RECORD;
      }
      const UserMetricsAction* const action =
          GetActionForLocationAndDisposition(opened_from_, initial_disposition);
      if (action)
        base::RecordAction(*action);
      chrome::OpenAll(parent_window_, navigator_, selection_,
                      initial_disposition, profile_);
      break;
    }

    case IDC_BOOKMARK_BAR_RENAME_FOLDER:
    case IDC_BOOKMARK_BAR_EDIT:
      base::RecordAction(UserMetricsAction("BookmarkBar_ContextMenu_Edit"));

      if (selection_.size() != 1) {
        NOTREACHED();
        break;
      }

      BookmarkEditor::Show(
          parent_window_,
          profile_,
          BookmarkEditor::EditDetails::EditNode(selection_[0]),
          selection_[0]->is_url() ? BookmarkEditor::SHOW_TREE :
                                    BookmarkEditor::NO_TREE);
      break;

    case IDC_BOOKMARK_BAR_UNDO: {
      base::RecordAction(UserMetricsAction("BookmarkBar_ContextMenu_Undo"));
      BookmarkUndoServiceFactory::GetForProfile(profile_)->undo_manager()->
          Undo();
      break;
    }

    case IDC_BOOKMARK_BAR_REDO: {
      base::RecordAction(UserMetricsAction("BookmarkBar_ContextMenu_Redo"));
      BookmarkUndoServiceFactory::GetForProfile(profile_)->undo_manager()->
          Redo();
      break;
    }

    case IDC_BOOKMARK_BAR_REMOVE: {
      base::RecordAction(UserMetricsAction("BookmarkBar_ContextMenu_Remove"));

      for (size_t i = 0; i < selection_.size(); ++i) {
        int index = selection_[i]->parent()->GetIndexOf(selection_[i]);
        if (index > -1)
          model_->Remove(selection_[i]);
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
      base::string16 title;
      chrome::GetURLAndTitleToBookmark(
          browser_->tab_strip_model()->GetActiveWebContents(),
          &url, &title);
      BookmarkEditor::Show(parent_window_,
                           profile_,
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
          parent_window_,
          profile_,
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

    case IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS: {
      PrefService* prefs = profile_->GetPrefs();
      prefs->SetBoolean(
          bookmarks::prefs::kShowManagedBookmarksInBookmarkBar,
          !prefs->GetBoolean(
              bookmarks::prefs::kShowManagedBookmarksInBookmarkBar));
      break;
    }

    case IDC_BOOKMARK_MANAGER: {
      if (selection_.size() != 1)
        chrome::ShowBookmarkManager(browser_);
      else if (selection_[0]->is_folder())
        chrome::ShowBookmarkManagerForNode(browser_, selection_[0]->id());
      else if (parent_)
        chrome::ShowBookmarkManagerForNode(browser_, parent_->id());
      else
        chrome::ShowBookmarkManager(browser_);
      break;
    }

    case IDC_CUT:
      bookmarks::CopyToClipboard(model_, selection_, true);
      break;

    case IDC_COPY:
      bookmarks::CopyToClipboard(model_, selection_, false);
      break;

    case IDC_PASTE: {
      size_t index;
      const BookmarkNode* paste_target =
          bookmarks::GetParentForNewNodes(parent_, selection_, &index);
      if (!paste_target)
        return;

      bookmarks::PasteFromClipboard(model_, paste_target, index);
      break;
    }

    default:
      NOTREACHED();
  }

  // It's possible executing the command resulted in deleting |this|.
  if (!ref)
    return;

  if (delegate_)
    delegate_->DidExecuteCommand(id);
}

bool BookmarkContextMenuController::IsItemForCommandIdDynamic(int command_id)
    const {
  return command_id == IDC_BOOKMARK_BAR_UNDO ||
         command_id == IDC_BOOKMARK_BAR_REDO ||
         command_id == IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS;
}

base::string16 BookmarkContextMenuController::GetLabelForCommandId(
    int command_id) const {
  if (command_id == IDC_BOOKMARK_BAR_UNDO) {
    return BookmarkUndoServiceFactory::GetForProfile(profile_)->
        undo_manager()->GetUndoLabel();
  }
  if (command_id == IDC_BOOKMARK_BAR_REDO) {
    return BookmarkUndoServiceFactory::GetForProfile(profile_)->
        undo_manager()->GetRedoLabel();
  }
  if (command_id == IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS) {
    bookmarks::ManagedBookmarkService* managed =
        ManagedBookmarkServiceFactory::GetForProfile(profile_);
    return l10n_util::GetStringFUTF16(IDS_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS,
                                      managed->managed_node()->GetTitle());
  }

  NOTREACHED();
  return base::string16();
}

bool BookmarkContextMenuController::IsCommandIdChecked(int command_id) const {
  PrefService* prefs = profile_->GetPrefs();
  if (command_id == IDC_BOOKMARK_BAR_ALWAYS_SHOW)
    return prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar);
  if (command_id == IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS)
    return prefs->GetBoolean(
        bookmarks::prefs::kShowManagedBookmarksInBookmarkBar);

  DCHECK_EQ(IDC_BOOKMARK_BAR_SHOW_APPS_SHORTCUT, command_id);
  return prefs->GetBoolean(bookmarks::prefs::kShowAppsShortcutInBookmarkBar);
}

bool BookmarkContextMenuController::IsCommandIdEnabled(int command_id) const {
  PrefService* prefs = profile_->GetPrefs();

  bool is_root_node = selection_.size() == 1 &&
                      selection_[0]->parent() == model_->root_node();
  bool can_edit = prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled) &&
                  bookmarks::CanAllBeEditedByUser(model_->client(), selection_);
  IncognitoModePrefs::Availability incognito_avail =
      IncognitoModePrefs::GetAvailability(prefs);

  switch (command_id) {
    case IDC_BOOKMARK_BAR_OPEN_INCOGNITO:
      return !profile_->IsOffTheRecord() &&
             incognito_avail != IncognitoModePrefs::DISABLED;

    case IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO:
      return chrome::HasBookmarkURLsAllowedInIncognitoMode(selection_, profile_)
             &&
             !profile_->IsOffTheRecord() &&
             incognito_avail != IncognitoModePrefs::DISABLED;

    case IDC_BOOKMARK_BAR_OPEN_ALL:
      return chrome::HasBookmarkURLs(selection_);
    case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW:
      return chrome::HasBookmarkURLs(selection_) &&
             incognito_avail != IncognitoModePrefs::FORCED;

    case IDC_BOOKMARK_BAR_RENAME_FOLDER:
    case IDC_BOOKMARK_BAR_EDIT:
      return selection_.size() == 1 && !is_root_node && can_edit;

    case IDC_BOOKMARK_BAR_UNDO:
      return can_edit &&
          BookmarkUndoServiceFactory::GetForProfile(profile_)->
              undo_manager()->undo_count() > 0;

    case IDC_BOOKMARK_BAR_REDO:
      return can_edit &&
          BookmarkUndoServiceFactory::GetForProfile(profile_)->
              undo_manager()->redo_count() > 0;

    case IDC_BOOKMARK_BAR_REMOVE:
      return !selection_.empty() && !is_root_node && can_edit;

    case IDC_BOOKMARK_BAR_NEW_FOLDER:
    case IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK:
      return can_edit && model_->client()->CanBeEditedByUser(parent_) &&
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
               bookmarks::CanPasteFromClipboard(model_, selection_[0])) ||
              bookmarks::CanPasteFromClipboard(model_, parent_));
  }
  return true;
}

bool BookmarkContextMenuController::IsCommandIdVisible(int command_id) const {
  if (command_id == IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS) {
    // The option to hide the Managed Bookmarks folder is only available if
    // there are any managed bookmarks configured at all.
    bookmarks::ManagedBookmarkService* managed =
        ManagedBookmarkServiceFactory::GetForProfile(profile_);
    return !managed->managed_node()->children().empty();
  }

  return true;
}

void BookmarkContextMenuController::BookmarkModelChanged() {
  if (delegate_)
    delegate_->CloseMenu();
}
