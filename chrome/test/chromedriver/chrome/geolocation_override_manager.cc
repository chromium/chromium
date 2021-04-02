// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/geolocation_override_manager.h"

#include <memory>

#include "base/values.h"
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

Status GeolocationOverrideManager::OnEvent(
    DevToolsClient* client,
    const std::string& method,
    const base::DictionaryValue& params) {
  if (method == "Page.frameNavigated") {
    const base::Value* unused_value;
    if (!params.Get("frame.parentId", &unused_value))
      return ApplyOverrideIfNeeded();
  }
  return Status(kOk);
}

Status GeolocationOverrideManager::ApplyOverrideIfNeeded() {
  if (!overridden_geoposition_)
    return Status(kOk);

  base::DictionaryValue params;
  params.SetDouble("latitude", overridden_geoposition_->latitude);
  params.SetDouble("longitude", overridden_geoposition_->longitude);
  params.SetDouble("accuracy", overridden_geoposition_->accuracy);
  return client_->SendCommand("Page.setGeolocationOverride", params);
}
