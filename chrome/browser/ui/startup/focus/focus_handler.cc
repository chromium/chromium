// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/focus/focus_handler.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/startup/focus/focus_result_file_writer.h"
#include "chrome/browser/ui/startup/focus/match_candidate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"
#include "url/gurl.h"

namespace focus {

FocusResult::FocusResult(FocusStatus status)
    : status(status), error_type(Error::kNone) {}

FocusResult::FocusResult(FocusStatus status,
                         const std::string& matched_selector,
                         const std::string& matched_url)
    : status(status),
      matched_selector(matched_selector),
      matched_url(matched_url),
      error_type(Error::kNone) {}

FocusResult::FocusResult(FocusStatus status, Error error_type)
    : status(status), error_type(error_type) {}

FocusResult::FocusResult(FocusStatus status, std::string opened_url)
    : status(status),
      opened_url(std::move(opened_url)),
      error_type(Error::kNone) {}

FocusResult::FocusResult(const FocusResult& other) = default;

FocusResult::FocusResult(FocusResult&& other) noexcept = default;

FocusResult& FocusResult::operator=(const FocusResult& other) = default;

FocusResult& FocusResult::operator=(FocusResult&& other) noexcept = default;

FocusResult::~FocusResult() = default;

bool FocusResult::IsSuccess() const {
  return status == FocusStatus::kFocused;
}

bool FocusResult::HasMatch() const {
  return matched_selector.has_value();
}

namespace {

std::vector<MatchCandidate> CollectMatchingElements(const Selector& selector,
                                                    Profile& profile) {
  std::vector<MatchCandidate> candidates;

  // Get the WebAppProvider to access the registrar for app matching.
  // This is safe access for our use-case, as we are only using this when
  // encountering a browser with an AppBrowserController, which means that an
  // app is installed here, at least for now.
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(&profile);
  web_app::WebAppRegistrar* registrar =
      provider ? &provider->registrar_unsafe() : nullptr;

  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser_window) {
        if (browser_window->GetProfile() != &profile) {
          return true;
        }

        TabStripModel* tab_strip = browser_window->GetTabStripModel();
        if (!tab_strip) {
          return true;
        }

        for (int i = 0; i < tab_strip->count(); i++) {
          content::WebContents* web_contents = tab_strip->GetWebContentsAt(i);
          if (!web_contents) {
            continue;
          }

          std::optional<MatchCandidate> match =
              MatchTab(selector, *browser_window, i, *web_contents, registrar);
          if (match.has_value()) {
            candidates.push_back(std::move(match.value()));
          }
        }
        return true;
      });

  return candidates;
}

void SortCandidatesByMRU(std::vector<MatchCandidate>& candidates) {
  std::sort(candidates.begin(), candidates.end());
}

bool FocusCandidate(const MatchCandidate& candidate) {
  if (candidate.app_id.has_value()) {
    // App window.
    candidate.browser_window->GetWindow()->Show();
    candidate.browser_window->GetWindow()->Activate();
    return true;
  }

  // Regular tab.
  TabStripModel* tab_strip = candidate.browser_window->GetTabStripModel();
  CHECK(tab_strip);

  int actual_index =
      tab_strip->GetIndexOfWebContents(&candidate.web_contents.get());
  CHECK_NE(actual_index, TabStripModel::kNoTab);

  tab_strip->ActivateTabAt(actual_index);
  candidate.browser_window->GetWindow()->Show();
  candidate.browser_window->GetWindow()->Activate();
  return true;
}

bool FocusByUrl(const Selector& selector,
                Profile& profile,
                std::string& focused_url) {
  std::vector<MatchCandidate> candidates =
      CollectMatchingElements(selector, profile);

  if (candidates.empty()) {
    return false;
  }

  SortCandidatesByMRU(candidates);
  const MatchCandidate& best_candidate = candidates[0];

  if (FocusCandidate(best_candidate)) {
    focused_url = best_candidate.matched_url;
    return true;
  }

  return false;
}

std::optional<FocusResult> FocusBestMatch(
    const std::vector<Selector>& selectors,
    Profile& profile) {
  for (const auto& selector : selectors) {
    std::string matched_url;
    std::string matched_selector = selector.ToString();

    if (FocusByUrl(selector, profile, matched_url)) {
      return FocusResult(FocusStatus::kFocused, matched_selector, matched_url);
    }
  }

  return std::nullopt;
}

FocusResult TryFocusExistingContent(const std::vector<Selector>& selectors,
                                    Profile& profile) {
  auto result = FocusBestMatch(selectors, profile);
  return result.value_or(FocusResult(FocusStatus::kNoMatch));
}

FocusResult ProcessFocusRequestWithDetails(
    const base::CommandLine& command_line,
    Profile& profile) {
  // If no focus flag is present, return no match (nothing to focus)
  if (!command_line.HasSwitch(switches::kFocus)) {
    return FocusResult(FocusStatus::kNoMatch);
  }

  // Get and validate selectors.
  std::string selectors_string =
      command_line.GetSwitchValueASCII(switches::kFocus);
  if (selectors_string.empty()) {
    return FocusResult(FocusStatus::kParseError,
                       FocusResult::Error::kEmptySelector);
  }

  std::vector<Selector> selectors = ParseSelectors(selectors_string);
  if (selectors.empty()) {
    return FocusResult(FocusStatus::kParseError,
                       FocusResult::Error::kInvalidFormat);
  }

  // Try to focus existing content.
  FocusResult result = TryFocusExistingContent(selectors, profile);
  if (result.status == FocusStatus::kFocused) {
    return result;
  }

  // If no existing content found, return no match.
  return FocusResult(FocusStatus::kNoMatch);
}

}  // namespace

FocusResult ProcessFocusRequest(const base::CommandLine& command_line,
                                Profile& profile) {
  return ProcessFocusRequestWithDetails(command_line, profile);
}

FocusResult ProcessFocusRequestWithResultFile(
    const base::CommandLine& command_line,
    Profile& profile) {
  FocusResult result = ProcessFocusRequestWithDetails(command_line, profile);

  // Write results to file if --focus-result-file is specified.
  // Skip writing result files for off-the-record profiles for privacy.
  if (command_line.HasSwitch(switches::kFocusResultFile) &&
      !profile.IsOffTheRecord()) {
    base::FilePath result_file_path =
        command_line.GetSwitchValuePath(switches::kFocusResultFile);

    WriteResultToFile(result_file_path.AsUTF8Unsafe(), result);
  }

  return result;
}

}  // namespace focus
