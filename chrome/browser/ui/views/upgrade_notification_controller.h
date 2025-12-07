// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_UPGRADE_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_UPGRADE_NOTIFICATION_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/views/critical_notification_bubble_view.h"
#endif

class BrowserWindowInterface;

// Responsible for observing outdated install and critical upgrade notifications
// from the UpgradeDetector and updating browser UI appropriately.
class UpgradeNotificationController : public UpgradeObserver {
 public:
  explicit UpgradeNotificationController(BrowserWindowInterface* browser);
  UpgradeNotificationController(const UpgradeNotificationController&) = delete;
  UpgradeNotificationController& operator=(
      const UpgradeNotificationController&) = delete;
  ~UpgradeNotificationController() override;

  // UpgradeObserver:
  void OnOutdatedInstall() override;
  void OnOutdatedInstallNoAutoUpdate() override;
  void OnCriticalUpgradeInstalled() override;
#if BUILDFLAG(IS_WIN)
  std::unique_ptr<CriticalNotificationBubbleView>
  GetCriticalNotificationBubbleViewForTest();
#endif
 private:
  const raw_ref<BrowserWindowInterface> browser_;
  base::ScopedObservation<UpgradeDetector, UpgradeObserver>
      upgrade_detector_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_UPGRADE_NOTIFICATION_CONTROLLER_H_
