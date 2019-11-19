// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/devtools_protocol_encoding.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"

namespace ui_devtools {
namespace {

// Platform allows us to inject the string<->double conversion
// routines from base:: into the inspector_protocol JSON parser / serializer.
class Platform : public crdtp::json::Platform {
 public:
  bool StrToD(const char* str, double* result) const override {
    return base::StringToDouble(str, result);
  }

  // Prints |value| in a format suitable for JSON.
  std::unique_ptr<char[]> DToStr(double value) const override {
    std::string str = base::NumberToString(value);
    std::unique_ptr<char[]> result(new char[str.size() + 1]);
    memcpy(result.get(), str.c_str(), str.size() + 1);
    return result;
  }
};
}  // namespace

crdtp::Status ConvertCBORToJSON(crdtp::span<uint8_t> cbor, std::string* json) {
  Platform platform;
  return crdtp::json::ConvertCBORToJSON(platform, cbor, json);
}

crdtp::Status ConvertJSONToCBOR(crdtp::span<uint8_t> json, std::string* cbor) {
  Platform platform;
  return ConvertJSONToCBOR(platform, json, cbor);
}

crdtp::Status ConvertJSONToCBOR(crdtp::span<uint8_t> json,
                                std::vector<uint8_t>* cbor) {
  Platform platform;
  return ConvertJSONToCBOR(platform, json, cbor);
}

}  // namespace ui_devtools
