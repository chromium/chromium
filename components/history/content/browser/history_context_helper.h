// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CONTENT_BROWSER_HISTORY_CONTEXT_HELPER_H_
#define COMPONENTS_HISTORY_CONTENT_BROWSER_HISTORY_CONTEXT_HELPER_H_

#include "components/history/core/browser/history_context.h"

namespace content {
class WebContents;
}  // namespace content

namespace history {

// Helper function that associates a ContextID to a content::WebContents. The
// ContextID will become invalid once the content::WebContents is destroyed.
ContextID ContextIDForWebContents(content::WebContents* web_contents);

}  // namespace history

#endif  // COMPONENTS_HISTORY_CONTENT_BROWSER_HISTORY_CONTEXT_HELPER_H_
