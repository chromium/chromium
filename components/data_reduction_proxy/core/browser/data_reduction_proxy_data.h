// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_DATA_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_DATA_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "net/base/network_change_notifier.h"
#include "net/nqe/effective_connection_type.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}

namespace data_reduction_proxy {

// DataReductionProxy-related data that can be put into UserData or other
// storage vehicles to associate this data with the object that owns it.
class DataReductionProxyData : public base::SupportsUserData::Data {
 public:
  // Holds connection timing data for each network request while loading the
  // associated resource.
  struct RequestInfo {
    enum Protocol { HTTP, HTTPS, QUIC, UNKNOWN };

    RequestInfo(Protocol protocol,
                bool proxy_fallback,
                base::TimeDelta dns_time,
                base::TimeDelta connect_time,
                base::TimeDelta http_time);

    RequestInfo(const RequestInfo& other);

    const Protocol protocol;

    // If this request caused the proxy to fallback.
    const bool proxy_bypass;

    // See https://www.w3.org/TR/resource-timing/ for definitions.
    const base::TimeDelta dns_time;
    const base::TimeDelta connect_time;
    const base::TimeDelta http_time;

    // Used for testing.
    bool operator==(const RequestInfo& other) const {
      return protocol == other.protocol && proxy_bypass == other.proxy_bypass &&
             dns_time == other.dns_time && connect_time == other.connect_time &&
             http_time == other.http_time;
    }
  };

  DataReductionProxyData();
  ~DataReductionProxyData() override;

  // Allow copying.
  DataReductionProxyData(const DataReductionProxyData& other);

  // Whether the DataReductionProxy was used for this request or navigation.
  // Also true if the user is the holdback experiment, and the request would
  // otherwise be eligible to use the proxy.
  bool used_data_reduction_proxy() const { return used_data_reduction_proxy_; }
  void set_used_data_reduction_proxy(bool used_data_reduction_proxy) {
    used_data_reduction_proxy_ = used_data_reduction_proxy;
  }

  // Whether a lite page response was seen for the request or navigation.
  bool lite_page_received() const { return lite_page_received_; }
  void set_lite_page_received(bool lite_page_received) {
    lite_page_received_ = lite_page_received;
  }

  // Whether a Lo-Fi (or empty-image) page policy directive was received for
  // the navigation.
  bool lofi_policy_received() const { return lofi_policy_received_; }
  void set_lofi_policy_received(bool lofi_policy_received) {
    lofi_policy_received_ = lofi_policy_received;
  }

  // Whether a server Lo-Fi page response was seen for the request or
  // navigation.
  bool lofi_received() const { return lofi_received_; }
  void set_lofi_received(bool lofi_received) { lofi_received_ = lofi_received; }

  // Whether client Lo-Fi was requested for this request. This is only set on
  // image requests that have added a range header to attempt to get a smaller
  // file size image.
  bool client_lofi_requested() const { return client_lofi_requested_; }
  void set_client_lofi_requested(bool client_lofi_requested) {
    client_lofi_requested_ = client_lofi_requested;
  }

  // This response was fetched from cache, but the original request used DRP.
  bool was_cached_data_reduction_proxy_response() const {
    return was_cached_data_reduction_proxy_response_;
  }
  void set_was_cached_data_reduction_proxy_response(
      bool was_cached_data_reduction_proxy_response) {
    was_cached_data_reduction_proxy_response_ =
        was_cached_data_reduction_proxy_response;
  }

  // The session key used for this request. Only set for main frame requests.
  std::string session_key() const { return session_key_; }
  void set_session_key(const std::string& session_key) {
    session_key_ = session_key;
  }

  // The URL the frame is navigating to. This may change during the navigation
  // when encountering a server redirect. Only set for main frame requests.
  GURL request_url() const { return request_url_; }
  void set_request_url(const GURL& request_url) { request_url_ = request_url; }

  // The EffectiveConnectionType after the proxy is resolved. This is set for
  // main frame requests only.
  net::EffectiveConnectionType effective_connection_type() const {
    return effective_connection_type_;
  }
  void set_effective_connection_type(
      const net::EffectiveConnectionType& effective_connection_type) {
    effective_connection_type_ = effective_connection_type;
  }

