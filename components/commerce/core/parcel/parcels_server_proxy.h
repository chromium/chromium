// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PARCEL_PARCELS_SERVER_PROXY_H_
#define COMPONENTS_COMMERCE_CORE_PARCEL_PARCELS_SERVER_PROXY_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/commerce/core/parcel/parcels_utils.h"
#include "components/commerce/core/proto/parcel.pb.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

class EndpointFetcher;
class GURL;
struct EndpointResponse;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace commerce {

// Class for getting the parcel tracking status from the server.
class ParcelsServerProxy {
 public:
  using GetParcelStatusCallback =
      base::OnceCallback<void(bool /*success*/,
                              std::unique_ptr<std::vector<ParcelStatus>>)>;
  using StopParcelTrackingCallback = base::OnceCallback<void(bool /*success*/)>;
  using EndpointCallback =
      base::OnceCallback<void(std::unique_ptr<EndpointFetcher>,
                              std::unique_ptr<EndpointResponse>)>;

  ParcelsServerProxy(
      signin::IdentityManager* identity_manager,
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory);
  virtual ~ParcelsServerProxy();
  ParcelsServerProxy(const ParcelsServerProxy& other) = delete;
  ParcelsServerProxy& operator=(const ParcelsServerProxy& other) = delete;

  // Get parcel status.
  virtual void GetParcelStatus(
      const std::vector<ParcelIdentifier>& parcel_identifiers,
      GetParcelStatusCallback callback);

  // Start tracking parcel status.
  virtual void StartTrackingParcels(
      const std::vector<ParcelIdentifier>& parcel_identifiers,
      const std::string& source_page_domain,
      GetParcelStatusCallback callback);

  // Called to stop tracking a given parcel.
  virtual void StopTrackingParcel(const std::string& tracking_id,
                                  StopParcelTrackingCallback callback);

  // Called to stop tracking multiple parcels.
  virtual void StopTrackingParcels(
      const std::vector<ParcelIdentifier>& parcel_identifiers,
      StopParcelTrackingCallback callback);

  // Called to stop tracking all parcels.
  virtual void StopTrackingAllParcels(StopParcelTrackingCallback callback);

 protected:
  // This method could be overridden in tests.
  virtual std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      const GURL& url,
      const std::string& http_method,
      const std::string& post_data,
      const net::NetworkTrafficAnnotationTag traffic_annotation);

 private:
  // Parse the server response to get the parcel status.
  void ProcessGetParcelStatusResponse(
      ParcelRequestType request_type,
      GetParcelStatusCallback callback,
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      std::unique_ptr<EndpointResponse> response);

  // Called when json string from server response is parsed.
  void OnGetParcelStatusJsonParsed(
      ParcelRequestType request_type,
      GetParcelStatusCallback callback,
      data_decoder::DataDecoder::ValueOrError result);

  // Called when response for stop tracking request is returned.
  void OnStopTrackingResponse(ParcelRequestType request_type,
                              StopParcelTrackingCallback callback,
                              std::unique_ptr<EndpointFetcher> endpoint_fetcher,
                              std::unique_ptr<EndpointResponse> response);

  // Helper method that is called when a server response is received.
  void OnServerResponse(EndpointCallback callback,
                        std::unique_ptr<EndpointFetcher> endpoint_fetcher,
                        std::unique_ptr<EndpointResponse> response);

  // Helper method to send a json request to a server.
  void SendJsonRequestToServer(
      base::Value::Dict request_json,
      const GURL& server_url,
      const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
      EndpointCallback callback);

  // Helper method to send stop tracking request to a server.
  void SendStopTrackingRequestToServer(
      ParcelRequestType request_type,
      const GURL& server_url,
      const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
      StopParcelTrackingCallback callback);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  base::WeakPtrFactory<ParcelsServerProxy> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PARCEL_PARCELS_SERVER_PROXY_H_
