// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_page_action_controller.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_triggers.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/gfx/animation/tween.h"

namespace {

bool IsNTPUrl(const GURL& url) {
  return url == chrome::ChromeUINewTabURLAsGURL() ||
         url == chrome::ChromeUINewTabPageURLAsGURL();
}

}  // namespace

DEFINE_USER_DATA(BookmarkPageActionController);

BookmarkPageActionController::BookmarkPageActionController(
    tabs::TabInterface& tab,
    PrefService* pref_service,
    page_actions::PageActionController& page_action_controller)
    : ContentsObservingTabFeature(tab),
      page_action_controller_(page_action_controller),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  edit_bookmarks_enabled_.Init(
      bookmarks::prefs::kEditBookmarksEnabled, pref_service,
      base::BindRepeating(
          &BookmarkPageActionController::UpdatePageActionVisibility,
          base::Unretained(this)));
  UpdatePageActionVisibility();
  ObserveBookmarkTabHelper(tab.GetContents());
}

BookmarkPageActionController::~BookmarkPageActionController() = default;

// static
BookmarkPageActionController* BookmarkPageActionController::From(
    tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

// static
void BookmarkPageActionController::RecordPageActionExecution(
    page_actions::PageActionTrigger trigger) {
  BookmarkEntryPoint entry_point;
  switch (trigger) {
    case page_actions::PageActionTrigger::kMouse:
      entry_point = BookmarkEntryPoint::kStarMouse;
      break;
    case page_actions::PageActionTrigger::kKeyboard:
      entry_point = BookmarkEntryPoint::kStarKey;
      break;
    case page_actions::PageActionTrigger::kGesture:
      entry_point = BookmarkEntryPoint::kStarGesture;
      break;
    default:
      NOTREACHED();
  }
  UMA_HISTOGRAM_ENUMERATION("Bookmarks.EntryPoint", entry_point);
}

void BookmarkPageActionController::URLStarredChanged(
    content::WebContents* web_contents,
    bool starred) {
  SetStarred(starred);
}

void BookmarkPageActionController::ObserveBookmarkTabHelper(
    content::WebContents* contents) {
  tab_helper_observation_.Reset();
  if (auto* bookmark_helper = BookmarkTabHelper::FromWebContents(contents)) {
    tab_helper_observation_.Observe(bookmark_helper);
    SetStarred(bookmark_helper->is_starred());
  } else {
    SetStarred(false);
  }
}

void BookmarkPageActionController::PrimaryPageChanged(content::Page& page) {
  UpdatePageActionVisibility();
}

void BookmarkPageActionController::OnVisibilityChanged(
    content::Visibility visibility) {
  UpdatePageActionVisibility();
}

void BookmarkPageActionController::OnDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  ContentsObservingTabFeature::OnDiscardContents(tab, old_contents,
                                                 new_contents);
  ObserveBookmarkTabHelper(new_contents);
}

void BookmarkPageActionController::UpdatePageActionVisibility() {
  if (ShouldShowPageAction()) {
    page_action_controller_->Show(kActionBookmarkThisTab);
  } else {
    page_action_controller_->Hide(kActionBookmarkThisTab);
  }
}

bool BookmarkPageActionController::ShouldShowPageAction() const {
  const auto& url = tab().GetContents()->GetLastCommittedURL();
  return browser_defaults::bookmarks_enabled &&
         edit_bookmarks_enabled_.GetValue() && url.is_valid() && !IsNTPUrl(url);
}

void BookmarkPageActionController::SetStarred(bool starred) {
  const std::u16string name = l10n_util::GetStringUTF16(
      starred ? IDS_TOOLTIP_STARRED : IDS_TOOLTIP_STAR);
  page_action_controller_->OverrideAccessibleName(kActionBookmarkThisTab, name);
  page_action_controller_->OverrideTooltip(kActionBookmarkThisTab, name);

  page_action_controller_->OverrideImage(
      kActionBookmarkThisTab,
      ui::ImageModel::FromVectorIcon(
          starred ? features::IsRoundedIconsEnabled()
                        ? omnibox::kStarFilledIcon
                        : omnibox::kStarActiveChromeRefreshOldIcon
          : features::IsRoundedIconsEnabled()
              ? omnibox::kStarIcon
              : omnibox::kStarChromeRefreshOldIcon),
      starred ? page_actions::PageActionColorSource::kCascadingAccent
              : page_actions::PageActionColorSource::kForeground,
      features::IsToolbarGlowUpEnabled()
          ? std::make_optional<page_actions::PageActionAnimationParams>(
                {.resource_id = IDR_STAR_LOTTIE,
                 .start_offset = starred ? 0.0f : 0.5f,
                 .end_offset = starred ? 0.25f : 0.75f,
                 .tween = gfx::Tween::FAST_OUT_SLOW_IN_3,
                 .duration = starred ? base::Milliseconds(400)
                                     : base::Milliseconds(250)})
          : std::nullopt);
}
