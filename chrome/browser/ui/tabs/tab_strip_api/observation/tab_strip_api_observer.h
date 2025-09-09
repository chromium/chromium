// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_OBSERVATION_TAB_STRIP_API_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_OBSERVATION_TAB_STRIP_API_OBSERVER_H_

namespace tabs_api::observation {

class TabStripApiObserver {
 public:
  virtual ~TabStripApiObserver() = default;

  virtual void OnTabEvents(const std::vector<mojom::TabsEventPtr>& events) = 0;
};

}  // namespace tabs_api::observation

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_OBSERVATION_TAB_STRIP_API_OBSERVER_H_
