// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_CONTENT_PUBLIC_CONTEXT_MENU_DOWNLOAD_H_
#define COMPONENTS_DOWNLOAD_CONTENT_PUBLIC_CONTEXT_MENU_DOWNLOAD_H_

#include <string>

namespace content {
class WebContents;
struct ContextMenuParams;
}  // namespace content

namespace download {

// Starts a download for the given ContextMenuParams.
void CreateContextMenuDownload(content::WebContents* web_contents,
                               const content::ContextMenuParams& params,
                               const std::string& origin,
                               bool is_link);

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_CONTENT_PUBLIC_CONTEXT_MENU_DOWNLOAD_H_
