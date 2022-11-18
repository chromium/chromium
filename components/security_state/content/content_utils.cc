// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/content/content_utils.h"

#include <memory>

#include "components/dom_distiller/core/url_constants.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

namespace security_state {

std::unique_ptr<security_state::VisibleSecurityState> GetVisibleSecurityState(
    content::WebContents* web_contents) {
  auto state = std::make_unique<security_state::VisibleSecurityState>();

  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  if (entry->IsInitialEntry())
    return state;
  // Set fields that are not dependent on the connection info.
  state->is_error_page = entry->GetPageType() == content::PAGE_TYPE_ERROR;
  state->is_view_source =
      entry->GetVirtualURL().SchemeIs(content::kViewSourceScheme);
  state->is_devtools =
      entry->GetVirtualURL().SchemeIs(content::kChromeDevToolsScheme);
  state->is_reader_mode =
      entry->GetURL().SchemeIs(dom_distiller::kDomDistillerScheme);
  state->url = entry->GetURL();

  if (!entry->GetSSL().initialized)
    return state;
  state->connection_info_initialized = true;
  const content::SSLStatus& ssl = entry->GetSSL();
  state->certificate = ssl.certificate;
  state->cert_status = ssl.cert_status;
  state->connection_status = ssl.connection_status;
  state->key_exchange_group = ssl.key_exchange_group;
  state->peer_signature_algorithm = ssl.peer_signature_algorithm;
  state->pkp_bypassed = ssl.pkp_bypassed;
  state->displayed_mixed_content =
      !!(ssl.content_status & content::SSLStatus::DISPLAYED_INSECURE_CONTENT);
  state->ran_mixed_content =
      !!(ssl.content_status & content::SSLStatus::RAN_INSECURE_CONTENT);
  state->displayed_content_with_cert_errors =
      !!(ssl.content_status &
         content::SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS);
  state->ran_content_with_cert_errors =
      !!(ssl.content_status & content::SSLStatus::RAN_CONTENT_WITH_CERT_ERRORS);
  state->contained_mixed_form =
      !!(ssl.content_status &
         content::SSLStatus::DISPLAYED_FORM_WITH_INSECURE_ACTION);

  return state;
}

}  // namespace security_state
