// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_app_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
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
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_update.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-microtask-queue.h"

using read_anything::mojom::ReadAnythingTheme;
using read_anything::mojom::ReadAnythingThemePtr;

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
  gin::ConvertFromV8(isolate, v8_child_ids, &ax_node_data->child_ids);
}

void SetAXNodeDataId(v8::Isolate* isolate,
                     gin::Dictionary* v8_dict,
                     ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_id;
  v8_dict->Get("id", &v8_id);
  gin::ConvertFromV8(isolate, v8_id, &ax_node_data->id);
}

void SetAXNodeDataLanguage(v8::Isolate* isolate,
                           gin::Dictionary* v8_dict,
                           ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_language;
  v8_dict->Get("language", &v8_language);
  std::string language;
  gin::ConvertFromV8(isolate, v8_language, &language);
  ax_node_data->AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                   language);
}

void SetAXNodeDataName(v8::Isolate* isolate,
                       gin::Dictionary* v8_dict,
                       ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_name;
  v8_dict->Get("name", &v8_name);
  std::string name;
  gin::ConvertFromV8(isolate, v8_name, &name);
  ax_node_data->SetName(name);
  ax_node_data->SetNameFrom(ax::mojom::NameFrom::kContents);
}

void SetAXNodeDataRole(v8::Isolate* isolate,
                       gin::Dictionary* v8_dict,
                       ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_role;
  v8_dict->Get("role", &v8_role);
  std::string role_name;
  gin::ConvertFromV8(isolate, v8_role, &role_name);
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

void SetAXNodeDataHtmlTag(v8::Isolate* isolate,
                          gin::Dictionary* v8_dict,
                          ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_html_tag;
  v8_dict->Get("htmlTag", &v8_html_tag);
  std::string html_tag;
  gin::Converter<std::string>::FromV8(isolate, v8_html_tag, &html_tag);
  ax_node_data->AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag,
                                   html_tag);
}

void SetAXNodeDataTextDirection(v8::Isolate* isolate,
                                gin::Dictionary* v8_dict,
                                ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_direction;
  v8_dict->Get("direction", &v8_direction);
  int direction;
  gin::ConvertFromV8(isolate, v8_direction, &direction);
  ax_node_data->AddIntAttribute(ax::mojom::IntAttribute::kTextDirection,
                                direction);
}

void SetAXNodeDataUrl(v8::Isolate* isolate,
                      gin::Dictionary* v8_dict,
                      ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_url;
  v8_dict->Get("url", &v8_url);
  std::string url;
  gin::ConvertFromV8(isolate, v8_url, &url);
  ax_node_data->AddStringAttribute(ax::mojom::StringAttribute::kUrl, url);
}

void SetSelectionAnchorObjectId(v8::Isolate* isolate,
                                gin::Dictionary* v8_dict,
                                ui::AXTreeData* ax_tree_data) {
  v8::Local<v8::Value> v8_anchor_object_id;
  v8_dict->Get("anchor_object_id", &v8_anchor_object_id);
  gin::ConvertFromV8(isolate, v8_anchor_object_id,
                     &ax_tree_data->sel_anchor_object_id);
}

void SetSelectionFocusObjectId(v8::Isolate* isolate,
                               gin::Dictionary* v8_dict,
                               ui::AXTreeData* ax_tree_data) {
  v8::Local<v8::Value> v8_focus_object_id;
  v8_dict->Get("focus_object_id", &v8_focus_object_id);
  gin::ConvertFromV8(isolate, v8_focus_object_id,
                     &ax_tree_data->sel_focus_object_id);
}

void SetSelectionAnchorOffset(v8::Isolate* isolate,
                              gin::Dictionary* v8_dict,
                              ui::AXTreeData* ax_tree_data) {
  v8::Local<v8::Value> v8_anchor_offset;
  v8_dict->Get("anchor_offset", &v8_anchor_offset);
  gin::ConvertFromV8(isolate, v8_anchor_offset,
                     &ax_tree_data->sel_anchor_offset);
}

void SetSelectionFocusOffset(v8::Isolate* isolate,
                             gin::Dictionary* v8_dict,
                             ui::AXTreeData* ax_tree_data) {
  v8::Local<v8::Value> v8_focus_offset;
  v8_dict->Get("focus_offset", &v8_focus_offset);
  gin::ConvertFromV8(isolate, v8_focus_offset, &ax_tree_data->sel_focus_offset);
}

