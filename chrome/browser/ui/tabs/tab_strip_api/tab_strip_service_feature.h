// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_FEATURE_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_FEATURE_H_

#include <memory>

#include "build/build_config.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_service.h"
#include "components/browser_apis/tab_strip/tab_strip_service_view.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/browser_apis/tab_strip/tab_strip_experiment_api.mojom.h"
#endif

class BrowserWindowInterface;

namespace tabs_api {
class PlatformAdaptersProvider;
}  // namespace tabs_api

// Public interface for retrieving the tab strip service, either through mojo
// or the native interface.
class TabStripServiceFeature {
 public:
  DECLARE_USER_DATA(TabStripServiceFeature);

  TabStripServiceFeature(
      std::unique_ptr<tabs_api::PlatformAdaptersProvider> provider,
      ui::UnownedUserDataHost& host);
  ~TabStripServiceFeature();

  // Returns the feature for `browser`, or null if it does not have one.
  static TabStripServiceFeature* From(BrowserWindowInterface* browser);

  void Accept(mojo::PendingReceiver<tabs_api::mojom::TabStripService> client);
#if !BUILDFLAG(IS_ANDROID)
  void AcceptExperimental(
      mojo::PendingReceiver<tabs_api::mojom::TabStripExperimentService> client);
#endif
  tabs_api::TabStripService* GetTabStripService() const;
  tabs_api::TabStripServiceView* GetTabStripServiceView();

 private:
  std::unique_ptr<tabs_api::TabStripService> tab_strip_service_;
  std::unique_ptr<tabs_api::TabStripServiceView> tab_strip_service_view_;

  ui::ScopedUnownedUserData<TabStripServiceFeature> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_FEATURE_H_
