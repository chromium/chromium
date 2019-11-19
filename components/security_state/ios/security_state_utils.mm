// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/ios/security_state_utils.h"

#include <memory>

#include "components/security_state/core/security_state.h"
#include "components/security_state/ios/ssl_status_input_event_data.h"
#import "ios/web/common/origin_util.h"
#include "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/security/security_style.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/web_state.h"
#include "url/origin.h"

namespace security_state {

std::unique_ptr<security_state::VisibleSecurityState>
GetVisibleSecurityStateForWebState(const web::WebState* web_state) {
  auto state = std::make_unique<security_state::VisibleSecurityState>();

  const web::NavigationItem* item =
      web_state->GetNavigationManager()->GetVisibleItem();
  if (!item || item->GetSSL().security_style == web::SECURITY_STYLE_UNKNOWN)
    return state;

  state->connection_info_initialized = true;
  state->url = item->GetURL();
  const web::SSLStatus& ssl = item->GetSSL();
  state->certificate = ssl.certificate;
  state->cert_status = ssl.cert_status;
  state->displayed_mixed_content =
      (ssl.content_status & web::SSLStatus::DISPLAYED_INSECURE_CONTENT) ? true
                                                                        : false;

  security_state::SSLStatusInputEventData* input_events =
      static_cast<security_state::SSLStatusInputEventData*>(
          ssl.user_data.get());
  if (input_events)
    state->insecure_input_events = *input_events->input_events();

  return state;
}

security_state::SecurityLevel GetSecurityLevelForWebState(
    const web::WebState* web_state) {
  if (!web_state) {
    return security_state::NONE;
  }
  return security_state::GetSecurityLevel(
      *GetVisibleSecurityStateForWebState(web_state),
      false /* used policy installed certificate */,
      base::BindRepeating(&web::IsOriginSecure));
}

}  // namespace security_state
