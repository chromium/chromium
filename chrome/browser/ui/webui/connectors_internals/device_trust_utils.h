// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_DEVICE_TRUST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_DEVICE_TRUST_UTILS_H_

#include <optional>

#include "build/build_config.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/connectors_internals.mojom.h"

namespace enterprise_connectors::utils {

// Retrieves the KeyInfo containing any information about the currently loaded
// key.
connectors_internals::mojom::KeyInfoPtr GetKeyInfo();

// Returns true if the current Chrome build is allowed to delete Device Trust
// keys.
bool CanDeleteDeviceTrustKey();

}  // namespace enterprise_connectors::utils

#endif  // CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_DEVICE_TRUST_UTILS_H_