  // The connection type (Wifi, 2G, 3G, 4G, None, etc) as reported by the
  // NetworkChangeNotifier. Only set for main frame requests.
  net::NetworkChangeNotifier::ConnectionType connection_type() const {
    return connection_type_;
  }
  void set_connection_type(
      const net::NetworkChangeNotifier::ConnectionType connection_type) {
    connection_type_ = connection_type;
  }

  // An identifier that is guaranteed to be unique to each page load during a
  // data saver session. Only present on main frame requests.
  const base::Optional<uint64_t>& page_id() const { return page_id_; }
  void set_page_id(uint64_t page_id) { page_id_ = page_id; }

  // Whether the blacklist prevented a preview.
  bool black_listed() const { return black_listed_; }
  void set_black_listed(bool black_listed) { black_listed_ = black_listed; }

  // Holds connection timing data for each redirect while loading the associated
  // resource.
  std::vector<RequestInfo> request_info() const { return request_info_; }
  void set_request_info(std::vector<RequestInfo> request_info) {
    request_info_ = std::move(request_info);
  }
  // Adds an additional |RequestInfo| to the end of the list.
  void add_request_info(const RequestInfo& info) {
    request_info_.push_back(info);
  }

  // Passes ownership of |request_info_| to the caller so it can be preserved
  // when |this| is deleted.
  std::vector<RequestInfo> TakeRequestInfo();

  // Removes |this| from |request|.
  static void ClearData(net::URLRequest* request);

  // Returns the Data from the URLRequest's UserData.
  static DataReductionProxyData* GetData(const net::URLRequest& request);
  // Returns the Data for a given URLRequest. If there is currently no
  // DataReductionProxyData on URLRequest, it creates one, and adds it to the
  // URLRequest's UserData, and returns a raw pointer to the new instance.
  static DataReductionProxyData* GetDataAndCreateIfNecessary(
      net::URLRequest* request);

  // Given a URLRequest, pull out the necessary timing information and returns a
  // fully populated |RequestInfo| struct.
  static std::unique_ptr<RequestInfo> CreateRequestInfoFromRequest(
      net::URLRequest* request,
      bool did_bypass_proxy);

  // Create a brand new instance of DataReductionProxyData that could be used in
  // a different thread. Several of deep copies may occur per navigation, so
  // this is inexpensive.
  std::unique_ptr<DataReductionProxyData> DeepCopy() const;

 private:
  // Whether the DataReductionProxy was used for this request or navigation.
  // Also true if the user is the holdback experiment, and the request would
  // otherwise be eligible to use the proxy.
  // Cached responses are not considered to have used DRP.
  bool used_data_reduction_proxy_;

  // Whether client Lo-Fi was requested for this request. This is only set on
  // image requests that have added a range header to attempt to get a smaller
  // file size image.
  bool client_lofi_requested_;

  // Whether a lite page response was seen for the request or navigation.
  bool lite_page_received_;

  // Whether server Lo-Fi directive was received for this navigation. True if
  // the proxy returns the empty-image page-policy for the main frame response.
  bool lofi_policy_received_;

  // Whether a lite page response was seen for the request or navigation.
  bool lofi_received_;

  // Whether the blacklist prevented a preview.
  bool black_listed_;

  // This response was fetched from cache, but the original request used DRP.
  bool was_cached_data_reduction_proxy_response_;

  // The session key used for this request or navigation.
  std::string session_key_;

  // The URL the frame is navigating to. This may change during the navigation
  // when encountering a server redirect.
  GURL request_url_;

  // The EffectiveConnectionType when the request or navigation starts. This is
  // set for main frame requests only.
  net::EffectiveConnectionType effective_connection_type_;

  // The connection type (Wifi, 2G, 3G, 4G, None, etc) as reported by the
  // NetworkChangeNotifier. Only set for main frame requests.
  net::NetworkChangeNotifier::ConnectionType connection_type_;

  // An identifier that is guaranteed to be unique to each page load during a
  // data saver session. Only present on main frame requests.
  base::Optional<uint64_t> page_id_;

  // Lists the connection timing data for each network request while loading the
  // main frame html. Used in PLM pingbacks.
  std::vector<RequestInfo> request_info_;

  DISALLOW_ASSIGN(DataReductionProxyData);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_DATA_H_
