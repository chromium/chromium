// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/privacy_sandbox/dialog_origin_marker.h"

#include "content/public/browser/web_contents.h"

namespace privacy_sandbox {

DialogOriginMarker::DialogOriginMarker(content::WebContents* contents,
                                       BaseDialogUIDelegate& delegate)
    : content::WebContentsUserData<DialogOriginMarker>(*contents),
      delegate_(delegate) {}

DialogOriginMarker::~DialogOriginMarker() = default;

BaseDialogUIDelegate& DialogOriginMarker::GetDelegate() {
  return *delegate_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DialogOriginMarker);

}  // namespace privacy_sandbox
