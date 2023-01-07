// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_SYSTEM_CONNECTOR_H_
#define CHROMECAST_BROWSER_SYSTEM_CONNECTOR_H_

#include <memory>

#include "services/service_manager/public/cpp/connector.h"

namespace chromecast {

// Returns a Connector which can be used to connect to service interfaces from
// the browser process. May return null in unit testing environments.
//
// This function is safe to call from any thread, but the returned pointer is
// different on each thread and is NEVER safe to retain or pass across threads.
service_manager::Connector* GetSystemConnector();

void SetSystemConnector(std::unique_ptr<service_manager::Connector> connector);

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_SYSTEM_CONNECTOR_H_
