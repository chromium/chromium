// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_PROFILE_PRELOAD_CANDIDATE_SELECTOR_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_PROFILE_PRELOAD_CANDIDATE_SELECTOR_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/top_chrome/per_profile_webui_tracker.h"
#include "chrome/browser/ui/webui/top_chrome/preload_candidate_selector.h"
#include "url/gurl.h"

namespace webui {

// ProfilePreloadCandidateSelector selects a WebUI with the highest site
// engagement score that is not currently present under a profile. If all WebUIs
// have low engagement scores, the selector will not make a selection.
class ProfilePreloadCandidateSelector : public PreloadCandidateSelector {
 public:
  // This class uses a PerProfileWebUITracker to check the presence of WebUIs
  // under a profile. URLs that are present will not be selected.
  explicit ProfilePreloadCandidateSelector(
      PerProfileWebUITracker* webui_tracker);
  ~ProfilePreloadCandidateSelector() override;
  ProfilePreloadCandidateSelector(const ProfilePreloadCandidateSelector&) =
      delete;
  ProfilePreloadCandidateSelector& operator=(
      const ProfilePreloadCandidateSelector&) = delete;

  // PreloadCandidateSelector:
  void Init(const std::vector<GURL>& preloadable_urls) override;
  std::optional<GURL> GetURLToPreload(
      const PreloadContext& context) const override;

  void set_no_preload_if_most_engaged_url_is_background_for_testing(
      bool value) {
    no_preload_if_most_engaged_url_is_background_ = value;
  }

 private:
  // The WebUI tracker should always outlive this class.
  raw_ptr<PerProfileWebUITracker> webui_tracker_;
  std::vector<GURL> preloadable_urls_;

  // If true, the selector will not select a URL if the most engaged URL is
  // already present in the background.
  bool no_preload_if_most_engaged_url_is_background_;
};

}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_PROFILE_PRELOAD_CANDIDATE_SELECTOR_H_
