// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/public/autofill_assistant.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/hash/legacy_hash.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/common/signatures.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill_assistant {

AutofillAssistant::BundleCapabilitiesInformation::
    BundleCapabilitiesInformation() = default;
AutofillAssistant::BundleCapabilitiesInformation::
    ~BundleCapabilitiesInformation() = default;
AutofillAssistant::BundleCapabilitiesInformation::BundleCapabilitiesInformation(
    const BundleCapabilitiesInformation& other) = default;

AutofillAssistant::CapabilitiesInfo::CapabilitiesInfo() = default;
AutofillAssistant::CapabilitiesInfo::CapabilitiesInfo(
    const std::string& url,
    const base::flat_map<std::string, std::string>& script_parameters,
    const absl::optional<BundleCapabilitiesInformation>&
        bundle_capabilities_information)
    : url(url),
      script_parameters(script_parameters),
      bundle_capabilities_information(bundle_capabilities_information) {}
AutofillAssistant::CapabilitiesInfo::~CapabilitiesInfo() = default;
AutofillAssistant::CapabilitiesInfo::CapabilitiesInfo(
    const CapabilitiesInfo& other) = default;
AutofillAssistant::CapabilitiesInfo&
AutofillAssistant::CapabilitiesInfo::operator=(const CapabilitiesInfo& other) =
    default;

// static
uint64_t AutofillAssistant::GetHashPrefix(uint32_t hash_prefix_length,
                                          const url::Origin& origin) {
  // Right-shifts are undefined behavior if shift is by the number of bits of
  // the operand or more.
  DCHECK_GE(hash_prefix_length, 1u);
  DCHECK_LE(hash_prefix_length, 64u);
  std::string canonicalized_url = origin.GetURL().spec();
  base::TrimString(canonicalized_url, "/", &canonicalized_url);
  uint64_t hash = base::legacy::CityHash64(
      base::as_bytes(base::make_span(canonicalized_url)));
  return hash >> (64u - hash_prefix_length);
}

}  // namespace autofill_assistant
