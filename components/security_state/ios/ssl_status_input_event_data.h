// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_STATE_IOS_SSL_STATUS_INPUT_EVENT_DATA_H_
#define COMPONENTS_SECURITY_STATE_IOS_SSL_STATUS_INPUT_EVENT_DATA_H_

#include <memory>

#include "components/security_state/core/insecure_input_event_data.h"
#include "ios/web/public/security/ssl_status.h"

namespace security_state {

using web::SSLStatus;

// Stores flags about Input Events in the SSLStatus UserData to
// influence the Security Level of the page.
class SSLStatusInputEventData : public SSLStatus::UserData {
 public:
  SSLStatusInputEventData();
  explicit SSLStatusInputEventData(
      const security_state::InsecureInputEventData& initial_data);
  ~SSLStatusInputEventData() override;

  security_state::InsecureInputEventData* input_events();

  // SSLStatus::UserData:
  std::unique_ptr<SSLStatus::UserData> Clone() override;

 private:
  security_state::InsecureInputEventData data_;

  DISALLOW_COPY_AND_ASSIGN(SSLStatusInputEventData);
};

}  // namespace security_state

#endif  // COMPONENTS_SECURITY_STATE_IOS_SSL_STATUS_INPUT_EVENT_DATA_H_
