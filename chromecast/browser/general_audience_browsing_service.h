// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_GENERAL_AUDIENCE_BROWSING_SERVICE_H_
#define CHROMECAST_BROWSER_GENERAL_AUDIENCE_BROWSING_SERVICE_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "chromecast/browser/general_audience_browsing/mojom/general_audience_browsing.mojom.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_search_api {
class URLChecker;
}  // namespace safe_search_api

namespace chromecast {

class GeneralAudienceBrowsingService
    : public mojom::GeneralAudienceBrowsingAPIKeyObserver {
 public:
  using CheckURLCallback = base::OnceCallback<void(bool is_safe)>;

  GeneralAudienceBrowsingService(
      external_service_support::ExternalConnector* connector,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

  GeneralAudienceBrowsingService(const GeneralAudienceBrowsingService&) =
      delete;
  GeneralAudienceBrowsingService& operator=(
      const GeneralAudienceBrowsingService&) = delete;

  ~GeneralAudienceBrowsingService() override;

  // Starts a call to the Safe Search API for the given URL to determine whether
  // the URL is "safe" (not porn). Returns whether |callback| was run
  // synchronously.
  bool CheckURL(const GURL& url, CheckURLCallback callback);

  // Creates a SafeSearch URLChecker using a given URLLoaderFactory for testing.
  void SetSafeSearchURLCheckerForTest(
      std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker);

  // mojom::GeneralAudienceBrowsingAPIKeyObserver implementation
  void OnGeneralAudienceBrowsingAPIKeyChanged(
      const std::string& api_key) override;

 private:
  std::unique_ptr<safe_search_api::URLChecker> CreateSafeSearchURLChecker();

  std::string api_key_;

  std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker_;

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  mojo::Receiver<mojom::GeneralAudienceBrowsingAPIKeyObserver>
      general_audience_browsing_api_key_observer_receiver_{this};
  mojo::Remote<mojom::GeneralAudienceBrowsingAPIKeySubject>
      general_audience_browsing_api_key_subject_remote_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_GENERAL_AUDIENCE_BROWSING_SERVICE_H_
