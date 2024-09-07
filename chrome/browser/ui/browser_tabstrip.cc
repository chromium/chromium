// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tabstrip.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace chrome {

content::WebContents* AddAndReturnTabAt(
    Browser* browser,
    const GURL& url,
    int idx,
    bool foreground,
    std::optional<tab_groups::TabGroupId> group) {
  // Time new tab page creation time.  We keep track of the timing data in
  // WebContents, but we want to include the time it takes to create the
  // WebContents object too.
  base::TimeTicks new_tab_start_time = base::TimeTicks::Now();
  NavigateParams params(browser, url.is_empty() ? browser->GetNewTabURL() : url,
                        ui::PAGE_TRANSITION_TYPED);
  params.disposition = foreground ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                  : WindowOpenDisposition::NEW_BACKGROUND_TAB;
  params.tabstrip_index = idx;
  params.group = group;
  Navigate(&params);

  if (!params.navigated_or_inserted_contents)
    return nullptr;

  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(params.navigated_or_inserted_contents);
  core_tab_helper->set_new_tab_start_time(new_tab_start_time);

  return params.navigated_or_inserted_contents;
}

void AddTabAt(Browser* browser,
              const GURL& url,
              int idx,
              bool foreground,
              std::optional<tab_groups::TabGroupId> group) {
  /*void*/ AddAndReturnTabAt(browser, url, idx, foreground, std::move(group));
}

content::WebContents* AddSelectedTabWithURL(Browser* browser,
                                            const GURL& url,
                                            ui::PageTransition transition) {
  NavigateParams params(browser, url, transition);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  return params.navigated_or_inserted_contents;
}

content::WebContents* AddWebContents(
    Browser* browser,
    content::WebContents* source_contents,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    NavigateParams::WindowAction window_action) {
  // No code for this yet.
  DCHECK(disposition != WindowOpenDisposition::SAVE_TO_DISK);
  // Can't create a new contents for the current tab - invalid case.
  DCHECK(disposition != WindowOpenDisposition::CURRENT_TAB);

  NavigateParams params(browser, std::move(new_contents));
  params.source_contents = source_contents;
  params.url = target_url;
  params.disposition = disposition;
  params.window_features = window_features;
  params.window_action = window_action;
  // At this point, we're already beyond the popup blocker. Even if the popup
  // was created without a user gesture, we have to set |user_gesture| to true,
  // so it gets correctly focused.
  params.user_gesture = true;

  ConfigureTabGroupForNavigation(&params);

  Navigate(&params);
  return params.navigated_or_inserted_contents;
}

void CloseWebContents(Browser* browser,
                      content::WebContents* contents,
                      bool add_to_history) {
  int index = browser->tab_strip_model()->GetIndexOfWebContents(contents);
  if (index == TabStripModel::kNoTab) {
    DUMP_WILL_BE_NOTREACHED()
        << "CloseWebContents called for tab not in our strip";
    return;
  }

  browser->tab_strip_model()->CloseWebContentsAt(
      index, add_to_history ? TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB
                            : TabCloseTypes::CLOSE_NONE);
}

void ConfigureTabGroupForNavigation(NavigateParams* nav_params) {
  if (!nav_params->source_contents)
    return;

  if (!nav_params->browser || !nav_params->browser->SupportsWindowFeature(
                                  Browser::WindowFeature::FEATURE_TABSTRIP)) {
    return;
  }

  TabStripModel* model = nav_params->browser->tab_strip_model();
  DCHECK(model);

  const int source_index =
      model->GetIndexOfWebContents(nav_params->source_contents);

  // If the source tab is not in the current tab strip (e.g. if the current
  // navigation is in a new window), don't set the group. Groups cannot be
  // shared across multiple windows.
  if (source_index == TabStripModel::kNoTab)
    return;

  // Do not set the group when the navigation is from bookmarks.
  if (ui::PageTransitionCoreTypeIs(nav_params->transition,
                                   ui::PAGE_TRANSITION_AUTO_BOOKMARK)) {
    return;
  }

  if (nav_params->disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
      nav_params->disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    nav_params->group = model->GetTabGroupForTab(source_index);
  }
}

}  // namespace chrome
