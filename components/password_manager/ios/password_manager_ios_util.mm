// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_manager_ios_util.h"

#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/security_state/ios/security_state_utils.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/is_potentially_trustworthy.h"
#import "url/origin.h"

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

std::optional<autofill::FormData> JsonStringToFormData(
    NSString* json_string,
    const GURL& page_url,
    const url::Origin& frame_origin,
    const autofill::FieldDataManager& field_data_manager,
    const std::string& frame_id) {
  std::unique_ptr<base::Value> formValue = autofill::ParseJson(json_string);
  if (!formValue) {
    return std::nullopt;
  }

  auto* dict = formValue->GetIfDict();
  if (!dict) {
    return std::nullopt;
  }

  return autofill::ExtractFormData(*dict, false, std::u16string(), page_url,
                                   frame_origin, field_data_manager, frame_id);
}

bool IsCrossOriginIframe(web::WebState* web_state,
                         bool frame_is_main_frame,
                         const url::Origin& frame_security_origin) {
  return !frame_is_main_frame &&
         !url::Origin::Create(web_state->GetLastCommittedURL())
              .IsSameOriginWith(frame_security_origin);
}

}  // namespace password_manager
