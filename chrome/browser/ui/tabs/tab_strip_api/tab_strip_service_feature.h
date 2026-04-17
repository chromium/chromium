// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_FEATURE_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_FEATURE_H_

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_experiment_api.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace tabs_api {
class PlatformAdaptersProvider;
class ExperimentalPlatformAdaptersProvider;
}  // namespace tabs_api

// Public interface for retrieving the tab strip service, either through mojo
// or the native interface.
class TabStripServiceFeature {
 public:
  TabStripServiceFeature(
      std::unique_ptr<tabs_api::PlatformAdaptersProvider> provider,
      std::unique_ptr<tabs_api::ExperimentalPlatformAdaptersProvider>
          experimental_provider);
  ~TabStripServiceFeature();

  void Accept(mojo::PendingReceiver<tabs_api::mojom::TabStripService> client);
  void AcceptExperimental(
      mojo::PendingReceiver<tabs_api::mojom::TabStripExperimentService> client);
  tabs_api::TabStripService* GetTabStripService() const;

 private:
  std::unique_ptr<tabs_api::TabStripService> tab_strip_service_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_FEATURE_H_
