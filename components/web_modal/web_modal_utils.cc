// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_modal/web_modal_utils.h"

#include "components/web_modal/web_contents_modal_dialog_manager.h"

namespace web_modal {

bool WebContentsHasActiveWebModal(content::WebContents* web_contents) {
  auto* manager = WebContentsModalDialogManager::FromWebContents(web_contents);
  return manager && manager->IsDialogActive();
}

}  // namespace web_modal
