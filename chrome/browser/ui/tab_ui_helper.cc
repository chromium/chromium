// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_ui_helper.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/resources/grit/ui_resources.h"

namespace {

// Whether the throbber should be shown for a restored tab after it becomes
// visible, instead of when it's active in the tab strip (this signal is known
// to be broken crbug.com/413080225#comment8).
BASE_FEATURE(kSessionRestoreShowThrobberOnVisible,
             "SessionRestoreShowThrobberOnVisible",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

TabUIHelper::TabUIHelper(tabs::TabInterface& tab_interface)
    : ContentsObservingTabFeature(tab_interface) {}

TabUIHelper::~TabUIHelper() = default;

std::u16string TabUIHelper::GetTitle() const {
  tabs::TabInterface* const tab_interface =
      tabs::TabInterface::GetFromContents(web_contents());
  const tab_groups::SavedTabGroupWebContentsListener* wc_listener =
      tab_interface->GetTabFeatures()->saved_tab_group_web_contents_listener();
  if (wc_listener) {
    if (const std::optional<tab_groups::DeferredTabState>& deferred_tab_state =
            wc_listener->deferred_tab_state()) {
      return deferred_tab_state.value().title();
    }
  }

  const std::u16string& contents_title = web_contents()->GetTitle();
  if (!contents_title.empty()) {
    return contents_title;
  }

#if BUILDFLAG(IS_MAC)
  return l10n_util::GetStringUTF16(IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED);
#else
  return std::u16string();
#endif
}

ui::ImageModel TabUIHelper::GetFavicon() const {
  tabs::TabInterface* const tab_interface =
      tabs::TabInterface::GetFromContents(web_contents());
  const tab_groups::SavedTabGroupWebContentsListener* wc_listener =
      tab_interface->GetTabFeatures()->saved_tab_group_web_contents_listener();
  if (wc_listener) {
    if (const std::optional<tab_groups::DeferredTabState>& deferred_tab_state =
            wc_listener->deferred_tab_state()) {
      return deferred_tab_state.value().favicon();
    }
  }

  return ui::ImageModel::FromImage(
      favicon::TabFaviconFromWebContents(web_contents()));
}

bool TabUIHelper::ShouldHideThrobber() const {
  // We want to hide a background tab's throbber during page load if it is
  // created by session restore. A restored tab's favicon is already fetched
  // by |SessionRestoreDelegate|.
  if (created_by_session_restore_ && !was_active_at_least_once_) {
    return true;
  }

  return false;
}

void TabUIHelper::SetWasActiveAtLeastOnce() {
  if (!base::FeatureList::IsEnabled(kSessionRestoreShowThrobberOnVisible)) {
    was_active_at_least_once_ = true;
  }
}

void TabUIHelper::DidStopLoading() {
  // Reset the properties after the initial navigation finishes loading, so that
  // latter navigations are not affected. Note that the prerendered page won't
  // reset the properties because DidStopLoading is not called for prerendering.
  created_by_session_restore_ = false;
}

void TabUIHelper::OnVisibilityChanged(content::Visibility visiblity) {
  if (base::FeatureList::IsEnabled(kSessionRestoreShowThrobberOnVisible) &&
      visiblity == content::Visibility::VISIBLE) {
    was_active_at_least_once_ = true;
  }
}
