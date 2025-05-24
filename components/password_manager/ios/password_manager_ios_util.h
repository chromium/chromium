// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_IOS_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_IOS_UTIL_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "url/gurl.h"
#import "url/origin.h"

namespace web {
class WebState;
}

namespace autofill {
class FormData;
class FieldDataManager;
}

namespace password_manager {

// Checks if |web_state|'s content is a secure HTML. This is done in order to
// ignore API calls from insecure context.
bool WebStateContentIsSecureHtml(const web::WebState* web_state);

// Converts password form data from |json_string| to autofill::FormData.
std::optional<autofill::FormData> JsonStringToFormData(
    NSString* json_string,
    const GURL& page_url,
    const url::Origin& frame_origin,
    const autofill::FieldDataManager& field_data_manager,
    const std::string& frame_id);

// Returns whether an iframe is cross-origin.
bool IsCrossOriginIframe(web::WebState* web_state,
                         bool frame_is_main_frame,
                         const url::Origin& frame_security_origin);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_IOS_UTIL_H_
