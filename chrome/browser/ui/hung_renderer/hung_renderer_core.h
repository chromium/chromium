// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HUNG_RENDERER_HUNG_RENDERER_CORE_H_
#define CHROME_BROWSER_UI_HUNG_RENDERER_HUNG_RENDERER_CORE_H_

#include <string>
#include <vector>


namespace content {
class RenderProcessHost;
class WebContents;
}  // namespace content

// Given a WebContents that is hung, and the RenderProcessHost of a hung
// process, returns the complete list of WebContentses that are hung, in no
// particular order except that the WebContents |hung_web_contents| is first.
std::vector<content::WebContents*> GetHungWebContentsList(
    content::WebContents* hung_web_contents,
    content::RenderProcessHost* hung_process);

// Given a RenderProcessHost of a hung process, and a WebContents that is
// affected by it, returns the title of the WebContents that should be used in
// the "Hung Page" dialog.
std::u16string GetHungWebContentsTitle(
    content::WebContents* affected_web_contents,
    content::RenderProcessHost* hung_process);

#endif  // CHROME_BROWSER_UI_HUNG_RENDERER_HUNG_RENDERER_CORE_H_
