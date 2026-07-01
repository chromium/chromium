// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_FRAME_SERVICE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_FRAME_SERVICE_H_

#include <deque>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"

class GlobalBrowserCollection;
class BrowserWindowInterface;

// A singleton service that is the single source of truth for whether
// a browser window should display the glass frame or not.
class GlassFrameService : public BrowserCollectionObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnGlassFrameStateChanged(GlassFrameService* service) = 0;
  };

  static GlassFrameService* GetInstance();

  // Maximum number of windows that will display the glass frame at any given
  // time.
  static constexpr size_t kMaxGlassWindows = 1;

  GlassFrameService(const GlassFrameService&) = delete;
  GlassFrameService& operator=(const GlassFrameService&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool IsBrowserWindowEligible(BrowserWindowInterface* browser);

  GlassFrameService();
  ~GlassFrameService() override;

  // BrowserCollectionObserver:
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

 private:
  base::ObserverList<Observer> observers_;

  // Deque of tracked browsers, ordered from most recently activated to
  // least recently activated.
  std::deque<BrowserWindowInterface*> activated_browsers_;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_FRAME_SERVICE_H_
