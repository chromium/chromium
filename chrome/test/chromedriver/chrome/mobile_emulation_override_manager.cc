// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/mobile_emulation_override_manager.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/test/chromedriver/chrome/device_metrics.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

namespace {

Status OverrideDeviceMetricsIfNeeded(DevToolsClient* client,
                                     const MobileDevice& mobile_device) {
  Status status{kOk};
  if (!mobile_device.device_metrics.has_value()) {
    return status;
  }
  base::Value::Dict params;
  params.Set("width", mobile_device.device_metrics->width);
  params.Set("height", mobile_device.device_metrics->height);
  params.Set("deviceScaleFactor",
             mobile_device.device_metrics->device_scale_factor);
  params.Set("mobile", mobile_device.device_metrics->mobile);
  status = client->SendCommand("Emulation.setDeviceMetricsOverride", params);
  if (status.IsError()) {
    return status;
  }

  if (mobile_device.device_metrics->touch) {
    base::Value::Dict emulate_touch_params;
    emulate_touch_params.Set("enabled", true);
    status = client->SendCommand("Emulation.setTouchEmulationEnabled",
                                 emulate_touch_params);
    if (status.IsError()) {
      return status;
    }
  }
  return status;
}

Status OverrideClientHintsIfNeeded(DevToolsClient* client,
                                   const MobileDevice& mobile_device,
                                   int major_version) {
  Status status{kOk};
  if (!mobile_device.client_hints.has_value()) {
    return status;
  }
  std::string user_agent;
  if (mobile_device.user_agent.has_value()) {
    std::string version = base::StringPrintf("%d.0.0.0", major_version);
    user_agent = base::StringPrintfNonConstexpr(
        mobile_device.user_agent.value().c_str(), version.c_str());
  } else {
    std::string major_version_str = base::NumberToString(major_version);
    status = mobile_device.GetReducedUserAgent(std::move(major_version_str),
                                               &user_agent);
    if (status.IsError()) {
      return status;
    }
  }
  base::Value::Dict ua_metadata;
  ua_metadata.Set("architecture", mobile_device.client_hints->architecture);
  ua_metadata.Set("bitness", mobile_device.client_hints->bitness);
  ua_metadata.Set("mobile", mobile_device.client_hints->mobile);
  ua_metadata.Set("model", mobile_device.client_hints->model);
  ua_metadata.Set("platform", mobile_device.client_hints->platform);
  ua_metadata.Set("platformVersion",
                  mobile_device.client_hints->platform_version);
  ua_metadata.Set("wow64", mobile_device.client_hints->wow64);
  if (mobile_device.client_hints->brands.has_value()) {
    base::Value::List brands;
    for (const BrandVersion& bv : mobile_device.client_hints->brands.value()) {
      base::Value::Dict brand;
      brand.Set("brand", bv.brand);
      brand.Set("version", bv.version);
      brands.Append(std::move(brand));
    }
    ua_metadata.Set("brands", std::move(brands));
  }
  if (mobile_device.client_hints->full_version_list.has_value()) {
    base::Value::List brands;
    for (const BrandVersion& bv :
         mobile_device.client_hints->full_version_list.value()) {
      base::Value::Dict brand;
      brand.Set("brand", bv.brand);
      brand.Set("version", bv.version);
      brands.Append(std::move(brand));
    }
    ua_metadata.Set("fullVersionList", std::move(brands));
  }
  base::Value::Dict params;
  params.Set("userAgent", user_agent);
  params.Set("userAgentMetadata", std::move(ua_metadata));
  status =
      client->SendCommand("Emulation.setUserAgentOverride", std::move(params));
  return status;
}

}  // namespace

MobileEmulationOverrideManager::MobileEmulationOverrideManager(
    DevToolsClient* client,
    std::optional<MobileDevice> mobile_device,
    int browser_major_version)
    : client_(client),
      mobile_device_(mobile_device),
      browser_major_version_(browser_major_version) {
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
  Status status{kOk};
  if (!mobile_device_.has_value()) {
    return status;
  }

  status = OverrideDeviceMetricsIfNeeded(client_.get(), mobile_device_.value());
  if (status.IsError())
    return status;

  status = OverrideClientHintsIfNeeded(client_.get(), mobile_device_.value(),
                                       browser_major_version_);

  return status;
}
