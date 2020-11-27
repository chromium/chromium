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
  const auto& all_frames = web_contents->GetAllFrames();
  const auto& it = std::find_if(
      all_frames.begin(), all_frames.end(), [&](const auto& frame) {
        return frame->GetDevToolsFrameToken().ToString() == frame_id;
      });
  if (it == all_frames.end()) {
    return nullptr;
  }
  return *it;
}

}  // namespace autofill_assistant
