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
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"
#include "chrome/common/extensions/manifest_handlers/ui_overrides_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// For bookmark in-product help.
int GetBookmarkPromoStringSpecifier() {
  static constexpr int kTextIds[] = {IDS_BOOKMARK_PROMO_0, IDS_BOOKMARK_PROMO_1,
                                     IDS_BOOKMARK_PROMO_2};
  const std::string& str = variations::GetVariationParamValue(
      "BookmarkInProductHelp", "x_promo_string");
  size_t text_specifier;
  if (!base::StringToSizeT(str, &text_specifier) ||
      text_specifier >= base::size(kTextIds)) {
    text_specifier = 0;
  }

  return kTextIds[text_specifier];
}

}  // namespace

StarView::StarView(CommandUpdater* command_updater,
                   Browser* browser,
                   PageActionIconView::Delegate* delegate)
    : PageActionIconView(command_updater, IDC_BOOKMARK_THIS_TAB, delegate),
      browser_(browser) {
  DCHECK(browser_);
  extension_observer_.Add(
      extensions::ExtensionRegistry::Get(browser_->profile()));
  edit_bookmarks_enabled_.Init(
      bookmarks::prefs::kEditBookmarksEnabled, browser_->profile()->GetPrefs(),
      base::BindRepeating(&StarView::EditBookmarksPrefUpdated,
                          base::Unretained(this)));
  SetID(VIEW_ID_STAR_BUTTON);
  SetActive(false);
}

StarView::~StarView() {}

void StarView::ShowPromo() {
  FeaturePromoBubbleView* bookmark_promo_bubble =
      FeaturePromoBubbleView::CreateOwned(
          this, views::BubbleBorder::TOP_RIGHT,
          FeaturePromoBubbleView::ActivationAction::ACTIVATE,
          GetBookmarkPromoStringSpecifier());
  if (!bookmark_promo_observer_.IsObserving(
          bookmark_promo_bubble->GetWidget())) {
    bookmark_promo_observer_.Add(bookmark_promo_bubble->GetWidget());
    SetActive(false);
    UpdateIconImage();
  }
}

bool StarView::Update() {
  bool was_visible = GetVisible();
  SetVisible(browser_defaults::bookmarks_enabled &&
             !delegate()->IsLocationBarUserInputInProgress() &&
             edit_bookmarks_enabled_.GetValue() &&
             !IsBookmarkStarHiddenByExtension());
  return was_visible != GetVisible();
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
  OnExecuting(source);
  chrome::BookmarkCurrentTabIgnoringExtensionOverrides(browser_);
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
    SetActive(false);
    UpdateIconImage();
  }
}

void StarView::OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const extensions::Extension* extension) {
  if (extensions::UIOverrides::RemovesBookmarkButton(extension))
    Update();
}

void StarView::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                   const extensions::Extension* extension,
                                   extensions::UnloadedExtensionReason reason) {
  if (extensions::UIOverrides::RemovesBookmarkButton(extension))
    Update();
}

void StarView::EditBookmarksPrefUpdated() {
  Update();
}

bool StarView::IsBookmarkStarHiddenByExtension() const {
  const extensions::ExtensionSet& extension_set =
      extensions::ExtensionRegistry::Get(browser_->profile())
          ->enabled_extensions();
  for (const scoped_refptr<const extensions::Extension> extension :
       extension_set) {
    if (!extensions::UIOverrides::RemovesBookmarkButton(extension.get()))
      continue;
    if (extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kBookmarkManagerPrivate)) {
      return true;
    }
    if (extensions::FeatureSwitch::enable_override_bookmarks_ui()
            ->IsEnabled()) {
      return true;
    }
  }
  return false;
}
