// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/star_view.h"

#include <string>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_class_properties.h"

StarView::StarView(CommandUpdater* command_updater,
                   Browser* browser,
                   IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
                   PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_BOOKMARK_THIS_TAB,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "BookmarksStar",
                         false) {
  DCHECK(browser);

  edit_bookmarks_enabled_.Init(
      bookmarks::prefs::kEditBookmarksEnabled, browser->profile()->GetPrefs(),
      base::BindRepeating(&StarView::EditBookmarksPrefUpdated,
                          base::Unretained(this)));
  SetID(VIEW_ID_STAR_BUTTON);
  SetProperty(views::kElementIdentifierKey, kBookmarkStarViewElementId);
  SetActive(false);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(IDS_TOOLTIP_STAR));
}

StarView::~StarView() = default;

void StarView::AfterPropertyChange(const void* key, int64_t old_value) {
  View::AfterPropertyChange(key, old_value);
  if (key == user_education::kHasInProductHelpPromoKey) {
    views::InkDropState next_state;
    if (GetProperty(user_education::kHasInProductHelpPromoKey) ||
        GetVisible()) {
      next_state = views::InkDropState::ACTIVATED;
    } else {
      next_state = views::InkDropState::DEACTIVATED;
    }
    views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(next_state);
  }
}

void StarView::UpdateImpl() {
  SetVisible(browser_defaults::bookmarks_enabled &&
             edit_bookmarks_enabled_.GetValue());
}

void StarView::OnExecuting(PageActionIconView::ExecuteSource execute_source) {
  BookmarkEntryPoint entry_point = BookmarkEntryPoint::kStarMouse;
  switch (execute_source) {
    case EXECUTE_SOURCE_MOUSE:
      entry_point = BookmarkEntryPoint::kStarMouse;
      break;
    case EXECUTE_SOURCE_KEYBOARD:
      entry_point = BookmarkEntryPoint::kStarKey;
      break;
    case EXECUTE_SOURCE_GESTURE:
      entry_point = BookmarkEntryPoint::kStarGesture;
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("Bookmarks.EntryPoint", entry_point);
}

views::BubbleDialogDelegate* StarView::GetBubble() const {
  return BookmarkBubbleView::bookmark_bubble();
}

const gfx::VectorIcon& StarView::GetVectorIcon() const {
    return GetActive() ? omnibox::kStarActiveChromeRefreshIcon
                       : omnibox::kStarChromeRefreshIcon;
}

std::u16string StarView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(GetActive() ? IDS_TOOLTIP_STARRED
                                               : IDS_TOOLTIP_STAR);
}

void StarView::EditBookmarksPrefUpdated() {
  Update();
}

BEGIN_METADATA(StarView)
END_METADATA
