// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/list_accounts_test_utils.h"

#include "google_apis/gaia/gaia_auth_test_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_features.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "services/network/test/test_url_loader_factory.h"

namespace signin {

using network::TestURLLoaderFactory;

void SetListAccountsResponseHttpNotFound(
    TestURLLoaderFactory* test_url_loader_factory) {
  test_url_loader_factory->AddResponse(
      GaiaUrls::GetInstance()
          ->ListAccountsURLWithSource(GaiaConstants::kChromeSource)
          .spec(),
      /*content=*/"", net::HTTP_NOT_FOUND);
}

void SetListAccountsResponseWithUnexpectedServiceResponse(
    TestURLLoaderFactory* test_url_loader_factory) {
  // This value is chosen to be an invalid string so that it fails to parse.
  const std::string content = "-";
  std::string source = GaiaConstants::kChromeSource;

  // Set response for first request that will lead to a one time retry request.
  test_url_loader_factory->AddResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(source).spec(),
      content);

  // Seconde request would have the source with the error as a suffix.
  test_url_loader_factory->AddResponse(
      GaiaUrls::GetInstance()
          ->ListAccountsURLWithSource(source +
                                      GaiaConstants::kUnexpectedServiceResponse)
          .spec(),
      content);
}

void SetListAccountsResponseWithParams(
    const std::vector<gaia::CookieParams>& params,
    TestURLLoaderFactory* test_url_loader_factory) {
  const std::string url =
      GaiaUrls::GetInstance()
          ->ListAccountsURLWithSource(GaiaConstants::kChromeSource)
          .spec();

  test_url_loader_factory->AddResponse(
      url, gaia::CreateListAccountsResponseInBinaryFormat(params));
}

void SetListAccountsResponseNoAccounts(
    TestURLLoaderFactory* test_url_loader_factory) {
  SetListAccountsResponseWithParams({}, test_url_loader_factory);
}

void SetListAccountsResponseOneAccount(
    const std::string& email,
    const GaiaId& gaia_id,
    TestURLLoaderFactory* test_url_loader_factory) {
  gaia::CookieParams params = {email, gaia_id, /*valid=*/true,
                               /*signed_out=*/false, /*verified=*/true};
  SetListAccountsResponseWithParams({params}, test_url_loader_factory);
}

void SetListAccountsResponseOneAccountWithParams(
    const gaia::CookieParams& params,
    TestURLLoaderFactory* test_url_loader_factory) {
  SetListAccountsResponseWithParams({params}, test_url_loader_factory);
}

void SetListAccountsResponseTwoAccounts(
    const std::string& email1,
    const GaiaId& gaia_id1,
    const std::string& email2,
    const GaiaId& gaia_id2,
    TestURLLoaderFactory* test_url_loader_factory) {
  SetListAccountsResponseWithParams(
      {{email1, gaia_id1, /*valid=*/true, /*signed_out=*/false,
        /*verified=*/true},
       {email2, gaia_id2, /*valid=*/true, /*signed_out=*/false,
        /*verified=*/true}},
      test_url_loader_factory);
}

}  // namespace signin
