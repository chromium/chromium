// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/list_accounts_test_utils.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
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
  std::string source = GaiaConstants::kChromeSource;
  // Set response for first request that will lead to a one time retry request.
  test_url_loader_factory->AddResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(source).spec(), "");

  // Seconde request would have the source with the error as a suffix.
  test_url_loader_factory->AddResponse(
      GaiaUrls::GetInstance()
          ->ListAccountsURLWithSource(source +
                                      GaiaConstants::kUnexpectedServiceResponse)
          .spec(),
      "");
}

void SetListAccountsResponseWithParams(
    const std::vector<CookieParams>& params,
    TestURLLoaderFactory* test_url_loader_factory) {
  std::vector<std::string> response_body;
  for (const auto& param : params) {
    std::string response_part = base::StringPrintf(
        "[\"b\", 0, \"n\", \"%s\", \"p\", 0, 0, 0, 0, %d, \"%s\"",
        param.email.c_str(), param.valid ? 1 : 0, param.gaia_id.c_str());
    if (param.signed_out || !param.verified) {
      response_part +=
          base::StringPrintf(", null, null, null, %d, %d",
                             param.signed_out ? 1 : 0, param.verified ? 1 : 0);
    }
    response_part += "]";
    response_body.push_back(response_part);
  }

  test_url_loader_factory->AddResponse(
      GaiaUrls::GetInstance()
          ->ListAccountsURLWithSource(GaiaConstants::kChromeSource)
          .spec(),
      std::string("[\"f\", [") + base::JoinString(response_body, ", ") + "]]");
}

void SetListAccountsResponseNoAccounts(
    TestURLLoaderFactory* test_url_loader_factory) {
  SetListAccountsResponseWithParams({}, test_url_loader_factory);
}

void SetListAccountsResponseOneAccount(
    const std::string& email,
    const std::string& gaia_id,
    TestURLLoaderFactory* test_url_loader_factory) {
  CookieParams params = {email, gaia_id, /*valid=*/true,
                         /*signed_out=*/false, /*verified=*/true};
  SetListAccountsResponseWithParams({params}, test_url_loader_factory);
}

void SetListAccountsResponseOneAccountWithParams(
    const CookieParams& params,
    TestURLLoaderFactory* test_url_loader_factory) {
  SetListAccountsResponseWithParams({params}, test_url_loader_factory);
}

void SetListAccountsResponseTwoAccounts(
    const std::string& email1,
    const std::string& gaia_id1,
    const std::string& email2,
    const std::string& gaia_id2,
    TestURLLoaderFactory* test_url_loader_factory) {
  SetListAccountsResponseWithParams(
      {{email1, gaia_id1, /*valid=*/true, /*signed_out=*/false,
        /*verified=*/true},
       {email2, gaia_id2, /*valid=*/true, /*signed_out=*/false,
        /*verified=*/true}},
      test_url_loader_factory);
}

}  // namespace signin
