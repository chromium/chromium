// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_SERVICE_MANAGER_CONNECTION_H_
#define CHROMECAST_BROWSER_SERVICE_MANAGER_CONNECTION_H_

#include "content/public/common/service_manager_connection.h"

namespace chromecast {

// Temporary alias until the downstream internal repository can update its
// #includes to reference this file rather than the one in Content.
using ServiceManagerConnection = content::ServiceManagerConnection;

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_SERVICE_MANAGER_CONNECTION_H_
