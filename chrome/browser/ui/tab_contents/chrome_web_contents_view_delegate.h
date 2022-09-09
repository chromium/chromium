// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_H_

#include <memory>

namespace content {
class WebContents;
class WebContentsViewDelegate;
}

std::unique_ptr<content::WebContentsViewDelegate> CreateWebContentsViewDelegate(
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_H_
