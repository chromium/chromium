// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_UTILS_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_UTILS_H_

#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "components/plus_addresses/plus_address_types.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace plus_addresses::test {

inline constexpr char kAffiliatedFacet[] = "https://facet.affiliated";
inline constexpr char16_t kAffiliatedFacetWithoutSchemeU16[] =
    u"facet.affiliated";
inline constexpr char kFakeManagementUrl[] = "https://manage.example/";
inline constexpr char kFakeOauthScope[] = "https://foo.example";
inline constexpr char kReservePath[] = "/v1/profiles/reserve";
inline constexpr char kConfirmPath[] = "/v1/profiles/create";
inline constexpr char kFakeErrorReportUrl[] = "https://error-link.example/";

inline constexpr char kFakePlusAddress[] = "plus@plus.plus";
inline constexpr char kFakePlusAddressRefresh[] = "plus-refresh@plus.plus";
inline constexpr char16_t kFakePlusAddressU16[] = u"plus@plus.plus";
inline constexpr char16_t kFakePlusAddressRefreshU16[] =
    u"plus-refresh@plus.plus";
inline constexpr char kFakeAffiliatedPlusAddress[] =
    "plus-affiliated@plus.plus";
inline constexpr char16_t kFakeAffiliatedPlusAddressU16[] =
    u"plus-affiliated@plus.plus";

// Returns a fully populated, confirmed PlusProfile.
PlusProfile CreatePlusProfile(std::string plus_address, bool is_confirmed);
PlusProfile CreatePlusProfile();
// Returns a fully populated, confirmed PlusProfile different from
// `CreatePlusProfile()`.
PlusProfile CreatePlusProfile2();

// Returns a fully populated, confirmed PlusProfile with the given `facet`.
PlusProfile CreatePlusProfileWithFacet(const affiliations::FacetURI& facet);

// Used in testing the GetOrCreate, Reserve, and Create network requests.
std::string MakeCreationResponse(const PlusProfile& profile);

// Used in testing the List network requests.
std::string MakeListResponse(const std::vector<PlusProfile>& profiles);

// Returns the server responses that is equivalent to returning `addresses` as
// pre-allocated paddresses.
std::string MakePreallocateResponse(
    const std::vector<PreallocatedPlusAddress>& addresses);

// Converts a PlusProfile to an equivalent JSON string.
std::string MakePlusProfile(const PlusProfile& profile);

// Creates a response mimicking the plus address server.
std::unique_ptr<net::test_server::HttpResponse>
HandleRequestToPlusAddressWithSuccess(
    const net::test_server::HttpRequest& request);

// Creates a pre-allocated plus address in the same form in which it is
// serialized to prefs.
base::Value CreatePreallocatedPlusAddress(
    base::Time end_of_life,
    std::string address = "some@plus.com");

}  // namespace plus_addresses::test

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_UTILS_H_
