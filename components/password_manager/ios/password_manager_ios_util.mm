// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/ios/password_manager_ios_util.h"

#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "components/autofill/ios/browser/autofill_util.h"
#include "components/security_state/ios/security_state_utils.h"
#import "ios/web/public/web_state.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/origin.h"

namespace password_manager {

bool WebStateContentIsSecureHtml(const web::WebState* web_state) {
  if (!web_state) {
    return false;
  }

  if (!web_state->ContentIsHTML()) {
    return false;
  }

  const GURL last_committed_url = web_state->GetLastCommittedURL();

  if (!network::IsUrlPotentiallyTrustworthy(last_committed_url)) {
    return false;
  }

  // If scheme is not cryptographic, the origin must be either localhost or a
  // file.
  if (!security_state::IsSchemeCryptographic(last_committed_url)) {
    return security_state::IsOriginLocalhostOrFile(last_committed_url);
  }

  // If scheme is cryptographic, valid SSL certificate is required.
  security_state::SecurityLevel security_level =
      security_state::GetSecurityLevelForWebState(web_state);
  return security_state::IsSslCertificateValid(security_level);
}

bool JsonStringToFormData(NSString* json_string,
                          autofill::FormData* form_data,
                          const GURL& page_url,
                          const autofill::FieldDataManager& field_data_manager,
                          const std::string& frame_id) {
  std::unique_ptr<base::Value> formValue = autofill::ParseJson(json_string);
  if (!formValue) {
    return false;
  }

  auto* dict = formValue->GetIfDict();
  if (!dict) {
    return false;
  }

  return autofill::ExtractFormData(*dict, false, std::u16string(), page_url,
                                   page_url.DeprecatedGetOriginAsURL(),
                                   field_data_manager, frame_id, form_data);
}

bool IsCrossOriginIframe(web::WebState* web_state,
                         bool frame_is_main_frame,
                         const GURL& frame_security_origin) {
  return !frame_is_main_frame &&
         !url::Origin::Create(web_state->GetLastCommittedURL())
              .IsSameOriginWith(frame_security_origin);
}

}  // namespace password_manager
