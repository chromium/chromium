// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_ZERO_TRUST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_ZERO_TRUST_UTILS_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/signals_type.h"
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals.mojom.h"

namespace enterprise_connectors {
namespace utils {

// Manually converts the given `signals` proto to a map.
base::flat_map<std::string, std::string> SignalsToMap(
    std::unique_ptr<SignalsType> signals);

// Retrieves the KeyInfo containing any information about the currently loaded
// key.
connectors_internals::mojom::KeyInfoPtr GetKeyInfo();

}  // namespace utils
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_ZERO_TRUST_UTILS_H_
