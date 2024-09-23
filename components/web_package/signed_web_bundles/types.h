// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_TYPES_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_TYPES_H_

#include <cstdint>
#include <vector>

#include "base/containers/flat_map.h"
#include "components/cbor/values.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_package {

class Ed25519PublicKey;
class EcdsaP256PublicKey;

using PublicKey = absl::variant<Ed25519PublicKey, EcdsaP256PublicKey>;

using BinaryData = std::vector<uint8_t>;

using AttributesMap = base::flat_map<std::string, cbor::Value>;

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_TYPES_H_
