// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_DEVICE_TRUST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_DEVICE_TRUST_UTILS_H_

#include "chrome/browser/ui/webui/connectors_internals/connectors_internals.mojom.h"

namespace enterprise_connectors {
namespace utils {

// Retrieves the KeyInfo containing any information about the currently loaded
// key.
connectors_internals::mojom::KeyInfoPtr GetKeyInfo();

}  // namespace utils
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_DEVICE_TRUST_UTILS_H_
