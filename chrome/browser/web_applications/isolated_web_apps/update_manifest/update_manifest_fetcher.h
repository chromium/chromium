// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_MANIFEST_UPDATE_MANIFEST_FETCHER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_MANIFEST_UPDATE_MANIFEST_FETCHER_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "net/base/address_list.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_host_resolver.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace web_app {

class UpdateManifest;

// Helper class to download and parse an update manifest of an Isolated Web App.
class UpdateManifestFetcher {
 public:
  enum class Error {
    kDownloadFailed,
    kInvalidJson,
    kInvalidManifest,
  };

  static std::string_view ErrorToString(Error error);

  using FetchCallback =
      base::OnceCallback<void(base::expected<UpdateManifest, Error>)>;

  UpdateManifestFetcher(
      GURL url,
      net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      network::mojom::NetworkContext* network_context,
      bool report_histogram_manifest_result = false);

  ~UpdateManifestFetcher();

  // Starts downloading and parsing. Will `CHECK` if called more than once.
  void FetchUpdateManifest(FetchCallback fetch_callback);

 private:
  void OnHostResolved(
      int result,
      const net::ResolveErrorInfo& resolve_error_info,
      const net::AddressList& resolved_addresses,
      const net::HostResolverEndpointResults& alternative_endpoints);

  void DownloadUpdateManifest(network::mojom::IPAddressSpace client_space);

  void OnUpdateManifestDownloaded(
      std::optional<std::string> update_manifest_content);

  void ParseUpdateManifest(const std::string& update_manifest_content);

  GURL url_;
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  bool report_histogram_manifest_result_;

  FetchCallback fetch_callback_;

  std::unique_ptr<network::SimpleHostResolver> host_resolver_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  base::WeakPtrFactory<UpdateManifestFetcher> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_MANIFEST_UPDATE_MANIFEST_FETCHER_H_
