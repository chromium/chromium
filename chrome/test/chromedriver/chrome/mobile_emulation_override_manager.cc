// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/mobile_emulation_override_manager.h"

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/device_metrics.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

MobileEmulationOverrideManager::MobileEmulationOverrideManager(
    DevToolsClient* client,
    const DeviceMetrics* device_metrics)
    : client_(client),
      overridden_device_metrics_(device_metrics) {
  if (overridden_device_metrics_)
    client_->AddListener(this);
}

MobileEmulationOverrideManager::~MobileEmulationOverrideManager() {
}

Status MobileEmulationOverrideManager::OnConnected(DevToolsClient* client) {
  return ApplyOverrideIfNeeded();
}

Status MobileEmulationOverrideManager::OnEvent(
    DevToolsClient* client,
    const std::string& method,
    const base::DictionaryValue& params) {
  if (method == "Page.frameNavigated") {
    if (!params.FindPath("frame.parentId"))
      return ApplyOverrideIfNeeded();
  }
  return Status(kOk);
}

bool MobileEmulationOverrideManager::IsEmulatingTouch() const {
  return overridden_device_metrics_ && overridden_device_metrics_->touch;
}

bool MobileEmulationOverrideManager::HasOverrideMetrics() const {
  return overridden_device_metrics_;
}

Status MobileEmulationOverrideManager::RestoreOverrideMetrics() {
  return ApplyOverrideIfNeeded();
}

const DeviceMetrics* MobileEmulationOverrideManager::GetDeviceMetrics() const {
  return overridden_device_metrics_;
}

Status MobileEmulationOverrideManager::ApplyOverrideIfNeeded() {
  if (!overridden_device_metrics_)
    return Status(kOk);

  base::DictionaryValue params;
  params.GetDict().Set("width", overridden_device_metrics_->width);
  params.GetDict().Set("height", overridden_device_metrics_->height);
  params.GetDict().Set("deviceScaleFactor",
                       overridden_device_metrics_->device_scale_factor);
  params.GetDict().Set("mobile", overridden_device_metrics_->mobile);
  params.GetDict().Set("fitWindow", overridden_device_metrics_->fit_window);
  params.GetDict().Set("textAutosizing",
                       overridden_device_metrics_->text_autosizing);
  params.GetDict().Set("fontScaleFactor",
                       overridden_device_metrics_->font_scale_factor);
  Status status = client_->SendCommand("Page.setDeviceMetricsOverride", params);
  if (status.IsError())
    return status;

  if (overridden_device_metrics_->touch) {
    base::DictionaryValue emulate_touch_params;
    emulate_touch_params.GetDict().Set("enabled", true);
    status = client_->SendCommand("Emulation.setTouchEmulationEnabled",
                                  emulate_touch_params);
    if (status.IsError())
      return status;
  }

  return Status(kOk);
}
