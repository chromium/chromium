// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CHOOSER_TITLE_UTIL_H_
#define COMPONENTS_PERMISSIONS_CHOOSER_TITLE_UTIL_H_

#include <string>

namespace content {
class RenderFrameHost;
}

namespace permissions {

// Creates a title for a chooser using the origin of the main frame
// containing `render_frame_host`. Returns the empty string if
// `render_frame_host` is null.
std::u16string CreateChooserTitle(content::RenderFrameHost* render_frame_host,
                                  int title_string_id_origin);

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CHOOSER_TITLE_UTIL_H_
