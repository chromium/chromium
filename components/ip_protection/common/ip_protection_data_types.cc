// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_data_types.h"

#include <array>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"

namespace ip_protection {

namespace {

// Size of a PRT when TLS serialized, before base64 encoding.
constexpr size_t kPRTSize = 79;
constexpr size_t kPRTPointSize = 33;
constexpr size_t kEpochIdSize = 8;

}  // namespace

std::string GetGeoIdFromGeoHint(const std::optional<GeoHint> geo_hint) {
  if (!geo_hint.has_value()) {
    return "";  // If nullopt, return empty string.
  }

  std::string geo_id = geo_hint->country_code;
  if (!geo_hint->iso_region.empty()) {
    geo_id += "," + geo_hint->iso_region;
  }
  if (!geo_hint->city_name.empty()) {
    geo_id += "," + geo_hint->city_name;
  }

  return geo_id;
}

// TODO(crbug.com/40176497): IN-TEST does not work for multi-line declarations.
std::optional<GeoHint> GetGeoHintFromGeoIdForTesting(  // IN-TEST
    const std::string& geo_id) {
  if (geo_id.empty()) {
    return std::nullopt;  // Return nullopt if the geo_id is empty.
  }
  GeoHint geo_hint;
  std::stringstream geo_id_stream(geo_id);
  std::string segment;

  // Extract country code.
  if (std::getline(geo_id_stream, segment, ',')) {
    geo_hint.country_code = segment;
  }

  // Extract ISO region.
  if (std::getline(geo_id_stream, segment, ',')) {
    geo_hint.iso_region = segment;
  }

  // Extract city name.
  if (std::getline(geo_id_stream, segment, ',')) {
    geo_hint.city_name = segment;
  }

  return geo_hint;
}

std::vector<MdlType> FromMdlResourceProto(
    const masked_domain_list::Resource& resource) {
  std::vector<MdlType> mdl_types;

  if (!resource.exclude_default_group()) {
    mdl_types.emplace_back(MdlType::kIncognito);
  }

  if (base::Contains(resource.experiments(),
                     masked_domain_list::Resource::Experiment::
                         Resource_Experiment_EXPERIMENT_EXTERNAL_REGULAR)) {
    mdl_types.emplace_back(MdlType::kRegularBrowsing);
  }

  return mdl_types;
}

TryGetProbabilisticRevealTokensOutcome::
    TryGetProbabilisticRevealTokensOutcome() = default;
TryGetProbabilisticRevealTokensOutcome::
    ~TryGetProbabilisticRevealTokensOutcome() = default;
TryGetProbabilisticRevealTokensOutcome::TryGetProbabilisticRevealTokensOutcome(
    const TryGetProbabilisticRevealTokensOutcome& other) = default;
TryGetProbabilisticRevealTokensOutcome::TryGetProbabilisticRevealTokensOutcome(
    TryGetProbabilisticRevealTokensOutcome&& other) = default;
TryGetProbabilisticRevealTokensOutcome&
TryGetProbabilisticRevealTokensOutcome::operator=(
    const TryGetProbabilisticRevealTokensOutcome&) = default;
TryGetProbabilisticRevealTokensOutcome&
TryGetProbabilisticRevealTokensOutcome::operator=(
    TryGetProbabilisticRevealTokensOutcome&&) = default;

ProbabilisticRevealToken::ProbabilisticRevealToken() = default;
ProbabilisticRevealToken::ProbabilisticRevealToken(std::int32_t version,
                                                   std::string u,
                                                   std::string e,
                                                   std::string epoch_id)
    : version(version),
      u(std::move(u)),
      e(std::move(e)),
      epoch_id(std::move(epoch_id)) {}
ProbabilisticRevealToken::ProbabilisticRevealToken(
    const ProbabilisticRevealToken&) = default;
ProbabilisticRevealToken::ProbabilisticRevealToken(ProbabilisticRevealToken&&) =
    default;
ProbabilisticRevealToken& ProbabilisticRevealToken::operator=(
    const ProbabilisticRevealToken&) = default;
ProbabilisticRevealToken& ProbabilisticRevealToken::operator=(
    ProbabilisticRevealToken&&) = default;
ProbabilisticRevealToken::~ProbabilisticRevealToken() = default;

/*
Serialize and base64 encode the following struct given in TLS presentation
language (rfc8446 section-3). Size of u and e depends on the version and only
possible version value is 1 for now. Only possible size for u and e is 33.
Returns null in case of failure.

struct {
  uint8 version;
  opaque u<0..2^16-1>;
  opaque e<0..2^16-1>;
  opaque epoch_id[8];
} tlsPRT;

Once serialized (before base64 encoding), output bytes will be as follows.

[1 byte for version |
 2 bytes for u size | 33 bytes for u |
 2 bytes for e size | 33 bytes for e |
 8 bytes for epoch_id]
*/
std::optional<std::string> ProbabilisticRevealToken::SerializeAndEncode()
    const {
  if (version != 1 || u.size() != e.size() || u.size() != kPRTPointSize ||
      epoch_id.size() != kEpochIdSize) {
    return std::nullopt;
  }
  bssl::ScopedCBB cbb;
  std::array<uint8_t, kPRTSize> prt;
  size_t cbb_size = 0;
  // CBB doc says CBB_init_fixed will not fail.
  CHECK(CBB_init_fixed(cbb.get(), prt.data(), kPRTSize));
  if (!CBB_add_u8(cbb.get(), version) || !CBB_add_u16(cbb.get(), u.size()) ||
      !CBB_add_bytes(cbb.get(), reinterpret_cast<const uint8_t*>(u.data()),
                     u.size()) ||
      !CBB_add_u16(cbb.get(), e.size()) ||
      !CBB_add_bytes(cbb.get(), reinterpret_cast<const uint8_t*>(e.data()),
                     e.size()) ||
      !CBB_add_bytes(cbb.get(),
                     reinterpret_cast<const uint8_t*>(epoch_id.data()),
                     epoch_id.size()) ||
      !CBB_finish(cbb.get(), nullptr, &cbb_size)) {
    return std::nullopt;
  }
  CHECK_EQ(cbb_size, kPRTSize);
  return base::Base64Encode(prt);
}

}  // namespace ip_protection
