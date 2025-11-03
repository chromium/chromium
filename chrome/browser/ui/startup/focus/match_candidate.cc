// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/focus/match_candidate.h"

#include "base/strings/string_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/startup/focus/selector.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/browser/web_contents.h"

namespace focus {

namespace {

// Canonicalizes URLs for consistent matching by normalizing format.
GURL CanonicalizeUrl(const GURL& url) {
  if (!url.is_valid()) {
    return url;
  }

  std::string spec = url.spec();

  // For root paths (just domain), ensure consistent trailing slash.
  if (url.path().length() <= 1) {
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

// Attempts to match an app window by its manifest ID against the selector.
// Returns the manifest ID spec if matched, std::nullopt otherwise.
std::optional<std::string> MatchAppByManifestId(
    BrowserWindowInterface& browser_window,
    const Selector& selector,
    const GURL& canonicalized_selector_url,
    web_app::WebAppRegistrar* registrar) {
  auto* app_controller = web_app::AppBrowserController::From(&browser_window);
  // Ensure this is specifically a WebAppBrowserController, not just any
  // AppBrowserController.
  web_app::WebAppBrowserController* web_app_controller =
      app_controller ? app_controller->AsWebAppBrowserController() : nullptr;
  if (!web_app_controller) {
    return std::nullopt;
  }

  const webapps::AppId& browser_app_id = web_app_controller->app_id();
  const web_app::WebApp* web_app = registrar->GetAppById(browser_app_id);
  if (!web_app) {
    return std::nullopt;
  }

  const webapps::ManifestId& manifest_id = web_app->manifest_id();
  GURL canonicalized_manifest_id = CanonicalizeUrl(manifest_id);

  bool is_match = false;
  if (selector.type == SelectorType::kUrlExact) {
    is_match = (canonicalized_manifest_id == canonicalized_selector_url);
  } else if (selector.type == SelectorType::kUrlPrefix) {
    std::string manifest_id_string = canonicalized_manifest_id.spec();
    std::string selector_url_string = canonicalized_selector_url.spec();
    is_match = base::StartsWith(manifest_id_string, selector_url_string,
                                base::CompareCase::SENSITIVE);
  }

  if (is_match) {
    return manifest_id.spec();
  }

  return std::nullopt;
}

}  // namespace

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

std::optional<MatchCandidate> MatchTab(const Selector& selector,
                                       BrowserWindowInterface& browser_window,
                                       int tab_index,
                                       content::WebContents& web_contents,
                                       web_app::WebAppRegistrar* registrar) {
  const GURL& tab_url = web_contents.GetLastCommittedURL();
  GURL canonicalized_tab_url = CanonicalizeUrl(tab_url);
  GURL canonicalized_selector_url = CanonicalizeUrl(selector.url);

  bool is_match = false;
  std::optional<std::string> matched_app_id;

  // Check if this is an app window and try matching against manifest ID ONLY.
  // Apps should only match by their manifest ID to allow disambiguation
  // between apps and regular tabs with similar URLs.
  if (browser_window.GetType() == BrowserWindowInterface::TYPE_APP &&
      registrar) {
    matched_app_id = MatchAppByManifestId(
        browser_window, selector, canonicalized_selector_url, registrar);
    // For app windows, return early - don't match by tab URL.
    if (!matched_app_id.has_value()) {
      return std::nullopt;
    }
    is_match = true;
  } else {
    // For regular browser windows, match by tab URL
    if (selector.type == SelectorType::kUrlExact) {
      is_match = (canonicalized_tab_url == canonicalized_selector_url);
    } else if (selector.type == SelectorType::kUrlPrefix) {
      std::string tab_url_string = canonicalized_tab_url.spec();
      std::string selector_url_string = canonicalized_selector_url.spec();
      is_match = base::StartsWith(tab_url_string, selector_url_string,
                                  base::CompareCase::SENSITIVE);
    }
  }

  if (!is_match) {
    return std::nullopt;
  }

  base::Time last_active_time = web_contents.GetLastActiveTime();
  return MatchCandidate(browser_window, tab_index, web_contents,
                        last_active_time, tab_url.spec(), matched_app_id);
}

}  // namespace focus
