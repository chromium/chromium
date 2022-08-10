// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/public/runtime_manager.h"

#include "components/autofill_assistant/browser/public/runtime_manager_impl.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

// static
RuntimeManager* RuntimeManager::GetOrCreateForWebContents(
    content::WebContents* contents) {
  return RuntimeManagerImpl::GetForWebContents(contents);
}

RuntimeManager* RuntimeManager::GetForWebContents(
    content::WebContents* contents) {
  return RuntimeManagerImpl::FromWebContents(contents);
}

}  // namespace autofill_assistant
