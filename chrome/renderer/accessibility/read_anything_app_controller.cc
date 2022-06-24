// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_app_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_update.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-microtask-queue.h"

// static
gin::WrapperInfo ReadAnythingAppController::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
ReadAnythingAppController* ReadAnythingAppController::Install(
    content::RenderFrame* render_frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::MicrotasksScope microtask_scope(isolate,
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame->GetWebFrame()->MainWorldScriptContext();
  if (context.IsEmpty())
    return nullptr;

  v8::Context::Scope context_scope(context);

  ReadAnythingAppController* controller =
      new ReadAnythingAppController(render_frame);
  gin::Handle<ReadAnythingAppController> handle =
      gin::CreateHandle(isolate, controller);
  if (handle.IsEmpty())
    return nullptr;

  v8::Local<v8::Object> chrome =
      content::GetOrCreateChromeObject(isolate, context);
  chrome->Set(context, gin::StringToV8(isolate, "readAnything"), handle.ToV8())
      .Check();
  return controller;
}

ReadAnythingAppController::ReadAnythingAppController(
    content::RenderFrame* render_frame)
    : render_frame_(render_frame) {}

ReadAnythingAppController::~ReadAnythingAppController() = default;

void ReadAnythingAppController::OnAXTreeDistilled(
    const ui::AXTreeUpdate& snapshot,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  content_node_ids_ = content_node_ids;
  tree_ = std::make_unique<ui::AXTree>();

  // Unserialize the snapshot. Failure to unserialize doesn't result in a crash:
  // we control both ends of the serialization-unserialization so any failures
  // are programming error.
  if (!tree_->Unserialize(snapshot))
    NOTREACHED() << tree_->error();

  // TODO(abigailbklein): Use v8::Function rather than javascript. If possible,
  // replace this function call with firing an event.
  std::string script = "chrome.readAnything.updateContent();";
  render_frame_->ExecuteJavaScript(base::ASCIIToUTF16(script));
}

void ReadAnythingAppController::OnFontNameChange(
    const std::string& new_font_name) {
  font_name_ = new_font_name;
  // TODO(abigailbklein): Use v8::Function rather than javascript. If possible,
  // replace this function call with firing an event.
  std::string script = "chrome.readAnything.updateFontName();";
  render_frame_->ExecuteJavaScript(base::ASCIIToUTF16(script));
}

gin::ObjectTemplateBuilder ReadAnythingAppController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<ReadAnythingAppController>::GetObjectTemplateBuilder(
             isolate)
      .SetProperty("contentNodeIds", &ReadAnythingAppController::ContentNodeIds)
      .SetProperty("fontName", &ReadAnythingAppController::FontName)
      .SetMethod("getChildren", &ReadAnythingAppController::GetChildren)
      .SetMethod("getHeadingLevel", &ReadAnythingAppController::GetHeadingLevel)
      .SetMethod("getTextContent", &ReadAnythingAppController::GetTextContent)
      .SetMethod("getUrl", &ReadAnythingAppController::GetUrl)
      .SetMethod("isHeading", &ReadAnythingAppController::IsHeading)
      .SetMethod("isLink", &ReadAnythingAppController::IsLink)
      .SetMethod("isParagraph", &ReadAnythingAppController::IsParagraph)
      .SetMethod("isStaticText", &ReadAnythingAppController::IsStaticText)
      .SetMethod("onConnected", &ReadAnythingAppController::OnConnected);
}

std::vector<ui::AXNodeID> ReadAnythingAppController::ContentNodeIds() {
  return content_node_ids_;
}

std::string ReadAnythingAppController::FontName() {
  return font_name_;
}

std::vector<ui::AXNodeID> ReadAnythingAppController::GetChildren(
    ui::AXNodeID ax_node_id) {
  std::vector<ui::AXNodeID> child_ids;
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  if (!ax_node)
    return std::vector<ui::AXNodeID>();
  for (auto it = ax_node->UnignoredChildrenBegin();
       it != ax_node->UnignoredChildrenEnd(); ++it) {
    child_ids.push_back(it->id());
  }
  return child_ids;
}

uint32_t ReadAnythingAppController::GetHeadingLevel(ui::AXNodeID ax_node_id) {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  if (!ax_node)
    return -1;
  return ax_node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);
}

std::string ReadAnythingAppController::GetTextContent(ui::AXNodeID ax_node_id) {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  if (!ax_node)
    return std::string();
  return ax_node->GetTextContentUTF8();
}

std::string ReadAnythingAppController::GetUrl(ui::AXNodeID ax_node_id) {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  if (!ax_node)
    return std::string();
  return ax_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl);
}

bool ReadAnythingAppController::IsHeading(ui::AXNodeID ax_node_id) {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  if (!ax_node)
    return false;
  return ui::IsHeading(ax_node->GetRole());
}

bool ReadAnythingAppController::IsLink(ui::AXNodeID ax_node_id) {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  if (!ax_node)
    return false;
  return ui::IsLink(ax_node->GetRole());
}

bool ReadAnythingAppController::IsParagraph(ui::AXNodeID ax_node_id) {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  if (!ax_node)
    return false;
  return ax_node->GetRole() == ax::mojom::Role::kParagraph;
}

bool ReadAnythingAppController::IsStaticText(ui::AXNodeID ax_node_id) {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  if (!ax_node)
    return false;
  return ax_node->GetRole() == ax::mojom::Role::kStaticText;
}

void ReadAnythingAppController::OnConnected() {
  mojo::PendingReceiver<read_anything::mojom::PageHandlerFactory>
      page_handler_factory_receiver =
          page_handler_factory_.BindNewPipeAndPassReceiver();
  page_handler_factory_->CreatePageHandler(
      receiver_.BindNewPipeAndPassRemote(),
      page_handler_.BindNewPipeAndPassReceiver());
  render_frame_->GetBrowserInterfaceBroker()->GetInterface(
      std::move(page_handler_factory_receiver));
}

ui::AXNode* ReadAnythingAppController::GetAXNode(ui::AXNodeID ax_node_id) {
  if (!tree_)
    return nullptr;
  return tree_->GetFromId(ax_node_id);
}
