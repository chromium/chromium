// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/page_agent_views.h"

#include <unordered_set>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "components/ui_devtools/agent_util.h"
#include "components/ui_devtools/ui_element.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/views_switches.h"

namespace ui_devtools {

namespace {

void PaintRectVector(std::vector<UIElement*> child_elements) {
  for (auto* element : child_elements) {
    if (element->type() == UIElementType::VIEW) {
      element->PaintRect();
    }
    PaintRectVector(element->children());
  }
}

std::unordered_set<std::string> GetSources(UIElement* root) {
  std::unordered_set<std::string> ret;

  for (auto& source : root->GetSources()) {
    ret.insert(source.path_ + "?l=" + base::NumberToString(source.line_));
  }

  for (auto* child : root->children()) {
    for (auto& child_source : GetSources(child)) {
      ret.insert(child_source);
    }
  }

  return ret;
}

void AddFrameResources(
    std::unique_ptr<protocol::Array<protocol::Page::FrameResource>>&
        frame_resources,
    const std::unordered_set<std::string>& all_sources) {
  for (const auto& source : all_sources) {
    frame_resources->emplace_back(
        protocol::Page::FrameResource::create()
            .setUrl(kChromiumCodeSearchSrcURL + source)
            .setType("Document")
            .setMimeType("text/x-c++hdr")
            .build());
  }
}

}  // namespace

PageAgentViews::PageAgentViews(DOMAgent* dom_agent) : PageAgent(dom_agent) {}

PageAgentViews::~PageAgentViews() {}

protocol::Response PageAgentViews::disable() {
  // Set bubble lock flag back to false.
  views::BubbleDialogDelegateView::devtools_dismiss_override_ = false;

  // Remove debug bounds rects if enabled.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          views::switches::kDrawViewBoundsRects)) {
    base::CommandLine::ForCurrentProcess()->InitFromArgv(
        base::CommandLine::ForCurrentProcess()->argv());
    PaintRectVector(dom_agent_->element_root()->children());
  }
  return protocol::Response::OK();
}

protocol::Response PageAgentViews::reload(protocol::Maybe<bool> bypass_cache) {
  if (!bypass_cache.isJust())
    return protocol::Response::OK();

  bool shift_pressed = bypass_cache.fromMaybe(false);

  // Ctrl+Shift+R called to toggle bubble lock.
  if (shift_pressed) {
    views::BubbleDialogDelegateView::devtools_dismiss_override_ =
        !views::BubbleDialogDelegateView::devtools_dismiss_override_;
  } else {
    // Ctrl+R called to toggle debug bounds rectangles.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            views::switches::kDrawViewBoundsRects)) {
      base::CommandLine::ForCurrentProcess()->InitFromArgv(
          base::CommandLine::ForCurrentProcess()->argv());
    } else {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          views::switches::kDrawViewBoundsRects);
    }
    PaintRectVector(dom_agent_->element_root()->children());
  }
  return protocol::Response::OK();
}

protocol::Response PageAgentViews::getResourceTree(
    std::unique_ptr<protocol::Page::FrameResourceTree>* object) {
  std::unique_ptr<protocol::Page::Frame> frame_object =
      protocol::Page::Frame::create()
          .setId("1")
          .setUrl(kChromiumCodeSearchURL)
          .build();
  auto subresources =
      std::make_unique<protocol::Array<protocol::Page::FrameResource>>();

  // Ensure that the DOM tree has been initialized, so all sources have
  // been added.
  if (dom_agent_->element_root() == nullptr) {
    std::unique_ptr<protocol::DOM::Node> node;
    dom_agent_->getDocument(&node);
  }

  std::unordered_set<std::string> all_sources =
      GetSources(dom_agent_->element_root());

  AddFrameResources(subresources, all_sources);
  std::unique_ptr<protocol::Page::FrameResourceTree> result =
      protocol::Page::FrameResourceTree::create()
          .setFrame(std::move(frame_object))
          .setResources(std::move(subresources))
          .build();
  *object = std::move(result);
  return protocol::Response::OK();
}

protocol::Response PageAgentViews::getResourceContent(
    const protocol::String& in_frameId,
    const protocol::String& in_url,
    protocol::String* out_content,
    bool* out_base64Encoded) {
  auto split_url = base::SplitStringUsingSubstr(
      in_url, "src/", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (split_url.size() != 2)
    return protocol::Response::Error("Invalid URL");

  auto split_path = base::SplitStringUsingSubstr(
      split_url[1], "?l=", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (split_path.size() != 2)
    return protocol::Response::Error("Invalid URL");

  if (GetSourceCode(split_path[0], out_content))
    return protocol::Response::OK();
  else
    return protocol::Response::Error("Could not read source file");
}

bool PageAgentViews::devtools_dismiss_override() {
  return views::BubbleDialogDelegateView::devtools_dismiss_override_;
}

}  // namespace ui_devtools
