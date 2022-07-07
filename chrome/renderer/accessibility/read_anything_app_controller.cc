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
#include "gin/converter.h"
#include "gin/dictionary.h"
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

namespace {

// The following methods convert v8::Value types to an AXTreeUpdate. This is not
// a complete conversion (thus way gin::Converter<ui::AXTreeUpdate> is not used
// or implemented) but just converting the bare minimum data types needed for
// the ReadAnythingAppTest.

void SetAXNodeDataChildIds(v8::Isolate* isolate,
                           gin::Dictionary* v8_dict,
                           ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_child_ids;
  v8_dict->Get("childIds", &v8_child_ids);
  gin::Converter<std::vector<int32_t>>::FromV8(isolate, v8_child_ids,
                                               &ax_node_data->child_ids);
}

void SetAXNodeDataHierarchicalLevel(v8::Isolate* isolate,
                                    gin::Dictionary* v8_dict,
                                    ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_hierarchical_level;
  v8_dict->Get("hierarchicalLevel", &v8_hierarchical_level);
  int32_t hierarchical_level;
  gin::Converter<int32_t>::FromV8(isolate, v8_hierarchical_level,
                                  &hierarchical_level);
  ax_node_data->AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel,
                                hierarchical_level);
}

void SetAXNodeDataId(v8::Isolate* isolate,
                     gin::Dictionary* v8_dict,
                     ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_id;
  v8_dict->Get("id", &v8_id);
  gin::Converter<int32_t>::FromV8(isolate, v8_id, &ax_node_data->id);
}

void SetAXNodeDataName(v8::Isolate* isolate,
                       gin::Dictionary* v8_dict,
                       ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_name;
  v8_dict->Get("name", &v8_name);
  std::string name;
  gin::Converter<std::string>::FromV8(isolate, v8_name, &name);
  ax_node_data->SetName(name);
  ax_node_data->SetNameFrom(ax::mojom::NameFrom::kContents);
}

void SetAXNodeDataRole(v8::Isolate* isolate,
                       gin::Dictionary* v8_dict,
                       ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_role;
  v8_dict->Get("role", &v8_role);
  std::string role_name;
  gin::Converter<std::string>::FromV8(isolate, v8_role, &role_name);
  if (role_name == "rootWebArea")
    ax_node_data->role = ax::mojom::Role::kRootWebArea;
  else if (role_name == "heading")
    ax_node_data->role = ax::mojom::Role::kHeading;
  else if (role_name == "link")
    ax_node_data->role = ax::mojom::Role::kLink;
  else if (role_name == "paragraph")
    ax_node_data->role = ax::mojom::Role::kParagraph;
  else if (role_name == "staticText")
    ax_node_data->role = ax::mojom::Role::kStaticText;
}

void SetAXNodeDataUrl(v8::Isolate* isolate,
                      gin::Dictionary* v8_dict,
                      ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_url;
  v8_dict->Get("url", &v8_url);
  std::string url;
  gin::Converter<std::string>::FromV8(isolate, v8_url, &url);
  ax_node_data->AddStringAttribute(ax::mojom::StringAttribute::kUrl, url);
}

void SetAXTreeUpdateRootId(v8::Isolate* isolate,
                           gin::Dictionary* v8_dict,
                           ui::AXTreeUpdate* snapshot) {
  v8::Local<v8::Value> v8_root_id;
  v8_dict->Get("rootId", &v8_root_id);
  gin::Converter<int32_t>::FromV8(isolate, v8_root_id, &snapshot->root_id);
}

ui::AXTreeUpdate GetSnapshotFromV8SnapshotLite(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_snapshot_lite) {
  ui::AXTreeUpdate snapshot;
  gin::Dictionary v8_snapshot_dict(
      isolate, v8::Local<v8::Object>::Cast(v8_snapshot_lite));
  SetAXTreeUpdateRootId(isolate, &v8_snapshot_dict, &snapshot);

  v8::Local<v8::Value> v8_nodes;
  v8_snapshot_dict.Get("nodes", &v8_nodes);
  std::vector<v8::Local<v8::Value>> v8_nodes_vector;
  gin::Converter<std::vector<v8::Local<v8::Value>>>::FromV8(isolate, v8_nodes,
                                                            &v8_nodes_vector);

  for (v8::Local<v8::Value> v8_node : v8_nodes_vector) {
    ui::AXNodeData ax_node_data;
    gin::Dictionary v8_node_dict(isolate, v8::Local<v8::Object>::Cast(v8_node));
    SetAXNodeDataId(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataName(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataChildIds(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataRole(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataHierarchicalLevel(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataUrl(isolate, &v8_node_dict, &ax_node_data);
    snapshot.nodes.push_back(ax_node_data);
  }
  return snapshot;
}

}  // namespace

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

void ReadAnythingAppController::OnFontSizeChanged(const float new_font_size) {
  font_size_ = new_font_size;

  // TODO: Use v*::Function rather than javascript.
  std::string script = "chrome.readAnything.updateFontSize();";
  render_frame_->ExecuteJavaScript(base::ASCIIToUTF16(script));
}

gin::ObjectTemplateBuilder ReadAnythingAppController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<ReadAnythingAppController>::GetObjectTemplateBuilder(
             isolate)
      .SetProperty("contentNodeIds", &ReadAnythingAppController::ContentNodeIds)
      .SetProperty("fontName", &ReadAnythingAppController::FontName)
      .SetProperty("fontSize", &ReadAnythingAppController::FontSize)
      .SetMethod("getChildren", &ReadAnythingAppController::GetChildren)
      .SetMethod("getHeadingLevel", &ReadAnythingAppController::GetHeadingLevel)
      .SetMethod("getTextContent", &ReadAnythingAppController::GetTextContent)
      .SetMethod("getUrl", &ReadAnythingAppController::GetUrl)
      .SetMethod("isHeading", &ReadAnythingAppController::IsHeading)
      .SetMethod("isLink", &ReadAnythingAppController::IsLink)
      .SetMethod("isParagraph", &ReadAnythingAppController::IsParagraph)
      .SetMethod("isStaticText", &ReadAnythingAppController::IsStaticText)
      .SetMethod("onConnected", &ReadAnythingAppController::OnConnected)
      .SetMethod("setContentForTesting",
                 &ReadAnythingAppController::SetContentForTesting)
      .SetMethod("setFontNameForTesting",
                 &ReadAnythingAppController::SetFontNameForTesting);
}

std::vector<ui::AXNodeID> ReadAnythingAppController::ContentNodeIds() {
  return content_node_ids_;
}

std::string ReadAnythingAppController::FontName() {
  return font_name_;
}

float ReadAnythingAppController::FontSize() {
  return font_size_;
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

void ReadAnythingAppController::SetFontNameForTesting(
    std::string new_font_name) {
  OnFontNameChange(new_font_name);
}

void ReadAnythingAppController::SetContentForTesting(
    v8::Local<v8::Value> v8_snapshot_lite,
    std::vector<ui::AXNodeID> content_node_ids) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  ui::AXTreeUpdate snapshot =
      GetSnapshotFromV8SnapshotLite(isolate, v8_snapshot_lite);
  OnAXTreeDistilled(snapshot, content_node_ids);
}

ui::AXNode* ReadAnythingAppController::GetAXNode(ui::AXNodeID ax_node_id) {
  if (!tree_)
    return nullptr;
  return tree_->GetFromId(ax_node_id);
}
