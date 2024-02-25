// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_ERROR_PAGE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_ERROR_PAGE_H_

#include <string>

#include "components/supervised_user/core/browser/supervised_user_utils.h"

namespace supervised_user {

int GetBlockMessageID(FilteringBehaviorReason reason, bool single_parent);

std::string BuildErrorPageHtml(bool allow_access_requests,
                               const std::string& profile_image_url,
                               const std::string& profile_image_url2,
                               const std::string& custodian,
                               const std::string& custodian_email,
                               const std::string& second_custodian,
                               const std::string& second_custodian_email,
                               FilteringBehaviorReason reason,
                               const std::string& app_locale,
                               bool already_sent_remote_request,
                               bool is_main_frame,
                               bool show_banner);

}  //  namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_ERROR_PAGE_H_
