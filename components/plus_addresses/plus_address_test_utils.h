// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_UTILS_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_UTILS_H_

#include "components/plus_addresses/plus_address_types.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace plus_addresses::test {

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

// Returns a fully populated, confirmed PlusProfile. If `use_full_domain` is
// true, a full domain (as opposed to eTLD+1) is used as facet.
// TODO(b/322147254): Remove parameter once plus addresses starts fully relying
// on sync data.
PlusProfile CreatePlusProfile(std::string plus_address,
                              bool is_confirmed,
                              bool use_full_domain);
PlusProfile CreatePlusProfile(bool use_full_domain = false);
// Returns a fully populated, confirmed PlusProfile different from
// `CreatePlusProfile()`. If `use_full_domain` is true, a full domain (as
// opposed to eTLD+1) is used as facet.
// TODO(b/322147254): Remove parameter once plus addresses starts fully relying
// on sync data.
PlusProfile CreatePlusProfile2(bool use_full_domain = false);

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

}  // namespace plus_addresses::test

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_UTILS_H_
