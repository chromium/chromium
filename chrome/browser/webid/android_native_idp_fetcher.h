// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_ANDROID_NATIVE_IDP_FETCHER_H_
#define CHROME_BROWSER_WEBID_ANDROID_NATIVE_IDP_FETCHER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/webid/verified_origin_resolver.h"
#include "content/public/browser/webid/native_idp_fetcher.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::webid {
class IdentityProviderService;
}  // namespace content::webid

namespace chrome {

class AndroidNativeIdpFetcher : public content::NativeIdpFetcher {
 public:
  explicit AndroidNativeIdpFetcher(const url::Origin& idp_origin);
  ~AndroidNativeIdpFetcher() override;

  AndroidNativeIdpFetcher(const AndroidNativeIdpFetcher&) = delete;
  AndroidNativeIdpFetcher& operator=(const AndroidNativeIdpFetcher&) = delete;

  // content::NativeIdpFetcher:
  void FetchAccounts(const GURL& accounts_url, FetchCallback callback) override;

 private:
  void OnOriginResolved(
      const std::string& request,
      const content::webid::VerifiedOriginResolver::Result& result);
  void OnConnected(const std::string& request, bool connected);
  void OnFetched(const std::optional<std::string>& response);

  url::Origin idp_origin_;
  std::unique_ptr<content::webid::VerifiedOriginResolver> resolver_;
  std::unique_ptr<content::webid::IdentityProviderService> idp_service_;
  FetchCallback pending_callback_;

  base::WeakPtrFactory<AndroidNativeIdpFetcher> weak_ptr_factory_{this};
};

}  // namespace chrome

#endif  // CHROME_BROWSER_WEBID_ANDROID_NATIVE_IDP_FETCHER_H_
