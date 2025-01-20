// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_MODAL_WEB_MODAL_UTILS_H_
#define COMPONENTS_WEB_MODAL_WEB_MODAL_UTILS_H_

#include "components/web_modal/web_modal_export.h"

namespace content {
class WebContents;
}  // namespace content

namespace web_modal {

// Returns true if the web contents has any active web modal dialogs.
bool WEB_MODAL_EXPORT
WebContentsHasActiveWebModal(content::WebContents* web_contents);

}  // namespace web_modal

#endif  // COMPONENTS_WEB_MODAL_WEB_MODAL_UTILS_H_
