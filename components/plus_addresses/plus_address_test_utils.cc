// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_test_utils.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/strings/string_util.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/plus_addresses/plus_address_preallocator.h"
#include "components/plus_addresses/plus_address_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace plus_addresses::test {

PlusProfile CreatePlusProfile(std::string plus_address, bool is_confirmed) {
  affiliations::FacetURI facet =
      affiliations::FacetURI::FromCanonicalSpec("https://foo.com");
  return PlusProfile(/*profile_id=*/"123", facet,
                     PlusAddress(std::move(plus_address)),
                     /*is_confirmed=*/is_confirmed);
}

PlusProfile CreatePlusProfile() {
  return CreatePlusProfile("plus+foo@plus.plus", /*is_confirmed=*/true);
}

PlusProfile CreatePlusProfile2() {
  affiliations::FacetURI facet =
      affiliations::FacetURI::FromCanonicalSpec("https://bar.com");
  return PlusProfile(/*profile_id=*/"234", facet,
                     PlusAddress("plus+bar@plus.plus"),
                     /*is_confirmed=*/true);
}

PlusProfile CreatePlusProfileWithFacet(const affiliations::FacetURI& facet) {
  PlusProfile profile = CreatePlusProfile();
  profile.facet = facet;
  return profile;
}

std::string MakeCreationResponse(const PlusProfile& profile) {
  std::string json = base::ReplaceStringPlaceholders(
      R"(
          {
            "plusProfile": $1
          }
        )",
      {MakePlusProfile(profile)}, nullptr);
  DCHECK(base::JSONReader::Read(json));
  return json;
}

std::string MakeListResponse(const std::vector<PlusProfile>& profiles) {
  base::Value::Dict response;
  base::Value::List list;
  for (const PlusProfile& profile : profiles) {
    std::string json = MakePlusProfile(profile);
    std::optional<base::Value::Dict> dict = base::JSONReader::ReadDict(json);
    DCHECK(dict.has_value());
    list.Append(std::move(dict.value()));
  }
  response.Set("plusProfiles", std::move(list));
  std::optional<std::string> json = base::WriteJson(response);
  DCHECK(json.has_value());
  return json.value();
}

std::string MakePreallocateResponse(
    const std::vector<PreallocatedPlusAddress>& addresses) {
  base::Value::List profiles;
  for (const PreallocatedPlusAddress& address : addresses) {
    profiles.Append(
        base::Value::Dict()
            .Set("emailAddress", *address.plus_address)
            .Set("reservationLifetime",
                 base::NumberToString(address.lifetime.InSeconds()) + "s"));
  }
  return base::WriteJson(
             base::Value::Dict().Set("emailAddresses", std::move(profiles)))
      .value();
}

std::string MakePlusProfile(const PlusProfile& profile) {
  // Note: the below must be kept in-line with the PlusAddressParser behavior.
  std::string mode = profile.is_confirmed ? "anyMode" : "UNSPECIFIED";
  std::string json = base::ReplaceStringPlaceholders(
      R"(
          {
            "ProfileId": "$1",
            "facet": "$2",
            "plusEmail": {
              "plusAddress": "$3",
              "plusMode": "$4"
            }
          }
        )",
      {*profile.profile_id, profile.facet.canonical_spec(),
       *profile.plus_address, mode},
      nullptr);
  DCHECK(base::JSONReader::Read(json));
  return json;
}

std::unique_ptr<net::test_server::HttpResponse>
HandleRequestToPlusAddressWithSuccess(
    const net::test_server::HttpRequest& request) {
  // Ignore unrecognized path.
  if (request.GetURL().path() != kReservePath &&
      request.GetURL().path() != kConfirmPath) {
    return nullptr;
  }

  bool is_refresh = [&]() {
    std::optional<base::Value> body = base::JSONReader::Read(request.content);
    if (!body || !body->is_dict() || !body->GetIfDict()) {
      return false;
    }
    return body->GetIfDict()->FindBool("refresh_email_address").value_or(false);
  }();
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("application/json");
  PlusProfile profile = CreatePlusProfile(
      /*plus_address=*/is_refresh ? kFakePlusAddressRefresh : kFakePlusAddress,
      /*is_confirmed=*/request.GetURL().path() == kConfirmPath);
  http_response->set_content(MakeCreationResponse(profile));
  return http_response;
}

base::Value CreatePreallocatedPlusAddress(base::Time end_of_life,
                                          std::string address) {
  return base::Value(
      base::Value::Dict()
          .Set(PlusAddressPreallocator::kEndOfLifeKey,
               base::TimeToValue(end_of_life))
          .Set(PlusAddressPreallocator::kPlusAddressKey, std::move(address)));
}

}  // namespace plus_addresses::test
