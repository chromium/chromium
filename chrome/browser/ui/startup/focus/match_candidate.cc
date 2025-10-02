// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/focus/match_candidate.h"

#include "base/strings/string_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/startup/focus/selector.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "content/public/browser/web_contents.h"

namespace focus {

MatchCandidate::MatchCandidate(BrowserWindowInterface& browser_window,
                               int tab_index,
                               content::WebContents& web_contents,
                               base::Time last_active_time,
                               const std::string& matched_url,
                               const std::optional<std::string>& app_id)
    : browser_window(browser_window),
      tab_index(tab_index),
      web_contents(web_contents),
      last_active_time(last_active_time),
      matched_url(matched_url),
      app_id(app_id) {}

MatchCandidate::MatchCandidate(MatchCandidate&& other) noexcept = default;

MatchCandidate& MatchCandidate::operator=(MatchCandidate&& other) noexcept =
    default;

MatchCandidate::~MatchCandidate() = default;

bool MatchCandidate::operator<(const MatchCandidate& other) const {
  // For MRU sorting: more recent items should come first in the sorted array.
  // std::sort uses operator< to determine order: if a < b, then a comes before.
  // So we want more recent (larger time) items to be "less than" older items.

  // First priority: Compare tab last active times.
  if (last_active_time != other.last_active_time) {
    return last_active_time >
           other.last_active_time;  // More recent = "less than" = comes first
  }

  // Second priority: App windows come before regular tabs (if times are equal).
  if (app_id.has_value() && !other.app_id.has_value()) {
    return true;  // App window comes first.
  }
  if (!app_id.has_value() && other.app_id.has_value()) {
    return false;  // Regular tab comes after app window.
  }

  // Third priority: Among regular tabs with same times, use tab index as
  // tiebreaker.
  if (!app_id.has_value() && !other.app_id.has_value()) {
    return tab_index < other.tab_index;  // Lower tab index comes first.
  }

  return false;  // Equal items
}

// Canonicalizes URLs for consistent matching by normalizing format.
GURL CanonicalizeUrl(const GURL& url) {
  if (!url.is_valid()) {
    return url;
  }

  std::string spec = url.spec();

  // For root paths (just domain), ensure consistent trailing slash.
  if (url.path_piece().length() <= 1) {
    // Root path: ensure it ends with /
    if (!base::EndsWith(spec, "/", base::CompareCase::SENSITIVE)) {
      spec += "/";
    }
  } else {
    // Non-root path: remove trailing slash.
    if (base::EndsWith(spec, "/", base::CompareCase::SENSITIVE)) {
      spec = spec.substr(0, spec.length() - 1);
    }
  }

  return GURL(spec);
}

std::optional<MatchCandidate> MatchTab(const Selector& selector,
                                       BrowserWindowInterface& browser_window,
                                       int tab_index,
                                       content::WebContents& web_contents) {
  if (selector.type == SelectorType::kApp) {
    return std::nullopt;
  }

  const GURL& tab_url = web_contents.GetLastCommittedURL();
  GURL canonicalized_tab_url = CanonicalizeUrl(tab_url);
  GURL canonicalized_selector_url = CanonicalizeUrl(selector.url);

  bool is_match = false;
  if (selector.type == SelectorType::kUrlExact) {
    is_match = (canonicalized_tab_url == canonicalized_selector_url);
  } else if (selector.type == SelectorType::kUrlPrefix) {
    std::string tab_url_string = canonicalized_tab_url.spec();
    std::string selector_url_string = canonicalized_selector_url.spec();
    is_match = base::StartsWith(tab_url_string, selector_url_string,
                                base::CompareCase::SENSITIVE);
  }

  if (!is_match) {
    return std::nullopt;
  }

  base::Time last_active_time = web_contents.GetLastActiveTime();
  return MatchCandidate(browser_window, tab_index, web_contents,
                        last_active_time, tab_url.spec(), std::nullopt);
}

std::optional<MatchCandidate> MatchApp(const Selector& selector,
                                       BrowserWindowInterface& browser_window) {
  if (selector.type != SelectorType::kApp ||
      browser_window.GetType() != BrowserWindowInterface::TYPE_APP) {
    return std::nullopt;
  }

  web_app::AppBrowserController* app_controller =
      browser_window.GetAppBrowserController();
  CHECK(app_controller);  // TYPE_APP browsers must have an AppBrowserController

  const webapps::AppId& browser_app_id = app_controller->app_id();
  if (browser_app_id != selector.app_id) {
    return std::nullopt;
  }

  TabStripModel* tab_strip = browser_window.GetTabStripModel();
  if (!tab_strip || tab_strip->count() == 0) {
    return std::nullopt;
  }

  content::WebContents* web_contents = tab_strip->GetActiveWebContents();
  if (!web_contents) {
    return std::nullopt;
  }

  base::Time last_active_time = web_contents->GetLastActiveTime();
  const GURL& tab_url = web_contents->GetLastCommittedURL();
  return MatchCandidate(browser_window, tab_strip->active_index(),
                        *web_contents, last_active_time, tab_url.spec(),
                        std::make_optional(selector.app_id));
}

}  // namespace focus
