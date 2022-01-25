// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_url_utils.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "net/base/url_util.h"

namespace {

const char kIsModalParamKey[] = "is_modal";
const char kDesignParamKey[] = "design";
const char kProfileColorParamKey[] = "profile_color";

bool StringToDesignVersion(base::StringPiece input,
                           SyncConfirmationUI::DesignVersion* output) {
  int int_value;
  if (!base::StringToInt(input, &int_value))
    return false;

  SyncConfirmationUI::DesignVersion value =
      static_cast<SyncConfirmationUI::DesignVersion>(int_value);
  // Make sure that `value` is a valid `DesignVersion`.
  switch (value) {
    case SyncConfirmationUI::DesignVersion::kColored:
    case SyncConfirmationUI::DesignVersion::kMonotone:
      *output = value;
      return true;
      // No default. Please update the switch statement when adding a new
      // enumerator.
  }

  return false;
}

}  // namespace

SyncConfirmationURLParams GetParamsFromSyncConfirmationURL(const GURL& url) {
  // Use defaults provided by `SyncConfirmationURLParams` for parameters that
  // fail to parse.
  SyncConfirmationURLParams params;

  std::string is_modal_str;
  int is_modal;
  if (net::GetValueForKeyInQuery(url, kIsModalParamKey, &is_modal_str) &&
      base::StringToInt(is_modal_str, &is_modal)) {
    params.is_modal = is_modal;
  }

  std::string design_str;
  SyncConfirmationUI::DesignVersion design;
  if (net::GetValueForKeyInQuery(url, kDesignParamKey, &design_str) &&
      StringToDesignVersion(design_str, &design)) {
    params.design = design;
  }

  std::string profile_color_str;
  SkColor profile_color;
  if (net::GetValueForKeyInQuery(url, kProfileColorParamKey,
                                 &profile_color_str) &&
      base::StringToUint(profile_color_str, &profile_color)) {
    params.profile_color = profile_color;
  }

  return params;
}

GURL AppendSyncConfirmationQueryParams(
    const GURL& url,
    const SyncConfirmationURLParams& params) {
  GURL url_with_params = net::AppendQueryParameter(
      url, kIsModalParamKey, base::NumberToString(params.is_modal));
  url_with_params = net::AppendQueryParameter(
      url_with_params, kDesignParamKey,
      base::NumberToString(static_cast<int>(params.design)));
  if (params.profile_color) {
    url_with_params =
        net::AppendQueryParameter(url_with_params, kProfileColorParamKey,
                                  base::NumberToString(*params.profile_color));
  }
  return url_with_params;
}
