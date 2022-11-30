// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_FORM_META_DATA_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_FORM_META_DATA_H_

#include "components/autofill/core/common/form_data.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
}

namespace password_manager {

// Returns the last committed URL unless this is an `about:` URL, in which case
// it the last committed origin is returned.
GURL GetURLFromRenderFrameHost(content::RenderFrameHost* rfh);

// Sets the members of |form| that are not sent via mojo: FrameData::host_frame,
// FormData::url, FormData::full_url, FormData::main_frame_origin
void SetFrameAndFormMetaData(content::RenderFrameHost* rfh,
                             autofill::FormData& form);

// Returns a copy of |form| with the meta data set as per
// SetFrameAndFormMetaData().
autofill::FormData GetFormWithFrameAndFormMetaData(
    content::RenderFrameHost* rfh,
    autofill::FormData form);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_FORM_META_DATA_H_
