// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/mobile_emulation_override_manager.h"

#include "chrome/test/chromedriver/chrome/device_metrics.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

MobileEmulationOverrideManager::MobileEmulationOverrideManager(
    DevToolsClient* client,
    absl::optional<MobileDevice> mobile_device)
    : client_(client), mobile_device_(mobile_device) {
  if (mobile_device_) {
    client_->AddListener(this);
  }
}

MobileEmulationOverrideManager::~MobileEmulationOverrideManager() = default;

Status MobileEmulationOverrideManager::OnConnected(DevToolsClient* client) {
  return ApplyOverrideIfNeeded();
}

Status MobileEmulationOverrideManager::OnEvent(
    DevToolsClient* client,
    const std::string& method,
    const base::Value::Dict& params) {
  if (method == "Page.frameNavigated") {
    if (!params.FindByDottedPath("frame.parentId"))
      return ApplyOverrideIfNeeded();
  }
  return Status(kOk);
}

bool MobileEmulationOverrideManager::IsEmulatingTouch() const {
  return HasOverrideMetrics() && mobile_device_->device_metrics->touch;
}

bool MobileEmulationOverrideManager::HasOverrideMetrics() const {
  return mobile_device_.has_value() &&
         mobile_device_->device_metrics.has_value();
}

Status MobileEmulationOverrideManager::RestoreOverrideMetrics() {
  return ApplyOverrideIfNeeded();
}

const DeviceMetrics* MobileEmulationOverrideManager::GetDeviceMetrics() const {
  if (!HasOverrideMetrics()) {
    return nullptr;
  }
  return &mobile_device_->device_metrics.value();
}

Status MobileEmulationOverrideManager::ApplyOverrideIfNeeded() {
  if (!HasOverrideMetrics()) {
    return Status(kOk);
  }

  base::Value::Dict params;
  params.Set("width", mobile_device_->device_metrics->width);
  params.Set("height", mobile_device_->device_metrics->height);
  params.Set("deviceScaleFactor",
             mobile_device_->device_metrics->device_scale_factor);
  params.Set("mobile", mobile_device_->device_metrics->mobile);
  params.Set("fitWindow", mobile_device_->device_metrics->fit_window);
  params.Set("textAutosizing", mobile_device_->device_metrics->text_autosizing);
  params.Set("fontScaleFactor",
             mobile_device_->device_metrics->font_scale_factor);
  Status status = client_->SendCommand("Page.setDeviceMetricsOverride", params);
  if (status.IsError())
    return status;

  if (mobile_device_->device_metrics->touch) {
    base::Value::Dict emulate_touch_params;
    emulate_touch_params.Set("enabled", true);
    status = client_->SendCommand("Emulation.setTouchEmulationEnabled",
                                  emulate_touch_params);
    if (status.IsError())
      return status;
  }

  return Status(kOk);
}
