// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_ERROR_PAGE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_ERROR_PAGE_H_

#include <optional>

#include "components/supervised_user/core/browser/supervised_user_utils.h"

namespace supervised_user {
class Custodian;

int GetBlockMessageID(FilteringBehaviorReason reason, bool single_parent);

int GetInterstitialMessageID(FilteringBehaviorReason reason);

#if BUILDFLAG(IS_ANDROID)
std::string BuildErrorPageHtmlWithoutApprovals(const GURL& url,
                                               const std::string& app_locale);
#endif  // BUILDFLAG(IS_ANDROID)

std::string BuildErrorPageHtmlWithApprovals(
    bool allow_access_requests,
    std::optional<Custodian> custodian,
    std::optional<Custodian> second_custodian,
    FilteringBehaviorReason reason,
    const std::string& app_locale,
    bool already_sent_remote_request,
    bool is_main_frame,
    std::optional<float> ios_font_size_multiplier);

}  //  namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_ERROR_PAGE_H_
