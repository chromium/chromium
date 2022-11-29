// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/geolocation_override_manager.h"

#include <memory>

#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/geoposition.h"
#include "chrome/test/chromedriver/chrome/status.h"

GeolocationOverrideManager::GeolocationOverrideManager(DevToolsClient* client)
    : client_(client) {
  client_->AddListener(this);
}

GeolocationOverrideManager::~GeolocationOverrideManager() {
}

Status GeolocationOverrideManager::OverrideGeolocation(
    const Geoposition& geoposition) {
  overridden_geoposition_ = std::make_unique<Geoposition>(geoposition);
  return ApplyOverrideIfNeeded();
}

Status GeolocationOverrideManager::OnConnected(DevToolsClient* client) {
  return ApplyOverrideIfNeeded();
}

Status GeolocationOverrideManager::OnEvent(DevToolsClient* client,
                                           const std::string& method,
                                           const base::Value::Dict& params) {
  if (method == "Page.frameNavigated") {
    if (!params.FindByDottedPath("frame.parentId"))
      return ApplyOverrideIfNeeded();
  }
  return Status(kOk);
}

Status GeolocationOverrideManager::ApplyOverrideIfNeeded() {
  if (!overridden_geoposition_)
    return Status(kOk);

  base::Value::Dict params;
  params.Set("latitude", overridden_geoposition_->latitude);
  params.Set("longitude", overridden_geoposition_->longitude);
  params.Set("accuracy", overridden_geoposition_->accuracy);
  return client_->SendCommand("Page.setGeolocationOverride", params);
}
