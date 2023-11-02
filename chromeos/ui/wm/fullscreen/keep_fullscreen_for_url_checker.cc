// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/wm/fullscreen/keep_fullscreen_for_url_checker.h"

#include "chromeos/ui/wm/fullscreen/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "url/gurl.h"

namespace chromeos {

KeepFullscreenForUrlChecker::KeepFullscreenForUrlChecker(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  pref_observer_.Init(pref_service);
  pref_observer_.Add(
      prefs::kKeepFullscreenWithoutNotificationUrlAllowList,
      base::BindRepeating(&KeepFullscreenForUrlChecker::OnPrefChanged,
                          base::Unretained(this)));

  // Initialize with the current pref values.
  OnPrefChanged();
}

KeepFullscreenForUrlChecker::~KeepFullscreenForUrlChecker() = default;

bool KeepFullscreenForUrlChecker::
    IsKeepFullscreenWithoutNotificationPolicySet() {
  return url_matcher_ != nullptr;
}

bool KeepFullscreenForUrlChecker::ShouldExitFullscreenForUrl(GURL window_url) {
  return url_matcher_ ? url_matcher_->MatchURL(window_url).empty() : true;
}

void KeepFullscreenForUrlChecker::OnPrefChanged() {
  const auto& url_allow_list = pref_service_->GetList(
      prefs::kKeepFullscreenWithoutNotificationUrlAllowList);
  if (url_allow_list.size() == 0) {
    url_matcher_ = nullptr;
    return;
  }

  url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  url_matcher::util::AddAllowFilters(url_matcher_.get(), url_allow_list);
}

}  // namespace chromeos
