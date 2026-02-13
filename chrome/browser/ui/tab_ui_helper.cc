// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_ui_helper.h"

#include <optional>

#include "base/byte_size.h"
#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/process/kill.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/performance_controls/memory_saver_utils.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/resources/grit/ui_resources.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"  // nogncheck
#endif

namespace {

// Whether the throbber should be shown for a restored tab after it becomes
// visible, instead of when it's active in the tab strip (this signal is known
// to be broken crbug.com/413080225#comment8).
BASE_FEATURE(kSessionRestoreShowThrobberOnVisible,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

DEFINE_USER_DATA(TabUIHelper);

TabUIHelper::TabUIHelper(tabs::TabInterface& tab_interface)
    : ContentsObservingTabFeature(tab_interface),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
}

TabUIHelper::~TabUIHelper() = default;

// static
const TabUIHelper* TabUIHelper::From(const tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

// static
TabUIHelper* TabUIHelper::From(tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

base::CallbackListSubscription TabUIHelper::AddTabUIChangeCallback(
    base::RepeatingClosure callback) {
  return tab_ui_change_callbacks_.Add(std::move(callback));
}

std::u16string TabUIHelper::GetTitle() const {
  const tab_groups::SavedTabGroupWebContentsListener* wc_listener =
      tab().GetTabFeatures()->saved_tab_group_web_contents_listener();
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
  const tab_groups::SavedTabGroupWebContentsListener* wc_listener =
      tab().GetTabFeatures()->saved_tab_group_web_contents_listener();
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

bool TabUIHelper::IsCrashed() {
  const base::TerminationStatus crashed_status =
      web_contents()->GetCrashedStatus();
  return (crashed_status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ||
#if BUILDFLAG(IS_CHROMEOS)
          crashed_status ==
              base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM ||
#endif
          crashed_status == base::TERMINATION_STATUS_PROCESS_CRASHED ||
          crashed_status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION ||
          crashed_status == base::TERMINATION_STATUS_LAUNCH_FAILED);
}

base::CallbackListSubscription TabUIHelper::AddTitleUpdatedCallback(
    TitleUpdatedCallbackList::CallbackType callback) {
  return title_change_callbacks_.Add(std::move(callback));
}

void TabUIHelper::TitleWasSet(content::NavigationEntry* entry) {
  title_change_callbacks_.Notify(GetTitle());
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

void TabUIHelper::WasDiscarded() {
  // Notify observers that the tab should update its UI to show discard status.
  if (ShouldShowDiscardStatus()) {
    tab_ui_change_callbacks_.Notify();
  }
}

#if !BUILDFLAG(IS_ANDROID)
void TabUIHelper::PrimaryPageChanged(content::Page& page) {
  if (tab().IsSplit()) {
    split_tabs::LogSplitViewUpdatedUKM(
        tab().GetBrowserWindowInterface()->GetTabStripModel(),
        tab().GetSplit().value());
  }
}
#endif

bool TabUIHelper::ShouldShowDiscardStatus() {
  content::WebContents* const web_contents = tab().GetContents();
  std::optional<mojom::LifecycleUnitDiscardReason> discard_reason =
      memory_saver::GetDiscardReason(web_contents);

  // Only show discard status for tabs that were proactively discarded or
  // suggested by the PerformanceDetectionManager to prevent confusion to users
  // on why a tab was discarded. Also, the favicon discard animation may use
  // resources so the animation should be limited to prevent performance issues.
  return memory_saver::IsURLSupported(web_contents->GetURL()) &&
         web_contents->WasDiscarded() && discard_reason.has_value() &&
         (discard_reason.value() ==
              mojom::LifecycleUnitDiscardReason::PROACTIVE ||
          discard_reason.value() ==
              mojom::LifecycleUnitDiscardReason::SUGGESTED);
}

std::optional<base::ByteSize> TabUIHelper::GetDiscardedMemorySavings() {
  content::WebContents* const web_contents = tab().GetContents();
  return web_contents->WasDiscarded()
             ? std::make_optional(
                   memory_saver::GetDiscardedMemorySavings(web_contents))
             : std::nullopt;
}
