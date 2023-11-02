// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/chooser_title_util.h"

#include "components/url_formatter/elide_url.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace permissions {

std::u16string CreateChooserTitle(content::RenderFrameHost* render_frame_host,
                                  int title_string_id_origin) {
  if (!render_frame_host)
    return u"";
  return l10n_util::GetStringFUTF16(
      title_string_id_origin,
      url_formatter::FormatOriginForSecurityDisplay(
          render_frame_host->GetMainFrame()->GetLastCommittedOrigin(),
          url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
}

}  // namespace permissions
