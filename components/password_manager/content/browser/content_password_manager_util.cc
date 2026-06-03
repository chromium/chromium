// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_util.h"

#include "components/password_manager/content/browser/bad_message.h"
#include "content/public/browser/render_frame_host.h"

namespace password_manager {

bool CheckFrameActiveAndNotPrerendering(content::RenderFrameHost* rfh) {
  if (!bad_message::CheckFrameNotPrerendering(rfh)) {
    return false;
  }
  return rfh->IsActive();
}

}  // namespace password_manager
