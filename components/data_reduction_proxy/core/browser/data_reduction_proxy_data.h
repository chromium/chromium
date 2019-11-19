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
#include "net/base/network_change_notifier.h"
#include "net/nqe/effective_connection_type.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

// DataReductionProxy-related data that can be put into UserData or other
// storage vehicles to associate this data with the object that owns it.
class DataReductionProxyData : public base::SupportsUserData::Data {
 public:
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

  // Whether a proxy-served lite page response was seen for the HTTP request or
  // navigation.
  bool lite_page_received_;

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

  DISALLOW_ASSIGN(DataReductionProxyData);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_DATA_H_
