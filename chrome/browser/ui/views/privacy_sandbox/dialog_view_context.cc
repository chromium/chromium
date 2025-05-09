// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/privacy_sandbox/dialog_view_context.h"

#include "content/public/browser/web_contents.h"

namespace privacy_sandbox {

DialogViewContext::DialogViewContext(content::WebContents* contents,
                                     BaseDialogUIDelegate& delegate)
    : content::WebContentsUserData<DialogViewContext>(*contents),
      delegate_(delegate) {}

DialogViewContext::~DialogViewContext() = default;

BaseDialogUIDelegate& DialogViewContext::GetDelegate() {
  return *delegate_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DialogViewContext);

}  // namespace privacy_sandbox
