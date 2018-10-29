// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/star_view.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/feature_promos/bookmark_promo_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/widget/widget_observer.h"

StarView::StarView(CommandUpdater* command_updater,
                   Browser* browser,
                   PageActionIconView::Delegate* delegate)
    : PageActionIconView(command_updater, IDC_BOOKMARK_PAGE, delegate),
      browser_(browser),
      bookmark_promo_observer_(this) {
  set_id(VIEW_ID_STAR_BUTTON);
  SetToggled(false);
}

StarView::~StarView() {}

void StarView::SetToggled(bool on) {
  PageActionIconView::SetActiveInternal(on);
}

void StarView::ShowPromo() {
  BookmarkPromoBubbleView* bookmark_promo_bubble =
      BookmarkPromoBubbleView::CreateOwned(this);
  if (!bookmark_promo_observer_.IsObserving(
          bookmark_promo_bubble->GetWidget())) {
    bookmark_promo_observer_.Add(bookmark_promo_bubble->GetWidget());
    SetActiveInternal(false);
    UpdateIconImage();
  }
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
  UMA_HISTOGRAM_ENUMERATION("Bookmarks.EntryPoint",
                            entry_point,
                            BOOKMARK_ENTRY_POINT_LIMIT);
}

void StarView::ExecuteCommand(ExecuteSource source) {
  if (browser_) {
    OnExecuting(source);
    chrome::BookmarkCurrentPageIgnoringExtensionOverrides(browser_);
  } else {
    PageActionIconView::ExecuteCommand(source);
  }
}

views::BubbleDialogDelegateView* StarView::GetBubble() const {
  return BookmarkBubbleView::bookmark_bubble();
}

const gfx::VectorIcon& StarView::GetVectorIcon() const {
  return active() ? omnibox::kStarActiveIcon : omnibox::kStarIcon;
}

base::string16 StarView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(active() ? IDS_TOOLTIP_STARRED
                                            : IDS_TOOLTIP_STAR);
}

SkColor StarView::GetInkDropBaseColor() const {
  return bookmark_promo_observer_.IsObservingSources()
             ? GetNativeTheme()->GetSystemColor(
                   ui::NativeTheme::kColorId_ProminentButtonColor)
             : PageActionIconView::GetInkDropBaseColor();
}

void StarView::OnWidgetDestroying(views::Widget* widget) {
  if (bookmark_promo_observer_.IsObserving(widget)) {
    bookmark_promo_observer_.Remove(widget);
    SetActiveInternal(false);
    UpdateIconImage();
  }
}
