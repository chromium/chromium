// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/on_device_internals/on_device_internals_crx_component.h"

#include <memory>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/optimization_guide_on_device_model_installer.h"
#include "chrome/browser/ui/webui/on_device_internals/on_device_internals_page_handler.h"
#include "components/update_client/crx_update_item.h"

namespace on_device_internals {

namespace {

bool IsDownloadEvent(const component_updater::CrxUpdateItem& item) {
  // See class comment: components/update_client/component.h
  switch (item.state) {
    case update_client::ComponentState::kDownloading:
    case update_client::ComponentState::kDecompressing:
    case update_client::ComponentState::kPatching:
    case update_client::ComponentState::kUpdating:
    case update_client::ComponentState::kUpToDate:
      return item.downloaded_bytes >= 0 && item.total_bytes >= 0;
    case update_client::ComponentState::kNew:
    case update_client::ComponentState::kChecking:
    case update_client::ComponentState::kCanUpdate:
    case update_client::ComponentState::kUpdated:
    case update_client::ComponentState::kUpdateError:
    case update_client::ComponentState::kRun:
      return false;
  }
}

bool IsAlreadyInstalled(const component_updater::CrxUpdateItem& item) {
  // See class comment: components/update_client/component.h
  switch (item.state) {
    case update_client::ComponentState::kUpdated:
    case update_client::ComponentState::kUpToDate:
      return true;
    case update_client::ComponentState::kNew:
    case update_client::ComponentState::kChecking:
    case update_client::ComponentState::kCanUpdate:
    case update_client::ComponentState::kDownloading:
    case update_client::ComponentState::kDecompressing:
    case update_client::ComponentState::kPatching:
    case update_client::ComponentState::kUpdating:
    case update_client::ComponentState::kUpdateError:
    case update_client::ComponentState::kRun:
      return false;
  }
}

}  // namespace

OnDeviceInternalsCrxObserver::OnDeviceInternalsCrxObserver(
    PageHandler& page_handler)
    : page_handler_(page_handler),
      component_id_(
          component_updater::GetOptimizationGuideOnDeviceModelExtensionId(
              component_updater::OnDeviceModelType::kBaseModel)) {
  component_updater::CrxUpdateItem item;
  bool success = g_browser_process->component_updater()->GetComponentDetails(
      component_id_, &item);

  if (success && IsAlreadyInstalled(item)) {
    return;
  }

  // Watch for progress updates.
  component_updater_observation_.Observe(
      g_browser_process->component_updater());
}

OnDeviceInternalsCrxObserver::~OnDeviceInternalsCrxObserver() = default;

void OnDeviceInternalsCrxObserver::OnEvent(
    const component_updater::CrxUpdateItem& item) {
  if (!IsDownloadEvent(item) || item.id != component_id_) {
    return;
  }

  page_handler_->SendDownloadProgress(item.downloaded_bytes, item.total_bytes);
}

}  // namespace on_device_internals
