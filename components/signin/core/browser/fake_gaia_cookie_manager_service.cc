// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

FakeGaiaCookieManagerService::FakeGaiaCookieManagerService(
    OAuth2TokenService* token_service,
    const std::string& source,
    SigninClient* client,
    bool use_fake_url_loader)
    : GaiaCookieManagerService(token_service, source, client) {
  if (use_fake_url_loader) {
    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory_.get());
  }
}

FakeGaiaCookieManagerService::~FakeGaiaCookieManagerService() {
  if (shared_loader_factory_)
    shared_loader_factory_->Detach();
}

void FakeGaiaCookieManagerService::SetListAccountsResponseHttpNotFound() {
  test_url_loader_factory_->AddResponse(
      GaiaUrls::GetInstance()
          ->ListAccountsURLWithSource(GaiaConstants::kChromeSource)
          .spec(),
      /*content=*/"", net::HTTP_NOT_FOUND);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseWebLoginRequired() {
  test_url_loader_factory_->AddResponse(
      GaiaUrls::GetInstance()
          ->ListAccountsURLWithSource(GaiaConstants::kChromeSource)
          .spec(),
      "Info=WebLoginRequired");
}

void FakeGaiaCookieManagerService::SetListAccountsResponseWithParams(
    const std::vector<CookieParams>& params) {
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

  test_url_loader_factory_->AddResponse(
      GaiaUrls::GetInstance()
          ->ListAccountsURLWithSource(GaiaConstants::kChromeSource)
          .spec(),
      std::string("[\"f\", [") + base::JoinString(response_body, ", ") + "]]");
}

void FakeGaiaCookieManagerService::SetListAccountsResponseNoAccounts() {
  SetListAccountsResponseWithParams({});
}

void FakeGaiaCookieManagerService::SetListAccountsResponseOneAccount(
    const std::string& email,
    const std::string& gaia_id) {
  CookieParams params = {email, gaia_id, true /* valid */,
                         false /* signed_out */, true /* verified */};
  SetListAccountsResponseWithParams({params});
}

void FakeGaiaCookieManagerService::SetListAccountsResponseOneAccountWithParams(
    const CookieParams& params) {
  SetListAccountsResponseWithParams({params});
}

void FakeGaiaCookieManagerService::SetListAccountsResponseTwoAccounts(
    const std::string& email1,
    const std::string& gaia_id1,
    const std::string& email2,
    const std::string& gaia_id2) {
  SetListAccountsResponseWithParams(
      {{email1, gaia_id1, true /* valid */, false /* signed_out */,
        true /* verified */},
       {email2, gaia_id2, true /* valid */, false /* signed_out */,
        true /* verified */}});
}

std::string FakeGaiaCookieManagerService::GetSourceForRequest(
    const GaiaCookieManagerService::GaiaCookieRequest& request) {
  // Always return the default.  This value must match the source used in the
  // SetXXXResponseYYY methods above so that the test URLFetcher factory will
  // be able to find the URLs.
  return GaiaConstants::kChromeSource;
}

std::string FakeGaiaCookieManagerService::GetDefaultSourceForRequest() {
  // Always return the default.  This value must match the source used in the
  // SetXXXResponseYYY methods above so that the test URLFetcher factory will
  // be able to find the URLs.
  return GaiaConstants::kChromeSource;
}

scoped_refptr<network::SharedURLLoaderFactory>
FakeGaiaCookieManagerService::GetURLLoaderFactory() {
  return shared_loader_factory_
             ? shared_loader_factory_
             : GaiaCookieManagerService::GetURLLoaderFactory();
}