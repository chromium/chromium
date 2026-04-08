// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_H_

#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_experiment_api.mojom.h"

namespace tabs_api {

namespace observation {

class TabStripApiBatchedObserver;

}  // namespace observation

class TabStripService
    : public mojom::TabStripServiceDirectReturnStub,
      public mojom::TabStripExperimentServiceDirectReturnStub {
 public:
  ~TabStripService() override = default;

  // Additional API for synchronous clients:
  using GetTabsResult =
      base::expected<mojom::ContainerPtr, mojo_base::mojom::ErrorPtr>;
  // This is used by clients that would like to manually handle the observation
  // registration and deregistration. This is mainly necessary for synchronous
  // clients that want to ensure that events are delivered within the same run
  // loop.
  virtual GetTabsResult GetTabsWithoutObservation() = 0;

  // Registers and desregisters for tab events. Users must immediately register
  // for events after retrieving the tabs. They must deregister themsevles
  // before shutting down.
  virtual void AddObserver(
      observation::TabStripApiBatchedObserver* observer) = 0;
  virtual void RemoveObserver(
      observation::TabStripApiBatchedObserver* observer) = 0;

  // Mojo handling:
  virtual void Accept(
      mojo::PendingReceiver<tabs_api::mojom::TabStripService> client) = 0;
  virtual void AcceptExperimental(
      mojo::PendingReceiver<tabs_api::mojom::TabStripExperimentService>
          client) = 0;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_H_
