// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_util.h"

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_scroll_observer.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "ui/base/window_open_disposition.h"
#include "url/origin.h"

namespace send_tab_to_self {

namespace {

// Opens the given `entry` in a new tab with the given `disposition` for the
// given `profile`.
// Returns a weak pointer to the opened WebContents.
base::WeakPtr<content::WebContents> OpenEntryInNewTabWithDisposition(
    Profile* profile,
    const SendTabToSelfEntry& entry,
    WindowOpenDisposition disposition) {
  RecordHasScrollPositionOnOpened(
      !entry.GetPageContext().scroll_position.IsEmpty());

  NavigateParams params(profile, entry.GetURL(), ui::PAGE_TRANSITION_LINK);
  params.disposition = disposition;
  params.window_action = NavigateParams::WindowAction::kShowWindow;

  std::optional<std::string> scroll_to_text_fragment =
      GetScrollPositionAsTextFragment(&entry);
  if (scroll_to_text_fragment) {
    params.internal_scroll_to_text_fragment = *scroll_to_text_fragment;
  }

  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);

  if (params.navigated_or_inserted_contents) {
    SendTabToSelfScrollObserver::CreateForWebContents(
        params.navigated_or_inserted_contents,
        /*restoration_attempted=*/scroll_to_text_fragment.has_value());
  }

  if (handle &&
      base::FeatureList::IsEnabled(kSendTabToSelfPropagateFormFields)) {
    FillWebContents(params.navigated_or_inserted_contents,
                    url::Origin::Create(entry.GetURL()),
                    entry.GetPageContext());
  }

  SendTabToSelfSyncServiceFactory::GetForProfile(profile)
      ->GetSendTabToSelfModel()
      ->MarkEntryOpened(entry.GetGUID());

  return params.navigated_or_inserted_contents
             ? params.navigated_or_inserted_contents->GetWeakPtr()
             : nullptr;
}

}  // namespace

base::WeakPtr<content::WebContents> OpenEntryInNewForegroundTab(
    Profile* profile,
    const SendTabToSelfEntry& entry) {
  return OpenEntryInNewTabWithDisposition(
      profile, entry, WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

base::WeakPtr<content::WebContents> OpenEntryInNewBackgroundTab(
    Profile* profile,
    const SendTabToSelfEntry& entry) {
  return OpenEntryInNewTabWithDisposition(
      profile, entry, WindowOpenDisposition::NEW_BACKGROUND_TAB);
}

}  // namespace send_tab_to_self
