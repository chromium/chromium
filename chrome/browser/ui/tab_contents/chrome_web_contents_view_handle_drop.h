// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_HANDLE_DROP_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_HANDLE_DROP_H_

#include "content/public/browser/web_contents_view_delegate.h"

namespace content {
class WebContents;
struct DropData;
}  // namespace content

// Common code to be called from the implementation of
// WebContentsViewDelegate::OnPerformingDrop() for each platform.
void HandleOnPerformingDrop(
    content::WebContents* web_contents,
    content::DropData drop_data,
    content::WebContentsViewDelegate::DropCompletionCallback callback);

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_HANDLE_DROP_H_
