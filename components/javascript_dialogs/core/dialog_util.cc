// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/javascript_dialogs/core/dialog_util.h"

#include "base/i18n/rtl.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

namespace javascript_dialogs::util {

// If an origin is opaque but has a precursor, then returns the precursor
// origin. If the origin is not opaque, returns it unchanged. Unwrapping origins
// allows the dialog code to provide the user with a clearer picture of which
// page is actually showing the dialog.
url::Origin UnwrapOriginIfOpaque(const url::Origin& origin) {
  if (!origin.opaque()) {
    return origin;
  }

  const url::SchemeHostPort& precursor =
      origin.GetTupleOrPrecursorTupleIfOpaque();
  if (!precursor.IsValid()) {
    return origin;
  }

  return url::Origin::CreateFromNormalizedTuple(
      precursor.scheme(), precursor.host(), precursor.port());
}

std::u16string DialogTitle(const url::Origin& main_frame_origin,
                           const url::Origin& alerting_frame_origin) {
  // Note that `Origin::Create()` handles unwrapping of `blob:` and
  // `filesystem:` schemed URLs, so no special handling is needed for that.
  // However, origins can be opaque but have precursors that are origins that a
  // user would be able to make sense of, so do unwrapping for that.
  const url::Origin unwrapped_main_frame_origin =
      UnwrapOriginIfOpaque(main_frame_origin);
  const url::Origin unwrapped_alerting_frame_origin =
      UnwrapOriginIfOpaque(alerting_frame_origin);

  bool is_same_origin_as_main_frame =
      unwrapped_alerting_frame_origin.IsSameOriginWith(
          unwrapped_main_frame_origin);
  if (unwrapped_alerting_frame_origin.GetURL().IsStandard() &&
      !unwrapped_alerting_frame_origin.GetURL().SchemeIsFile()) {
    std::u16string origin_string =
        url_formatter::FormatOriginForSecurityDisplay(
            unwrapped_alerting_frame_origin,
            url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    return l10n_util::GetStringFUTF16(
        is_same_origin_as_main_frame ? IDS_JAVASCRIPT_MESSAGEBOX_TITLE
                                     : IDS_JAVASCRIPT_MESSAGEBOX_TITLE_IFRAME,
        base::i18n::GetDisplayStringInLTRDirectionality(origin_string));
  }
  return l10n_util::GetStringUTF16(
      is_same_origin_as_main_frame
          ? IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL
          : IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL_IFRAME);
}

}  // namespace javascript_dialogs::util
