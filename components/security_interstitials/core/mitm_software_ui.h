// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_MITM_SOFTWARE_UI_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_MITM_SOFTWARE_UI_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/ssl_errors/error_classification.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace security_interstitials {

// Provides UI for SSL errors caused by MITM software misconfigurations.
class MITMSoftwareUI {
 public:
  MITMSoftwareUI(const GURL& request_url,
                 int cert_error,
                 const net::SSLInfo& ssl_info,
                 const std::string& mitm_software_name,
                 bool is_enterprise_managed,
                 ControllerClient* controller_);

  MITMSoftwareUI(const MITMSoftwareUI&) = delete;
  MITMSoftwareUI& operator=(const MITMSoftwareUI&) = delete;

  ~MITMSoftwareUI();

  void PopulateStringsForHTML(base::Value::Dict& load_time_data);
  void HandleCommand(SecurityInterstitialCommand command);

 protected:
  void PopulateEnterpriseUserStringsForHTML(base::Value::Dict& load_time_data);
  void PopulateAtHomeUserStringsForHTML(base::Value::Dict& load_time_data);

 private:
  const GURL request_url_;
  const int cert_error_;
  const net::SSLInfo ssl_info_;
  const std::string mitm_software_name_;
  const bool is_enterprise_managed_;
  raw_ptr<ControllerClient> controller_;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_MITM_SOFTWARE_UI_H_
