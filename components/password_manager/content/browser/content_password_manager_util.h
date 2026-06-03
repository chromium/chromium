// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_UTIL_H_

namespace content {
class RenderFrameHost;
}

namespace password_manager {

// Returns true if the frame is active and not prerendering.
// WARNING: This method will terminate the renderer process if the frame is
// prerendering.
bool CheckFrameActiveAndNotPrerendering(content::RenderFrameHost* rfh);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_UTIL_H_
