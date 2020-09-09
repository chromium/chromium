// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/star_view.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/star_menu_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/menu/menu_runner.h"

StarView::StarView(CommandUpdater* command_updater,
                   Browser* browser,
                   IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
                   PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_BOOKMARK_THIS_TAB,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate),
      browser_(browser) {
  DCHECK(browser_);

  edit_bookmarks_enabled_.Init(
      bookmarks::prefs::kEditBookmarksEnabled, browser_->profile()->GetPrefs(),
      base::BindRepeating(&StarView::EditBookmarksPrefUpdated,
                          base::Unretained(this)));
  SetID(VIEW_ID_STAR_BUTTON);
  SetActive(false);
}

StarView::~StarView() {}

void StarView::UpdateImpl() {
  SetVisible(browser_defaults::bookmarks_enabled &&
             edit_bookmarks_enabled_.GetValue());
}

void StarView::OnExecuting(PageActionIconView::ExecuteSource execute_source) {
  BookmarkEntryPoint entry_point = BOOKMARK_ENTRY_POINT_STAR_MOUSE;
  switch (execute_source) {
    case EXECUTE_SOURCE_MOUSE:
      entry_point = BOOKMARK_ENTRY_POINT_STAR_MOUSE;
      break;
    case EXECUTE_SOURCE_KEYBOARD:
      entry_point = BOOKMARK_ENTRY_POINT_STAR_KEY;
      break;
    case EXECUTE_SOURCE_GESTURE:
      entry_point = BOOKMARK_ENTRY_POINT_STAR_GESTURE;
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("Bookmarks.EntryPoint", entry_point,
                            BOOKMARK_ENTRY_POINT_LIMIT);
}

void StarView::ExecuteCommand(ExecuteSource source) {
  if (base::FeatureList::IsEnabled(features::kReadLater)) {
    menu_model_ = std::make_unique<StarMenuModel>(
        this, active(), chrome::CanMoveActiveTabToReadLater(browser_),
        chrome::IsCurrentTabUnreadInReadLater(browser_));
    menu_runner_ = std::make_unique<views::MenuRunner>(
        menu_model_.get(),
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::FIXED_ANCHOR);
    menu_runner_->RunMenuAt(GetWidget(), nullptr, GetAnchorBoundsInScreen(),
                            views::MenuAnchorPosition::kTopRight,
                            ui::MENU_SOURCE_NONE);
  } else {
    OnExecuting(source);
    chrome::BookmarkCurrentTab(browser_);
  }
}

views::BubbleDialogDelegate* StarView::GetBubble() const {
  return BookmarkBubbleView::bookmark_bubble();
}

const gfx::VectorIcon& StarView::GetVectorIcon() const {
  return active() ? omnibox::kStarActiveIcon : omnibox::kStarIcon;
}

base::string16 StarView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(active() ? IDS_TOOLTIP_STARRED
                                            : IDS_TOOLTIP_STAR);
}

const char* StarView::GetClassName() const {
  return "StarView";
}

void StarView::EditBookmarksPrefUpdated() {
  Update();
}

void StarView::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case StarMenuModel::CommandBookmark:
      chrome::BookmarkCurrentTab(browser_);
      break;
    case StarMenuModel::CommandMoveToReadLater:
      chrome::MoveCurrentTabToReadLater(browser_);
      break;
    case StarMenuModel::CommandMarkAsRead:
      chrome::MarkCurrentTabAsReadInReadLater(browser_);
      break;
    default:
      NOTREACHED();
  }
}

void StarView::MenuClosed(ui::SimpleMenuModel* source) {
  if (!GetBubble() || !GetBubble()->GetWidget() ||
      !GetBubble()->GetWidget()->IsVisible()) {
    SetHighlighted(false);
  }
  menu_runner_.reset();
}
