// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_TABS_GLIC_NUDGE_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_GLIC_NUDGE_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/observer_list.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/tabs/glic_nudge_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class WebContents;
}

namespace tabs {

// Controller that mediates Glic Nudges and ensures that only the active tab is
// targeted.
class GlicNudgeController {
 public:
  GlicNudgeController();
  GlicNudgeController(const GlicNudgeController&) = delete;
  GlicNudgeController& operator=(const GlicNudgeController& other) = delete;
  virtual ~GlicNudgeController();

  void AddObserver(GlicNudgeObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(GlicNudgeObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  bool HasObserver(GlicNudgeObserver* observer) {
    return observers_.HasObserver(observer);
  }

  void UpdateNudgeLabel(content::WebContents* web_contents,
                        const std::string& nudge_label);

 private:
  // Returns whether the nudge should be shown in the tabstrip for glic.
  bool GlicNudgeCriteriaMet();

  base::ObserverList<GlicNudgeObserver> observers_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_GLIC_NUDGE_CONTROLLER_H_
