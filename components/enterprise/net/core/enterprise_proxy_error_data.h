// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_DATA_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_DATA_H_

#include "url/gurl.h"

namespace enterprise_net {

// Base representation of enterprise proxy error data when an enterprise proxy
// disguised error (e.g. 403, 500, 502, 503, 504 realm challenge) is
// intercepted.
class EnterpriseProxyErrorData {
 public:
  EnterpriseProxyErrorData();
  EnterpriseProxyErrorData(GURL destination_url,
                           GURL proxy_url,
                           int error_code);
  EnterpriseProxyErrorData(const EnterpriseProxyErrorData&);
  EnterpriseProxyErrorData& operator=(const EnterpriseProxyErrorData&);
  EnterpriseProxyErrorData(EnterpriseProxyErrorData&&);
  EnterpriseProxyErrorData& operator=(EnterpriseProxyErrorData&&);
  virtual ~EnterpriseProxyErrorData();

  const GURL& destination_url() const { return destination_url_; }
  const GURL& proxy_url() const { return proxy_url_; }
  int error_code() const { return error_code_; }

 private:
  GURL destination_url_;
  GURL proxy_url_;
  int error_code_ = 0;
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_DATA_H_
