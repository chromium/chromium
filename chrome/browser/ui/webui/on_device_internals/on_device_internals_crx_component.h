// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ON_DEVICE_INTERNALS_ON_DEVICE_INTERNALS_CRX_COMPONENT_H_
#define CHROME_BROWSER_UI_WEBUI_ON_DEVICE_INTERNALS_ON_DEVICE_INTERNALS_CRX_COMPONENT_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/component_updater/component_updater_service.h"

namespace on_device_internals {

class PageHandler;

// Reports progress for the default model to PageHandler. PageHandler sends
// these progress updates to the chrome://on-device-internals page.
class OnDeviceInternalsCrxObserver : public component_updater::ServiceObserver {
 public:
  explicit OnDeviceInternalsCrxObserver(PageHandler& page_handler);
  ~OnDeviceInternalsCrxObserver() override;

  // component_updater::ServiceObserver:
  void OnEvent(const component_updater::CrxUpdateItem& item) override;

 private:
  // Safe because `page_handler_` owns `this`.
  base::raw_ref<PageHandler> page_handler_;

  std::string component_id_;

  base::ScopedObservation<component_updater::ComponentUpdateService,
                          component_updater::ComponentUpdateService::Observer>
      component_updater_observation_{this};
};
}  // namespace on_device_internals

#endif  // CHROME_BROWSER_UI_WEBUI_ON_DEVICE_INTERNALS_ON_DEVICE_INTERNALS_CRX_COMPONENT_H_