void SetSelectionIsBackward(v8::Isolate* isolate,
                            gin::Dictionary* v8_dict,
                            ui::AXTreeData* ax_tree_data) {
  v8::Local<v8::Value> v8_sel_is_backward;
  v8_dict->Get("is_backward", &v8_sel_is_backward);
  gin::ConvertFromV8(isolate, v8_sel_is_backward,
                     &ax_tree_data->sel_is_backward);
}

void SetAXTreeUpdateRootId(v8::Isolate* isolate,
                           gin::Dictionary* v8_dict,
                           ui::AXTreeUpdate* snapshot) {
  v8::Local<v8::Value> v8_root_id;
  v8_dict->Get("rootId", &v8_root_id);
  gin::ConvertFromV8(isolate, v8_root_id, &snapshot->root_id);
}

ui::AXTreeUpdate GetSnapshotFromV8SnapshotLite(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_snapshot_lite) {
  ui::AXTreeUpdate snapshot;
  gin::Dictionary v8_snapshot_dict(isolate);
  if (!gin::ConvertFromV8(isolate, v8_snapshot_lite, &v8_snapshot_dict))
    return snapshot;
  SetAXTreeUpdateRootId(isolate, &v8_snapshot_dict, &snapshot);

  v8::Local<v8::Value> v8_nodes;
  v8_snapshot_dict.Get("nodes", &v8_nodes);
  std::vector<v8::Local<v8::Value>> v8_nodes_vector;
  if (!gin::ConvertFromV8(isolate, v8_nodes, &v8_nodes_vector))
    return snapshot;
  for (v8::Local<v8::Value> v8_node : v8_nodes_vector) {
    gin::Dictionary v8_node_dict(isolate);
    if (!gin::ConvertFromV8(isolate, v8_node, &v8_node_dict))
      continue;
    ui::AXNodeData ax_node_data;
    SetAXNodeDataId(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataRole(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataName(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataChildIds(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataHtmlTag(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataLanguage(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataTextDirection(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataUrl(isolate, &v8_node_dict, &ax_node_data);
    snapshot.nodes.push_back(ax_node_data);
  }

  v8::Local<v8::Value> v8_selection;
  v8_snapshot_dict.Get("selection", &v8_selection);
  gin::Dictionary v8_selection_dict(isolate);
  if (!gin::ConvertFromV8(isolate, v8_selection, &v8_selection_dict))
    return snapshot;
  ui::AXTreeData ax_tree_data;
  SetSelectionAnchorObjectId(isolate, &v8_selection_dict, &ax_tree_data);
  SetSelectionFocusObjectId(isolate, &v8_selection_dict, &ax_tree_data);
  SetSelectionAnchorOffset(isolate, &v8_selection_dict, &ax_tree_data);
  SetSelectionFocusOffset(isolate, &v8_selection_dict, &ax_tree_data);
  SetSelectionIsBackward(isolate, &v8_selection_dict, &ax_tree_data);
  snapshot.has_tree_data = true;
  snapshot.tree_data = ax_tree_data;

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
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame->GetWebFrame()->MainWorldScriptContext();
  if (context.IsEmpty())
    return nullptr;
  v8::MicrotasksScope microtask_scope(isolate, context->GetMicrotaskQueue(),
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);

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
  // Reset state.
  display_node_ids_.clear();
  start_node_ = nullptr;
  end_node_ = nullptr;
  start_offset_ = -1;
  end_offset_ = -1;
  content_node_ids_ = content_node_ids;
  tree_ = std::make_unique<ui::AXTree>();

  // Unserialize the snapshot. Failure to unserialize doesn't result in a crash:
  // we control both ends of the serialization-unserialization so any failures
  // are programming error.
  if (!tree_->Unserialize(snapshot))
    NOTREACHED() << tree_->error();

  // Store state about the selection for easy access later. Selection state
  // comes from the tree data rather than AXPosition, as AXPosition requires
  // a valid and registered AXTreeID, which exists only when accessibility is
  // enabled. As Read Anything does not enable accessibility, it is not able to
  // use AXPosition.
  const ui::AXTreeData tree_data = snapshot.tree_data;
  has_selection_ = snapshot.has_tree_data &&
                   tree_data.sel_anchor_object_id != ui::kInvalidAXNodeID &&
                   tree_data.sel_focus_object_id != ui::kInvalidAXNodeID;
  if (!content_node_ids.empty()) {
    // If there are content_node_ids, this means the AXTree was successfully
    // distilled. Post-process in preparation to display the distilled content.
    PostProcessDistillableAXTree();
  } else if (has_selection_) {
    // Otherwise, if there is a selection, post-process the AXTree to display
    // the selected content.
    PostProcessAXTreeWithSelection(tree_data);
  } else {
    // TODO(crbug.com/1266555): Display a UI giving user instructions if the
    // tree was not distillable.
  }

  // TODO(abigailbklein): Use v8::Function rather than javascript. If possible,
  // replace this function call with firing an event.
  std::string script = "chrome.readAnything.updateContent();";
  render_frame_->ExecuteJavaScript(base::ASCIIToUTF16(script));
}

void ReadAnythingAppController::PostProcessAXTreeWithSelection(
    const ui::AXTreeData& tree_data) {
  DCHECK(has_selection_);
  // Identify the start and end nodes and offsets. The start node comes earlier
  // the end node in the tree order.
  ui::AXNode* anchor_node = GetAXNode(tree_data.sel_anchor_object_id);
  DCHECK(anchor_node);
  ui::AXNode* focus_node = GetAXNode(tree_data.sel_focus_object_id);
  DCHECK(focus_node);
  start_node_ = tree_data.sel_is_backward ? focus_node : anchor_node;
  end_node_ = tree_data.sel_is_backward ? anchor_node : focus_node;
  start_offset_ = tree_data.sel_is_backward ? tree_data.sel_focus_offset
                                            : tree_data.sel_anchor_offset;
  end_offset_ = tree_data.sel_is_backward ? tree_data.sel_anchor_offset
                                          : tree_data.sel_focus_offset;

  // If start node or end node is ignored, go to the nearest unignored node
  // within the selection.
  if (start_node_->IsIgnored()) {
    start_node_ = start_node_->GetNextUnignoredInTreeOrder();
    start_offset_ = 0;
  }
  if (end_node_->IsIgnored()) {
    end_node_ = end_node_->GetNextUnignoredInTreeOrder();
    end_offset_ = 0;
  }

  // Display nodes are the nodes which will be displayed by the rendering
  // algorithm of Read Anything app.ts. We wish to create a subtree which
  // stretches from start node to end node with tree root as the root.

  // Add all ancestor ids of start node, including the start node itself. This
  // does a first walk down to start node.
  base::queue<ui::AXNode*> ancestors =
      start_node_->GetAncestorsCrossingTreeBoundaryAsQueue();
  while (!ancestors.empty()) {
    ui::AXNodeID ancestor_id = ancestors.front()->id();
    display_node_ids_.insert(ancestor_id);
    ancestors.pop();
  }

  // Do a pre-order walk of the tree from the start node to the end node and add
  // all nodes to the list of display node ids.
  ui::AXNode* next_node = start_node_;
  DCHECK(!start_node_->IsIgnored());
  DCHECK(!end_node_->IsIgnored());
  while (next_node != end_node_) {
    next_node = next_node->GetNextUnignoredInTreeOrder();
    display_node_ids_.insert(next_node->id());
  }
}

void ReadAnythingAppController::PostProcessDistillableAXTree() {
  DCHECK(!content_node_ids_.empty());

  // Display nodes are the nodes which will be displayed by the rendering
  // algorithm of Read Anything app.ts. We wish to create a subtree which
  // stretches down from tree root to every content node and includes the
  // descendants of each content node.
  for (auto content_node_id : content_node_ids_) {
    ui::AXNode* content_node = GetAXNode(content_node_id);
    DCHECK(content_node);

    // Add all ancestor ids, including the content node itself, which is the
    // first ancestor in the queue. Exit the loop early if an ancestor is
    // already in display_node_ids_; this means that all of the remaining
    // ancestors in the queue are also already in display_node_ids.
    base::queue<ui::AXNode*> ancestors =
        content_node->GetAncestorsCrossingTreeBoundaryAsQueue();
    while (!ancestors.empty()) {
      ui::AXNodeID ancestor_id = ancestors.front()->id();
      if (base::Contains(display_node_ids_, ancestor_id))
        break;
      display_node_ids_.insert(ancestor_id);
      ancestors.pop();
    }

    // Add all descendant ids to the set.
    ui::AXNode* next_node = content_node;
    ui::AXNode* deepest_last_child =
        content_node->GetDeepestLastUnignoredChild();
    if (!deepest_last_child)
      continue;
    while (next_node != deepest_last_child) {
      next_node = next_node->GetNextUnignoredInTreeOrder();
      display_node_ids_.insert(next_node->id());
    }
  }
}

void ReadAnythingAppController::OnThemeChanged(ReadAnythingThemePtr new_theme) {
  background_color_ = new_theme->background_color;
  font_name_ = new_theme->font_name;
  font_size_ = new_theme->font_size;
  foreground_color_ = new_theme->foreground_color;
  letter_spacing_ = GetLetterSpacingValue(new_theme->letter_spacing);
  line_spacing_ = GetLineSpacingValue(new_theme->line_spacing);

  // TODO(abigailbklein): Use v8::Function rather than javascript. If possible,
  // replace this function call with firing an event.
  std::string script = "chrome.readAnything.updateTheme();";
  render_frame_->ExecuteJavaScript(base::ASCIIToUTF16(script));
}

gin::ObjectTemplateBuilder ReadAnythingAppController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<ReadAnythingAppController>::GetObjectTemplateBuilder(
             isolate)
      .SetProperty("backgroundColor",
                   &ReadAnythingAppController::BackgroundColor)
      .SetProperty("rootId", &ReadAnythingAppController::RootId)
      .SetProperty("fontName", &ReadAnythingAppController::FontName)
      .SetProperty("fontSize", &ReadAnythingAppController::FontSize)
      .SetProperty("foregroundColor",
                   &ReadAnythingAppController::ForegroundColor)
      .SetProperty("letterSpacing", &ReadAnythingAppController::LetterSpacing)
      .SetProperty("lineSpacing", &ReadAnythingAppController::LineSpacing)
      .SetMethod("getChildren", &ReadAnythingAppController::GetChildren)
      .SetMethod("getTextDirection",
                 &ReadAnythingAppController::GetTextDirection)
      .SetMethod("getHtmlTag", &ReadAnythingAppController::GetHtmlTag)
      .SetMethod("getLanguage", &ReadAnythingAppController::GetLanguage)
      .SetMethod("getTextContent", &ReadAnythingAppController::GetTextContent)
      .SetMethod("getUrl", &ReadAnythingAppController::GetUrl)
      .SetMethod("onConnected", &ReadAnythingAppController::OnConnected)
      .SetMethod("onLinkClicked", &ReadAnythingAppController::OnLinkClicked)
      .SetMethod("setContentForTesting",
                 &ReadAnythingAppController::SetContentForTesting)
      .SetMethod("setThemeForTesting",
                 &ReadAnythingAppController::SetThemeForTesting);
}

ui::AXNodeID ReadAnythingAppController::RootId() const {
  return tree_->root()->id();
}

SkColor ReadAnythingAppController::BackgroundColor() const {
  return background_color_;
}

std::string ReadAnythingAppController::FontName() const {
  return font_name_;
}

float ReadAnythingAppController::FontSize() const {
  return font_size_;
}

SkColor ReadAnythingAppController::ForegroundColor() const {
  return foreground_color_;
}

float ReadAnythingAppController::LetterSpacing() const {
  return letter_spacing_;
}

float ReadAnythingAppController::LineSpacing() const {
  return line_spacing_;
}

std::vector<ui::AXNodeID> ReadAnythingAppController::GetChildren(
    ui::AXNodeID ax_node_id) const {
  std::vector<ui::AXNodeID> child_ids;
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  DCHECK(ax_node);
  for (auto it = ax_node->UnignoredChildrenBegin();
       it != ax_node->UnignoredChildrenEnd(); ++it) {
    if (base::Contains(display_node_ids_, it->id()))
      child_ids.push_back(it->id());
  }
  return child_ids;
}

std::string ReadAnythingAppController::GetHtmlTag(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  DCHECK(ax_node);
  return ax_node->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
}

std::string ReadAnythingAppController::GetLanguage(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  DCHECK(ax_node);
  if (NodeIsContentNode(ax_node_id))
    return ax_node->GetLanguage();
  return ax_node->GetStringAttribute(ax::mojom::StringAttribute::kLanguage);
}

std::string ReadAnythingAppController::GetTextContent(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  DCHECK(ax_node);
  std::string text_content = ax_node->GetTextContentUTF8();
  // If this node is the start or end node, truncate the text content by the
  // corresponding offset.
  if (has_selection_) {
    if (ax_node == start_node_)
      text_content.erase(0, start_offset_);
    if (ax_node == end_node_)
      text_content.resize(end_offset_);
  }
  return text_content;
}

std::string ReadAnythingAppController::GetTextDirection(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  if (!ax_node)
    return std::string();

  auto text_direction = static_cast<ax::mojom::WritingDirection>(
      ax_node->GetIntAttribute(ax::mojom::IntAttribute::kTextDirection));

  // Vertical writing is displayed horizontally with "auto".
  switch (text_direction) {
    case ax::mojom::WritingDirection::kLtr:
      return "ltr";
    case ax::mojom::WritingDirection::kRtl:
      return "rtl";
    case ax::mojom::WritingDirection::kTtb:
      return "auto";
    case ax::mojom::WritingDirection::kBtt:
      return "auto";
    default:
      return std::string();
  }
}

std::string ReadAnythingAppController::GetUrl(ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  DCHECK(ax_node);
  return ax_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl);
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

void ReadAnythingAppController::OnLinkClicked(ui::AXNodeID ax_node_id) const {
  static const char* const kLinkElementTarget = "target";
  static const char* const kLinkElementBlank = "_blank";
  std::string url = GetUrl(ax_node_id);
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  DCHECK(ax_node);
  std::u16string target_attribute =
      ax_node->GetHtmlAttribute(kLinkElementTarget);
  bool open_in_new_tab = base::EqualsASCII(target_attribute, kLinkElementBlank);
  page_handler_->OnLinkClicked(GURL(url), open_in_new_tab);
}

void ReadAnythingAppController::SetThemeForTesting(const std::string& font_name,
                                                   float font_size,
                                                   SkColor foreground_color,
                                                   SkColor background_color,
                                                   int line_spacing,
                                                   int letter_spacing) {
  auto line_spacing_enum =
      static_cast<read_anything::mojom::Spacing>(line_spacing);
  auto letter_spacing_enum =
      static_cast<read_anything::mojom::Spacing>(letter_spacing);
  OnThemeChanged(ReadAnythingTheme::New(font_name, font_size, foreground_color,
                                        background_color, line_spacing_enum,
                                        letter_spacing_enum));
}

void ReadAnythingAppController::SetContentForTesting(
    v8::Local<v8::Value> v8_snapshot_lite,
    std::vector<ui::AXNodeID> content_node_ids) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  ui::AXTreeUpdate snapshot =
      GetSnapshotFromV8SnapshotLite(isolate, v8_snapshot_lite);
  OnAXTreeDistilled(snapshot, content_node_ids);
}

double ReadAnythingAppController::GetLetterSpacingValue(
    read_anything::mojom::Spacing letter_spacing) const {
  // auto ls = static_cast<read_anything::mojom::Spacing>(letter_spacing);
  switch (letter_spacing) {
    case read_anything::mojom::Spacing::kTight:
      return -0.05;
    case read_anything::mojom::Spacing::kDefault:
      return 0;
    case read_anything::mojom::Spacing::kLoose:
      return 0.05;
    case read_anything::mojom::Spacing::kVeryLoose:
      return 0.1;
  }
}

double ReadAnythingAppController::GetLineSpacingValue(
    read_anything::mojom::Spacing line_spacing) const {
  // auto ls = static_cast<read_anything::mojom::Spacing>(line_spacing);
  switch (line_spacing) {
    case read_anything::mojom::Spacing::kTight:
      return 1.0;
    case read_anything::mojom::Spacing::kDefault:
    default:
      return 1.15;
    case read_anything::mojom::Spacing::kLoose:
      return 1.5;
    case read_anything::mojom::Spacing::kVeryLoose:
      return 2.0;
  }
}

ui::AXNode* ReadAnythingAppController::GetAXNode(
    ui::AXNodeID ax_node_id) const {
  DCHECK(tree_);
  return tree_->GetFromId(ax_node_id);
}

bool ReadAnythingAppController::NodeIsContentNode(
    ui::AXNodeID ax_node_id) const {
  return base::Contains(content_node_ids_, ax_node_id);
}
