// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_DEVTOOLS_PROTOCOL_ENCODING_H_
#define COMPONENTS_UI_DEVTOOLS_DEVTOOLS_PROTOCOL_ENCODING_H_

#include <string>
#include <vector>

#include "third_party/inspector_protocol/crdtp/json.h"

// Convenience adaptation of the conversion function
// crdtp::json::ConvertCBORToJSON, crdtp::json::ConvertJSONToCBOR.
// using an implementation of crdtp::json::Platform that
// delegates to base/strings/string_number_conversions.h.
namespace ui_devtools {
crdtp::Status ConvertCBORToJSON(crdtp::span<uint8_t> cbor, std::string* json);

crdtp::Status ConvertJSONToCBOR(crdtp::span<uint8_t> json, std::string* cbor);

crdtp::Status ConvertJSONToCBOR(crdtp::span<uint8_t> json,
                                std::vector<uint8_t>* cbor);
}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_DEVTOOLS_PROTOCOL_ENCODING_H_
