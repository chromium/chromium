// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/page_agent_views.h"

#include <iterator>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/ui_devtools/agent_util.h"
#include "components/ui_devtools/ui_element.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace ui_devtools {

namespace {

void PaintRectVector(
    std::vector<raw_ptr<UIElement, VectorExperimental>> child_elements) {
  for (ui_devtools::UIElement* element : child_elements) {
    if (element->type() == UIElementType::VIEW) {
      element->PaintRect();
    }
    PaintRectVector(element->children());
  }
}

absl::flat_hash_set<std::string> GetSources(UIElement* root) {
  absl::flat_hash_set<std::string> ret;

  for (auto& source : root->GetSources()) {
    ret.insert(base::StrCat(
        {source.path_, "?l=", base::NumberToString(source.line_)}));
  }

  for (ui_devtools::UIElement* child : root->children()) {
    absl::flat_hash_set<std::string> child_sources = GetSources(child);
    ret.insert(std::make_move_iterator(child_sources.begin()),
               std::make_move_iterator(child_sources.end()));
  }

  return ret;
}

void AddFrameResources(
    std::unique_ptr<protocol::Array<protocol::Page::FrameResource>>&
        frame_resources,
    const absl::flat_hash_set<std::string>& all_sources) {
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

PageAgentViews::~PageAgentViews() = default;

protocol::Response PageAgentViews::disable() {
  return protocol::Response::Success();
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

  absl::flat_hash_set<std::string> all_sources =
      GetSources(dom_agent_->element_root());

  AddFrameResources(subresources, all_sources);
  std::unique_ptr<protocol::Page::FrameResourceTree> result =
      protocol::Page::FrameResourceTree::create()
          .setFrame(std::move(frame_object))
          .setResources(std::move(subresources))
          .build();
  *object = std::move(result);
  return protocol::Response::Success();
}

protocol::Response PageAgentViews::getResourceContent(
    const protocol::String& in_frameId,
    const protocol::String& in_url,
    protocol::String* out_content,
    bool* out_base64Encoded) {
  std::vector<std::string_view> split_url = base::SplitStringPieceUsingSubstr(
      in_url, "src/", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (split_url.size() != 2)
    return protocol::Response::ServerError("Invalid URL");

  std::vector<std::string_view> split_path = base::SplitStringPieceUsingSubstr(
      split_url[1], "?l=", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (split_path.size() != 2)
    return protocol::Response::ServerError("Invalid URL");

  if (GetSourceCode(std::string(split_path[0]), out_content)) {
    return protocol::Response::Success();
  }

  return protocol::Response::ServerError("Could not read source file");
}

}  // namespace ui_devtools
