// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_FOCUS_MATCH_CANDIDATE_H_
#define CHROME_BROWSER_UI_STARTUP_FOCUS_MATCH_CANDIDATE_H_

#include <optional>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/time/time.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}

namespace web_app {
class WebAppRegistrar;
}

namespace focus {

struct Selector;

// Represents a tab or app window that matches focus criteria and may be
// focused. Contains all necessary information to focus the candidate and sort
// by priority. Uses references to guarantee non-null browser_window and
// web_contents.
struct MatchCandidate {
  MatchCandidate(BrowserWindowInterface& browser_window,
                 int tab_index,
                 content::WebContents& web_contents,
                 base::Time last_active_time,
                 const std::string& matched_url,
                 const std::optional<std::string>& app_id);
  MatchCandidate(const MatchCandidate& other) = delete;
  MatchCandidate& operator=(const MatchCandidate& other) = delete;
  MatchCandidate(MatchCandidate&& other) noexcept;
  MatchCandidate& operator=(MatchCandidate&& other) noexcept;
  ~MatchCandidate();

  bool operator<(const MatchCandidate& other) const;

  raw_ref<BrowserWindowInterface> browser_window;
  int tab_index;
  raw_ref<content::WebContents> web_contents;
  base::Time last_active_time;
  std::string matched_url;
  std::optional<std::string> app_id;
};

// Creates a MatchCandidate if the given WebContents matches the selector.
// For app windows, matches against the app's manifest ID.
// For regular tabs, matches against the tab's URL.
// Returns std::nullopt if there's no match.
std::optional<MatchCandidate> MatchTab(
    const Selector& selector,
    BrowserWindowInterface& browser_window,
    int tab_index,
    content::WebContents& web_contents,
    web_app::WebAppRegistrar* registrar = nullptr);

}  // namespace focus

#endif  // CHROME_BROWSER_UI_STARTUP_FOCUS_MATCH_CANDIDATE_H_
