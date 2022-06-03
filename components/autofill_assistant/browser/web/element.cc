// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/element.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

DomObjectFrameStack::DomObjectFrameStack() = default;

DomObjectFrameStack::~DomObjectFrameStack() = default;

DomObjectFrameStack::DomObjectFrameStack(const DomObjectFrameStack&) = default;

content::RenderFrameHost* FindCorrespondingRenderFrameHost(
    const std::string& frame_id,
    content::WebContents* web_contents) {
  if (frame_id.empty()) {
    return web_contents->GetMainFrame();
  }
  content::RenderFrameHost* result = nullptr;
  web_contents->GetMainFrame()->ForEachRenderFrameHost(base::BindRepeating(
      [](const std::string& frame_id, content::RenderFrameHost** result,
         content::RenderFrameHost* render_frame_host) {
        if (render_frame_host->GetDevToolsFrameToken().ToString() == frame_id) {
          *result = render_frame_host;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      },
      frame_id, &result));
  return result;
}

}  // namespace autofill_assistant
