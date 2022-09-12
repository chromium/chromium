// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_WM_FULLSCREEN_KEEP_FULLSCREEN_FOR_URL_CHECKER_H_
#define CHROMEOS_UI_WM_FULLSCREEN_KEEP_FULLSCREEN_FOR_URL_CHECKER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/url_matcher/url_matcher.h"

class GURL;

namespace chromeos {

// Initialized with the given PrefService, the KeepFullscreenForUrlChecker is
// used to check if it is allowed by user pref to keep fullscreen after unlock.
class KeepFullscreenForUrlChecker {
 public:
  explicit KeepFullscreenForUrlChecker(PrefService* pref_service);

  KeepFullscreenForUrlChecker(const KeepFullscreenForUrlChecker&) = delete;
  KeepFullscreenForUrlChecker& operator=(const KeepFullscreenForUrlChecker&) =
      delete;

  ~KeepFullscreenForUrlChecker();

  // Returns true if the KeepFullscreenWithoutNotification value is set in the
  // corresponding PrefService, otherwise returns false.
  bool IsKeepFullscreenWithoutNotificationPolicySet();

  // Returns true if it is not allowed by user pref to keep full screen for the
  // given window URL. Returns false if it is allowed to keep full screen, that
  // is, if |window_url| matches a pattern in the
  // KeepFullscreenWithoutNotification policy of the corresponding PrefService.
  bool ShouldExitFullscreenForUrl(GURL window_url);

 private:
  // Updates the filters of the URLMatcher when the
  // KeepFullscreenWithoutNotificationUrlAllowList pref changed.
  void OnPrefChanged();

  PrefChangeRegistrar pref_observer_;

  raw_ptr<PrefService> pref_service_;

  std::unique_ptr<url_matcher::URLMatcher> url_matcher_;

  base::WeakPtrFactory<KeepFullscreenForUrlChecker> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_WM_FULLSCREEN_KEEP_FULLSCREEN_FOR_URL_CHECKER_H_
