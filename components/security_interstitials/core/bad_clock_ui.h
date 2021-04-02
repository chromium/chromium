// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_BAD_CLOCK_UI_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_BAD_CLOCK_UI_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/ssl_errors/error_classification.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace security_interstitials {

// Provides UI for SSL errors caused by clock misconfigurations.
class BadClockUI {
 public:
  BadClockUI(const GURL& request_url,
             int cert_error,  // Should correspond to a NET_ERROR
             const net::SSLInfo& ssl_info,
             const base::Time& time_triggered,  // Time the error was triggered
             ssl_errors::ClockState clock_state,
             ControllerClient* controller_);
  ~BadClockUI();

  void PopulateStringsForHTML(base::DictionaryValue* load_time_data);
  void HandleCommand(SecurityInterstitialCommand command);

 private:
  void PopulateClockStrings(base::DictionaryValue* load_time_data);

  const GURL request_url_;
  const int cert_error_;
  const net::SSLInfo ssl_info_;
  const base::Time time_triggered_;
  ControllerClient* controller_;
  ssl_errors::ClockState clock_state_;

  DISALLOW_COPY_AND_ASSIGN(BadClockUI);
};

}  // security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_BAD_CLOCK_UI_H_
