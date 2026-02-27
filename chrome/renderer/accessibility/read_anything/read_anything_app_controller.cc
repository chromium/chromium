// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_anything_app_controller.h"

#include <algorithm>
#include <climits>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/read_anything/read_anything.mojom-shared.h"
#include "chrome/common/read_anything/read_anything_util.h"
#include "chrome/renderer/accessibility/ax_tree_distiller.h"
#include "chrome/renderer/accessibility/phrase_segmentation/dependency_parser_model.h"
#include "chrome/renderer/accessibility/read_anything/read_aloud_traversal_utils.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_app_model.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_node_utils.h"
#include "components/language/core/common/locale_util.h"
#include "components/translate/core/common/translate_constants.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/object_template_builder.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/mojom/ax_event.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "url/url_util.h"
#include "v8/include/cppgc/allocation.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-cppgc.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-typed-array.h"

namespace {

// The amount of time after a new page has been opened in reading mode that
// reading mode waits before logging whether the distillation was successful,
// the distillation failed, or the distillation is still processing.
constexpr int kDistillationLoggingDelayMs = 5000;

constexpr char kUndeterminedLocale[] = "und";

// The number of seconds to wait before distilling after a user has stopped
// entering text into a richly editable text field.
const double kPostInputDistillSeconds = 1.5;

// The following methods convert v8::Value types to an AXTreeUpdate. This is not
// a complete conversion (thus way gin::Converter<ui::AXTreeUpdate> is not used
// or implemented) but just converting the bare minimum data types needed for
// the ReadAnythingAppTest.

void SetAXNodeDataChildIds(v8::Isolate* isolate,
                           gin::Dictionary* v8_dict,
                           ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_child_ids;
  v8_dict->Get("childIds", &v8_child_ids);
  std::vector<int32_t> child_ids;
  if (!gin::ConvertFromV8(isolate, v8_child_ids, &child_ids)) {
    return;
  }
  ax_node_data->child_ids = std::move(child_ids);
}

void SetAXNodeDataId(v8::Isolate* isolate,
                     gin::Dictionary* v8_dict,
                     ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_id;
  v8_dict->Get("id", &v8_id);
  ui::AXNodeID id;
  if (!gin::ConvertFromV8(isolate, v8_id, &id)) {
    return;
  }
  ax_node_data->id = id;
}

void SetAXNodeDataLanguage(v8::Isolate* isolate,
                           gin::Dictionary* v8_dict,
                           ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_language;
  v8_dict->Get("language", &v8_language);
  std::string language;
  if (!gin::ConvertFromV8(isolate, v8_language, &language)) {
    return;
  }
  ax_node_data->AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                   language);
}

void SetAXNodeDataName(v8::Isolate* isolate,
                       gin::Dictionary* v8_dict,
                       ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_name;
  v8_dict->Get("name", &v8_name);
  std::string name;
  if (!gin::ConvertFromV8(isolate, v8_name, &name)) {
    return;
  }
  ax_node_data->SetName(name);
  ax_node_data->SetNameFrom(ax::mojom::NameFrom::kContents);
}

void SetAXNodeDataRole(v8::Isolate* isolate,
                       gin::Dictionary* v8_dict,
                       ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_role;
  v8_dict->Get("role", &v8_role);
  std::string role_name;
  if (!gin::ConvertFromV8(isolate, v8_role, &role_name)) {
    return;
  }
  if (role_name == "rootWebArea") {
    ax_node_data->role = ax::mojom::Role::kRootWebArea;
  } else if (role_name == "heading") {
    ax_node_data->role = ax::mojom::Role::kHeading;
  } else if (role_name == "link") {
    ax_node_data->role = ax::mojom::Role::kLink;
  } else if (role_name == "paragraph") {
    ax_node_data->role = ax::mojom::Role::kParagraph;
  } else if (role_name == "staticText") {
    ax_node_data->role = ax::mojom::Role::kStaticText;
  } else if (role_name == "button") {
    ax_node_data->role = ax::mojom::Role::kButton;
  }
}

void SetAXNodeDataHtmlTag(v8::Isolate* isolate,
                          gin::Dictionary* v8_dict,
                          ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_html_tag;
  v8_dict->Get("htmlTag", &v8_html_tag);
  std::string html_tag;
  if (!gin::Converter<std::string>::FromV8(isolate, v8_html_tag, &html_tag)) {
    return;
  }
  ax_node_data->AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag,
                                   html_tag);
}

void SetAXNodeDataDisplay(v8::Isolate* isolate,
                          gin::Dictionary* v8_dict,
                          ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_display;
  v8_dict->Get("display", &v8_display);
  std::string display;
  if (!gin::Converter<std::string>::FromV8(isolate, v8_display, &display)) {
    return;
  }
  ax_node_data->AddStringAttribute(ax::mojom::StringAttribute::kDisplay,
                                   display);
}

void SetAXNodeDataTextDirection(v8::Isolate* isolate,
                                gin::Dictionary* v8_dict,
                                ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_direction;
  v8_dict->Get("direction", &v8_direction);
  int direction;
  if (!gin::ConvertFromV8(isolate, v8_direction, &direction)) {
    return;
  }
  ax_node_data->AddIntAttribute(ax::mojom::IntAttribute::kTextDirection,
                                direction);
}

void SetAXNodeDataTextStyle(v8::Isolate* isolate,
                            gin::Dictionary* v8_dict,
                            ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_text_style;
  v8_dict->Get("textStyle", &v8_text_style);
  std::string text_style;
  if (!gin::ConvertFromV8(isolate, v8_text_style, &text_style)) {
    return;
  }
  if (text_style.find("underline") != std::string::npos) {
    ax_node_data->AddTextStyle(ax::mojom::TextStyle::kUnderline);
  }
  if (text_style.find("overline") != std::string::npos) {
    ax_node_data->AddTextStyle(ax::mojom::TextStyle::kOverline);
  }
  if (text_style.find("italic") != std::string::npos) {
    ax_node_data->AddTextStyle(ax::mojom::TextStyle::kItalic);
  }
  if (text_style.find("bold") != std::string::npos) {
    ax_node_data->AddTextStyle(ax::mojom::TextStyle::kBold);
  }
}

void SetAXNodeDataUrl(v8::Isolate* isolate,
                      gin::Dictionary* v8_dict,
                      ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_url;
  v8_dict->Get("url", &v8_url);
  std::string url;
  if (!gin::ConvertFromV8(isolate, v8_url, &url)) {
    return;
  }
  ax_node_data->AddStringAttribute(ax::mojom::StringAttribute::kUrl, url);
}

void SetSelectionAnchorObjectId(v8::Isolate* isolate,
                                gin::Dictionary* v8_dict,
                                ui::AXTreeData* ax_tree_data) {
  v8::Local<v8::Value> v8_anchor_object_id;
  v8_dict->Get("anchor_object_id", &v8_anchor_object_id);
  ui::AXNodeID sel_anchor_object_id;
  if (!gin::ConvertFromV8(isolate, v8_anchor_object_id,
                          &sel_anchor_object_id)) {
    return;
  }
  ax_tree_data->sel_anchor_object_id = sel_anchor_object_id;
}

void SetSelectionFocusObjectId(v8::Isolate* isolate,
                               gin::Dictionary* v8_dict,
                               ui::AXTreeData* ax_tree_data) {
  v8::Local<v8::Value> v8_focus_object_id;
  v8_dict->Get("focus_object_id", &v8_focus_object_id);
  ui::AXNodeID sel_focus_object_id;
  if (!gin::ConvertFromV8(isolate, v8_focus_object_id, &sel_focus_object_id)) {
    return;
  }
  ax_tree_data->sel_focus_object_id = sel_focus_object_id;
}

void SetSelectionAnchorOffset(v8::Isolate* isolate,
                              gin::Dictionary* v8_dict,
                              ui::AXTreeData* ax_tree_data) {
  v8::Local<v8::Value> v8_anchor_offset;
  v8_dict->Get("anchor_offset", &v8_anchor_offset);
  int32_t sel_anchor_offset;
  if (!gin::ConvertFromV8(isolate, v8_anchor_offset, &sel_anchor_offset)) {
    return;
  }
  ax_tree_data->sel_anchor_offset = sel_anchor_offset;
}

void SetSelectionFocusOffset(v8::Isolate* isolate,
                             gin::Dictionary* v8_dict,
                             ui::AXTreeData* ax_tree_data) {
  v8::Local<v8::Value> v8_focus_offset;
  v8_dict->Get("focus_offset", &v8_focus_offset);
  int32_t sel_focus_offset;
  if (!gin::ConvertFromV8(isolate, v8_focus_offset, &sel_focus_offset)) {
    return;
  }
  ax_tree_data->sel_focus_offset = sel_focus_offset;
}

void SetSelectionIsBackward(v8::Isolate* isolate,
                            gin::Dictionary* v8_dict,
                            ui::AXTreeData* ax_tree_data) {
  v8::Local<v8::Value> v8_sel_is_backward;
  v8_dict->Get("is_backward", &v8_sel_is_backward);
  bool sel_is_backward;
  if (!gin::ConvertFromV8(isolate, v8_sel_is_backward, &sel_is_backward)) {
    return;
  }
  ax_tree_data->sel_is_backward = sel_is_backward;
}

void SetAXTreeUpdateRootId(v8::Isolate* isolate,
                           gin::Dictionary* v8_dict,
                           ui::AXTreeUpdate* snapshot) {
  v8::Local<v8::Value> v8_root_id;
  v8_dict->Get("rootId", &v8_root_id);
  ui::AXNodeID root_id;
  if (!gin::ConvertFromV8(isolate, v8_root_id, &root_id)) {
    return;
  }
  snapshot->root_id = root_id;
}

ui::AXTreeUpdate GetSnapshotFromV8SnapshotLite(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_snapshot_lite) {
  ui::AXTreeUpdate snapshot;
  ui::AXTreeData ax_tree_data;
  ax_tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  snapshot.has_tree_data = true;
  snapshot.tree_data = ax_tree_data;
  gin::Dictionary v8_snapshot_dict(isolate);
  if (!gin::ConvertFromV8(isolate, v8_snapshot_lite, &v8_snapshot_dict)) {
    return snapshot;
  }
  SetAXTreeUpdateRootId(isolate, &v8_snapshot_dict, &snapshot);

  v8::Local<v8::Value> v8_nodes;
  v8_snapshot_dict.Get("nodes", &v8_nodes);
  v8::LocalVector<v8::Value> v8_nodes_vector(isolate);
  if (!gin::ConvertFromV8(isolate, v8_nodes, &v8_nodes_vector)) {
    return snapshot;
  }
  for (v8::Local<v8::Value> v8_node : v8_nodes_vector) {
    gin::Dictionary v8_node_dict(isolate);
    if (!gin::ConvertFromV8(isolate, v8_node, &v8_node_dict)) {
      continue;
    }
    ui::AXNodeData ax_node_data;
    SetAXNodeDataId(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataRole(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataName(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataChildIds(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataHtmlTag(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataLanguage(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataTextDirection(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataTextStyle(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataUrl(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataDisplay(isolate, &v8_node_dict, &ax_node_data);
    snapshot.nodes.push_back(ax_node_data);
  }

  v8::Local<v8::Value> v8_selection;
  v8_snapshot_dict.Get("selection", &v8_selection);
  gin::Dictionary v8_selection_dict(isolate);
  if (!gin::ConvertFromV8(isolate, v8_selection, &v8_selection_dict)) {
    return snapshot;
  }
  SetSelectionAnchorObjectId(isolate, &v8_selection_dict, &snapshot.tree_data);
  SetSelectionFocusObjectId(isolate, &v8_selection_dict, &snapshot.tree_data);
  SetSelectionAnchorOffset(isolate, &v8_selection_dict, &snapshot.tree_data);
  SetSelectionFocusOffset(isolate, &v8_selection_dict, &snapshot.tree_data);
  SetSelectionIsBackward(isolate, &v8_selection_dict, &snapshot.tree_data);
  return snapshot;
}

SkBitmap CorrectColorOfBitMap(SkBitmap& originalBitmap) {
  SkBitmap converted;
  converted.allocPixels(SkImageInfo::Make(
      originalBitmap.width(), originalBitmap.height(),
      SkColorType::kRGBA_8888_SkColorType, originalBitmap.alphaType()));

  originalBitmap.readPixels(converted.info(), converted.getPixels(),
                            converted.rowBytes(), 0, 0);
  return converted;
}

template <typename T>
  requires(std::is_enum_v<T> &&
           requires {
             T::kMinValue;
             T::kMaxValue;
           })
std::optional<T> ToEnum(int value) {
  if (value >= std::to_underlying(T::kMinValue) &&
      value <= std::to_underlying(T::kMaxValue)) {
    return static_cast<T>(value);
  }
  return std::nullopt;
}

}  // namespace

// static
gin::WrapperInfo ReadAnythingAppController::kWrapperInfo = {
    {gin::kEmbedderNativeGin},
    gin::kReadAnythingAppController};

// static
ReadAnythingAppController* ReadAnythingAppController::Install(
    content::RenderFrame* render_frame) {
  v8::Isolate* isolate =
      render_frame->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame->GetWebFrame()->MainWorldScriptContext();
  if (context.IsEmpty()) {
    return nullptr;
  }
  v8::MicrotasksScope microtask_scope(isolate, context->GetMicrotaskQueue(),
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Context::Scope context_scope(context);

  ReadAnythingAppController* controller =
      cppgc::MakeGarbageCollected<ReadAnythingAppController>(
          isolate->GetCppHeap()->GetAllocationHandle(), render_frame);

  v8::Local<v8::Value> controller_v8;
  if (!controller->GetWrapper(isolate).ToLocal(&controller_v8)) {
    return nullptr;
  }

  v8::Local<v8::Object> chrome =
      content::GetOrCreateChromeObject(isolate, context);
  chrome->Set(context, gin::StringToV8(isolate, "readingMode"), controller_v8)
      .Check();
  return controller;
}

ReadAnythingAppController::ReadAnythingAppController(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  post_user_entry_draw_timer_ = std::make_unique<base::RetainingOneShotTimer>(
      FROM_HERE, base::Seconds(kPostInputDistillSeconds),
      base::BindRepeating(&ReadAnythingAppController::Draw,
                          weak_ptr_factory_.GetWeakPtr(),
                          /* recompute_display_nodes= */ true));
  renderer_load_triggered_time_ms_ = base::TimeTicks::Now();
  distiller_ = std::make_unique<AXTreeDistiller>(
      render_frame,
      base::BindRepeating(&ReadAnythingAppController::OnAXTreeDistilled,
                          weak_ptr_factory_.GetWeakPtr()));
  // TODO(crbug.com/40915547): Use a global ukm recorder instance instead.
  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
  content::RenderThread::Get()->BindHostReceiver(
      factory.BindNewPipeAndPassReceiver());
  ukm_recorder_ = ukm::MojoUkmRecorder::Create(*factory);
  if (features::IsDataCollectionModeForScreen2xEnabled()) {
    model_.SetDataCollectionForScreen2xCallback(
        base::BindOnce(&ReadAnythingAppController::DistillAndScreenshot,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  model_observer_.Observe(&model_);
  self_ = this;
}

ReadAnythingAppController::~ReadAnythingAppController() {
  RecordNumSelections();
  post_user_entry_draw_timer_->Stop();
}

void ReadAnythingAppController::OnDestruct() {
  self_.Clear();
}

void ReadAnythingAppController::OnNodeWillBeDeleted(ui::AXTree* tree,
                                                    ui::AXNode* node) {
  ui::AXNodeID node_id = CHECK_DEREF(node).id();
  if (model_.GetCurrentlyVisibleNodes()->contains(node_id)) {
    displayed_nodes_pending_deletion_.insert(node_id);
    if (!read_aloud_model_.speech_playing()) {
      ExecuteJavaScript("chrome.readingMode.onNodeWillBeDeleted(" +
                        base::ToString(node_id) + ")");
    }
  }
}

void ReadAnythingAppController::OnNodeDeleted(ui::AXTree* tree,
                                              ui::AXNodeID node_id) {
  if (!displayed_nodes_pending_deletion_.contains(node_id)) {
    return;
  }

  displayed_nodes_pending_deletion_.erase(node_id);

  // For Google Docs, we extract text from the "annotated canvas" element
  // nodes, which hold the currently visible text on screen. As the user
  // scrolls, these canvas elements are dynamically updated, resulting in
  // frequent calls to OnNodeDeleted. We found that redrawing content in the
  // Reading Model panel after node deletion during scrolling can lead to
  // unexpected behavior (e.g., an empty side panel). Therefore, Google Docs
  // require special handling to ensure correct text extraction and avoid
  // these issues.
  if (!displayed_nodes_pending_deletion_.empty() || IsGoogleDocs()) {
    return;
  }

  // Instead of redrawing everything, inform the webui that the node is being
  // deleted and it will adjust on that side. See OnNodeWillBeDeleted.
  if (model_.has_selection()) {
    DrawSelection();
  }
}

void ReadAnythingAppController::OnTreeDataChanged(
    ui::AXTree* tree,
    const ui::AXTreeData& old_data,
    const ui::AXTreeData& new_data) {
  if (model_.is_readability_next_distillation_method()) {
    return;
  }
  VLOG(1) << "Tree data changed for tree ID: " << tree->GetAXTreeID()
          << "\n---- OLD DATA: " << old_data.tree_id << ": "
          << old_data.ToString() << "\n---- NEW DATA: " << new_data.tree_id
          << ": " << new_data.ToString();
  // If we are waiting for the tree id of the active tree to be populated,
  // distill once we have it.
  if (waiting_for_tree_id_ && old_data.tree_id == ui::AXTreeIDUnknown() &&
      new_data.tree_id != ui::AXTreeIDUnknown() &&
      model_.active_tree_id() == tree->GetAXTreeID()) {
    VLOG(1) << "OnTreeDataChanged populated the active tree ID: "
            << new_data.tree_id;
    Distill();
  }
}

void ReadAnythingAppController::OnStringAttributeChanged(
    ui::AXTree* tree,
    ui::AXNode* node,
    ax::mojom::StringAttribute attr,
    const std::string& old_value,
    const std::string& new_value) {
  // Return early when the images flag is disabled to avoid potential crashes.
  if (!features::IsReadAnythingImagesViaAlgorithmEnabled() ||
      attr != ax::mojom::StringAttribute::kUrl) {
    return;
  }

  ui::AXNode* rm_node = model_.GetAXNode(node->id());
  if (!rm_node) {
    return;
  }
  // When the src for an image changes (e.g if an image was lazy loaded and
  // previously had a placeholder image), request the updated image. The info
  // will be returned via OnImageDataDownloaded.
  if (rm_node->GetRole() == ax::mojom::Role::kImage) {
    RequestImageData(node->id());
  }
}

bool ReadAnythingAppController::IsUpdateProcessingPaused() const {
  if (model_.screen2x_distiller_running() ||
      read_aloud_model_.speech_playing()) {
    return true;
  }

  if (model_.active_presentation_state() ==
      read_anything::mojom::ReadAnythingPresentationState::
          kInImmersiveOverlay) {
    // We only want to block the processing/distillation pipeline if there is
    // already a good distillation on IRM. If a distillation is pending, or if
    // the current distillation is empty, we don't want to block the pending
    // update.
    return model_.distillation_state() ==
           read_anything::mojom::ReadAnythingDistillationState::
               kDistillationWithContent;
  }

  return false;
}

void ReadAnythingAppController::ProcessPendingUpdatesIfAllowed() {
  if (IsUpdateProcessingPaused() || !model_.ContainsActiveTree()) {
    return;
  }

  model_.UnserializePendingUpdates(model_.active_tree_id());
  ProcessModelUpdates();
}

void ReadAnythingAppController::AccessibilityEventReceived(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    const std::vector<ui::AXEvent>& events) {
  model_.PrepareForAXTreeUpdates(tree_id);
  if (IsReadabilityEnabled() && IsReadabilityWithLinksEnabled() &&
      model_.should_extract_anchors_from_tree_for_readability()) {
    model_.ApplyAccessibilityUpdates(
        tree_id, const_cast<std::vector<ui::AXTreeUpdate>&>(updates),
        const_cast<std::vector<ui::AXEvent>&>(events));
    // If the tree is not ready, ProcessAXTreeAnchors will do an early return
    // and wait for the next update until it is able to process the tree.
    bool didProcessAnchors = model_.ProcessAXTreeAnchors();
    if (didProcessAnchors) {
      ExecuteJavaScript("chrome.readingMode.onAnchorsReadyForReadability();");
    }
    return;
  }

  // Remove the const-ness of the data here so that subsequent methods can move
  // the data.
  if (tree_id == model_.active_tree_id() && IsUpdateProcessingPaused()) {
    VLOG(1)
        << "In AccessibilityEventReceived. Calling QueueAccessibilityUpdates "
           "because distiller should not run yet.";

    model_.QueueAccessibilityUpdates(
        tree_id, const_cast<std::vector<ui::AXTreeUpdate>&>(updates),
        const_cast<std::vector<ui::AXEvent>&>(events));
  } else {
    model_.ApplyAccessibilityUpdates(
        tree_id, const_cast<std::vector<ui::AXTreeUpdate>&>(updates),
        const_cast<std::vector<ui::AXEvent>&>(events));
  }

  // From this point onward, `updates` and `events` should not be accessed.
  if (tree_id != model_.active_tree_id() || IsUpdateProcessingPaused()) {
    return;
  }

  ProcessModelUpdates();
}

void ReadAnythingAppController::ProcessModelUpdates() {
  if (model_.requires_distillation()) {
    if (model_.is_readability_next_distillation_method()) {
      return;
    }
    Distill();
  }

  if (model_.redraw_required()) {
    model_.reset_redraw_required();
    Draw(/* recompute_display_nodes= */ true);
  }

  // TODO(accessibility): it isn't clear this handles the pending updates path
  // correctly within the model.
  if (model_.requires_post_process_selection()) {
    PostProcessSelection();
  }

  // If the user typed something, this value will be true and it will reset the
  // timer to distill.
  if (model_.reset_draw_timer()) {
    post_user_entry_draw_timer_->Reset();
    model_.set_reset_draw_timer(false);
  }
}

void ReadAnythingAppController::AccessibilityLocationChangesReceived(
    const ui::AXTreeID& tree_id,
    const ui::AXLocationAndScrollUpdates& details) {
  NOTREACHED() << "Non-const ref version of this method should be used as a "
                  "performance optimization.";
}

void ReadAnythingAppController::AccessibilityLocationChangesReceived(
    const ui::AXTreeID& tree_id,
    ui::AXLocationAndScrollUpdates& details) {
  // AccessibilityLocationChangesReceived causes some unexpected crashes and
  // AXNode behavior. Therefore, flag-guard this behind the
  // IsReadAnythingDocsIntegration flag, since these changes were initially
  // added to support Google Docs. See crbug.com/411776559.
  if (!features::IsReadAnythingDocsIntegrationEnabled()) {
    return;
  }
  // If the AccessibilityLocationChangesReceived callback happens after
  // the current active tree has been destroyed, do nothing.
  DUMP_WILL_BE_CHECK(model_.active_tree_id() != ui::AXTreeIDUnknown());
  DUMP_WILL_BE_CHECK(model_.ContainsTree(tree_id));
  // TODO: crbug.com/411776559- Determine if a DUMP_WILL_BE_CHECK is needed
  // here or if it's okay to just ignore AccessibilityLocationChangesReceived
  // events if they're sent not on the active tree.
  DUMP_WILL_BE_CHECK(model_.active_tree_id() == tree_id);
  if (model_.active_tree_id() == ui::AXTreeIDUnknown() ||
      !model_.ContainsTree(tree_id) || model_.active_tree_id() != tree_id) {
    return;
  }
  // Listen to location change notifications to update locations of the nodes
  // accordingly.
  for (auto& change : details.location_changes) {
    ui::AXNode* ax_node = model_.GetAXNode(change.id);
    if (!ax_node) {
      continue;
    }
    ax_node->SetLocation(change.new_location.offset_container_id,
                         change.new_location.bounds,
                         change.new_location.transform.get());
  }
}

void ReadAnythingAppController::ExecuteJavaScript(const std::string& script) {
  // TODO(crbug.com/40802192): Use v8::Function rather than javascript. If
  // possible, replace this function call with firing an event.
  render_frame()->ExecuteJavaScript(base::ASCIIToUTF16(script));
}

void ReadAnythingAppController::SetDistillationState(
    read_anything::mojom::ReadAnythingDistillationState state) {
  if (model_.distillation_state() == state) {
    return;
  }

  page_handler_->OnDistillationStateChanged(state);
  model_.set_distillation_state(state);
  // Ensure that we always clear the AXTree anchors when a new
  // distillation occurs.
  if (IsReadabilityWithLinksEnabled() &&
      state == read_anything::mojom::ReadAnythingDistillationState::
                   kDistillationInProgress) {
    model_.set_should_extract_anchors_from_tree_for_readability(false);
    model_.ResetAXTreeAnchors();
  }
}

void ReadAnythingAppController::OnActiveAXTreeIDChanged(
    const ui::AXTreeID& tree_id,
    ukm::SourceId ukm_source_id,
    bool is_pdf) {
  if (tree_id == model_.active_tree_id() && !is_pdf) {
    VLOG(1) << "On active tree changed with same id: " << tree_id;
    return;
  }
  VLOG(1) << "On active tree changed with new id: " << tree_id;
  RecordNumSelections();

  // If the previous tree was not unknown (e.g. this is not the first tree
  // seen), log the words that were seen on the previous tree.
  if (model_.active_tree_id() != ui::AXTreeIDUnknown()) {
    RecordEstimatedWordsSeen();
    RecordEstimatedWordsHeard();
  }

  // Cancel any running draw timers.
  post_user_entry_draw_timer_->Stop();

  model_.SetRootTreeId(tree_id);
  model_.SetUkmSourceIdForTree(tree_id, ukm_source_id);
  model_.set_is_pdf(is_pdf);

  if (read_aloud_model_.speech_playing()) {
    model_.SetUrlInformationCallback(
        base::BindOnce(&ReadAnythingAppController::OnUrlInformationSet,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Delete all pending updates on the formerly active AXTree.
  // TODO(crbug.com/40802192): If distillation is in progress, cancel the
  // distillation request.
  model_.ClearPendingUpdates();
  model_.set_requires_distillation(false);
  model_.set_page_finished_loading(false);

  // Clear any stale distillation content.
  dom_distiller_title_.clear();
  dom_distiller_content_html_.clear();

  // Reset the distillation method for the new page. Every navigation
  // starts with the flag-determined distillation method before potentially
  // falling back to Screen2x if needed. If the new page is a PDF, the
  // distillation method is set to Screen2x directly.
  // We also update |next_distillation_method| since showLoading will clear the
  // previous active distillation in case there's any.
  auto initial_method = GetInitialDistillationMethod(is_pdf);
  model_.set_next_distillation_method(initial_method);
  model_.set_current_content_distillation_method(initial_method);

  ExecuteJavaScript("chrome.readingMode.showLoading();");

  if (model_.is_readability_next_distillation_method()) {
    return;
  }
  DistillNewTree();
}

ReadAnythingAppModel::DistillationMethod
ReadAnythingAppController::GetInitialDistillationMethod(bool is_pdf) const {
  // If |is_pdf| = true, override IsReadAnythingWithReadabilityEnabled flag and
  // return kScreen2x.
  return is_pdf || !features::IsReadAnythingWithReadabilityEnabled()
             ? ReadAnythingAppModel::DistillationMethod::kScreen2x
             : ReadAnythingAppModel::DistillationMethod::kReadability;
}

void ReadAnythingAppController::DistillNewTree() {
  // After the active tree has changed, start a timer for logging distillation
  // success or failures. Logging this via a timer reduces duplicate
  // distillation / failures being logged.
  distillationsCompleted_ = 0;
  timer_.Stop();
  timer_.Start(
      FROM_HERE, base::Milliseconds(kDistillationLoggingDelayMs),
      base::BindOnce(&ReadAnythingAppController::RecordDistillationSuccess,
                     base::Unretained(this)));

  if (features::IsImmersiveReadAnythingEnabled()) {
    SetDistillationState(read_anything::mojom::ReadAnythingDistillationState::
                             kDistillationInProgress);
  }

  // When the UI first constructs, this function may be called before tree_id
  // has been added to the tree list in AccessibilityEventReceived. In that
  // case, do not distill.
  if (model_.active_tree_id() != ui::AXTreeIDUnknown() &&
      model_.ContainsActiveTree()) {
    Distill();
  }
}

void ReadAnythingAppController::RecordDistillationSuccess() {
  read_anything::mojom::DistillationStatus distillationStatus;
  if (model_.screen2x_distiller_running()) {
    distillationStatus =
        distillationsCompleted_ > 0
            ? read_anything::mojom::DistillationStatus::kRestarted
            : read_anything::mojom::DistillationStatus::kStillRunning;
  } else if (!model_.content_node_ids().empty()) {
    distillationStatus = read_anything::mojom::DistillationStatus::kSuccess;
  } else {
    distillationStatus = read_anything::mojom::DistillationStatus::kFailure;
  }

  page_handler_->OnDistillationStatus(distillationStatus,
                                      model_.words_distilled());
  ukm::builders::Accessibility_ReadAnything_Distillation(
      model_.GetUkmSourceId())
      .SetDistillationStatus(static_cast<int>(distillationStatus))
      .Record(ukm_recorder_.get());
  distillationsCompleted_ = 0;
}

void ReadAnythingAppController::RecordNumSelections() {
  ukm::builders::Accessibility_ReadAnything_EmptyState(model_.GetUkmSourceId())
      .SetTotalNumSelections(model_.GetNumSelections())
      .Record(ukm_recorder_.get());
  model_.SetNumSelections(0);
}

void ReadAnythingAppController::RecordEstimatedWordsSeen() {
  VLOG(1) << "Words seen: " << model_.words_seen();
  base::UmaHistogramCustomCounts(kWordsSeenHistogramName, model_.words_seen(),
                                 1, kMaxWordsConsumed, kWordsConsumedBuckets);
  model_.set_words_seen(0);
}

void ReadAnythingAppController::RecordEstimatedWordsHeard() {
  VLOG(1) << "Words heard: " << model_.words_heard();
  base::UmaHistogramCustomCounts(kWordsHeardHistogramName, model_.words_heard(),
                                 1, kMaxWordsConsumed, kWordsConsumedBuckets);
  model_.set_words_heard(0);
}

void ReadAnythingAppController::OnAXTreeDestroyed(const ui::AXTreeID& tree_id) {
  // Cancel any running draw timers.
  VLOG(1) << "OnAXTreeDestroyed: " << tree_id;
  post_user_entry_draw_timer_->Stop();
  model_.OnAXTreeDestroyed(tree_id);
}

void ReadAnythingAppController::DistillAndScreenshot() {
  // For screen2x data generation mode, chrome is opened from the CLI to a
  // specific URL. The caller monitors for a dump of the distilled proto written
  // to a local file. Distill should only be called once the page finished
  // loading and is stable, so the proto represents the entire webpage.
  CHECK(features::IsDataCollectionModeForScreen2xEnabled());
  CHECK(model_.PageFinishedLoadingForDataCollection());
  CHECK(model_.ScreenAIServiceReadyForDataCollection());

  Distill(/*for_training_data=*/true);
  page_handler_->OnScreenshotRequested();
}

void ReadAnythingAppController::Distill(bool for_training_data) {
  if (!for_training_data &&
      features::IsDataCollectionModeForScreen2xEnabled()) {
    return;
  }

  if (IsUpdateProcessingPaused()) {
    // When distillation is in progress, the model may have queued up tree
    // updates. In those cases, assume we eventually get to `OnAXTreeDistilled`,
    // where we re-request `Distill`. When speech is playing, assume it will
    // eventually stop and call `OnIsSpeechActiveChanged` where we
    // re-request `Distill`.
    model_.set_requires_distillation(true);
    return;
  }

  model_.set_requires_distillation(false);

  ui::AXSerializableTree* tree = model_.GetActiveTree();
  if (tree->GetAXTreeID() == ui::AXTreeIDUnknown()) {
    VLOG(1)
        << "Active tree's ID has not been populated yet, skipping distillation";
    waiting_for_tree_id_ = true;
    return;
  }
  std::unique_ptr<
      ui::AXTreeSource<const ui::AXNode*, ui::AXTreeData*, ui::AXNodeData>>
      tree_source(tree->CreateTreeSource());
  ui::AXTreeSerializer<const ui::AXNode*, std::vector<const ui::AXNode*>,
                       ui::AXTreeUpdate*, ui::AXTreeData*, ui::AXNodeData>
      serializer(tree_source.get());
  ui::AXTreeUpdate snapshot;
  if (!tree->root()) {
    return;
  }

  if (model_.requires_tree_lang()) {
    model_.set_requires_tree_lang(false);
    std::string tree_lang = tree->root()->GetLanguage();
    SetLanguageCode(tree_lang.empty()
                        ? read_aloud_model_.default_language_code()
                        : tree_lang);
  }
  CHECK(serializer.SerializeChanges(tree->root(), &snapshot));
  model_.set_screen2x_distiller_running(true);
  if (features::IsImmersiveReadAnythingEnabled()) {
    SetDistillationState(read_anything::mojom::ReadAnythingDistillationState::
                             kDistillationInProgress);
  }
  VLOG(1) << "Distilling tree with ID: " << tree->GetAXTreeID();
  distiller_->Distill(*tree, snapshot, model_.GetUkmSourceId());
}

void ReadAnythingAppController::OnAXTreeDistilled(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  // The distiller will call OnAXTreeDistilled when the main content extractor
  // disconnects. If this happens during middle of a distillation, there was an
  // error, and we should reset the model. However, this disconnect can also
  // happen after a long time of inactivity. In this case, we shouldn't reset
  // the model since the last state is still the correct state and clearing the
  // model causes issues for read aloud.
  if (!model_.screen2x_distiller_running() &&
      tree_id == ui::AXTreeIDUnknown() && content_node_ids.empty()) {
    VLOG(1) << "Distillation terminated after the main content extractor "
               "disconnected";
    return;
  }

  // Reset distillation in progress, because distillation just finished. This
  // is needed for the IsUpdateProcessingPaused check below, because it will
  // consider the processing pipeline paused if distillation is in progress.
  model_.set_screen2x_distiller_running(false);

  // Update active distillation method now that screen2x distillation has
  // finished.
  model_.set_current_content_distillation_method(
      ReadAnythingAppModel::DistillationMethod::kScreen2x);

  // If speech is playing, we don't want to redraw and disrupt speech. We will
  // re-distill once speech pauses.
  if (IsUpdateProcessingPaused()) {
    model_.set_requires_distillation(true);
    VLOG(1) << "Distillation terminated because update processing is paused";
    return;
  }
  // Reset state, including the current side panel selection so we can update
  // it based on the new main panel selection in PostProcessSelection below.ona
  model_.Reset(content_node_ids);
  read_aloud_model_.ResetReadAloudState();

  // Return early if any of the following scenarios occurred while waiting for
  // distillation to complete:
  // 1. tree_id != model_.active_tree_id(): The active tree was changed.
  // 2. model_.active_tree_id()== ui::AXTreeIDUnknown(): The active tree was
  // change to
  //    an unknown tree id.
  // 3. !model_.ContainsTree(tree_id): The distilled tree was destroyed.
  // 4. tree_id == ui::AXTreeIDUnknown(): The distiller sent back an unknown
  //    tree id which occurs when there was an error.
  if (tree_id != model_.active_tree_id() ||
      model_.active_tree_id() == ui::AXTreeIDUnknown() ||
      !model_.ContainsTree(tree_id) || tree_id == ui::AXTreeIDUnknown()) {
    // VLOG statements added to help with debugging issues with distillation
    // on non-dev builds.
    if (tree_id != model_.active_tree_id()) {
      VLOG(1) << "Distillation terminated because not on active tree";
    }
    if (model_.active_tree_id() == ui::AXTreeIDUnknown()) {
      VLOG(1) << "Distillation terminated because active tree is unknown";
    }
    if (!model_.ContainsTree(tree_id)) {
      VLOG(1) << "Distillation terminated because current tree is missing";
    }
    if (tree_id == ui::AXTreeIDUnknown()) {
      VLOG(1) << "Distillation terminated because current tree is unknown";
    }
    return;
  }

  if (!model_.content_node_ids().empty()) {
    // If there are content_node_ids, this means the AXTree was successfully
    // distilled. We must call this before PostProcessSelection() below because
    // that call checks if the current selection is inside the currently
    // displayed nodes. Thus, we have to calculate the display nodes first.
    model_.ComputeDisplayNodeIdsForDistilledTree();

    distillationsCompleted_++;
  }

  // If there's no distillable content on the active tree, allow child tree
  // content to be distilled. This is needed to distill content on pages with
  // a single root node containing an iframe that contains a tree with all
  // the page's content.
  model_.AllowChildTreeForActiveTree(model_.content_node_ids().empty());

  // Draw the selection in the side panel (if one exists in the main panel).
  if (!PostProcessSelection()) {
    // If a draw did not occur, make sure to draw. This will happen if there is
    // no main content selection when the tree is distilled. Sometimes in Gmail,
    // The above call to ComputeDisplayNodeIdsForDistilledTree still produces
    // an empty display node list. If that happens and there are content nodes,
    // we should recompute the display nodes again.
    bool should_recompute_display_nodes =
        !model_.content_node_ids().empty() &&
        model_.GetCurrentlyVisibleNodes()->empty();
    VLOG(1) << "In OnAXTreeDistilled content node size: "
            << model_.content_node_ids().size()
            << " and display node size: " << model_.display_node_ids().size()
            << " and selection node size: "
            << model_.selection_node_ids().size();
    Draw(should_recompute_display_nodes);
  }

  if (model_.is_empty()) {
    // For Google Docs, the initial AXTree may be empty while the document is
    // loading. Therefore, to avoid displaying an empty side panel, wait for
    // Google Docs to finish loading.
    if (!IsGoogleDocs() || model_.page_finished_loading()) {
      if (features::IsImmersiveReadAnythingEnabled()) {
        SetDistillationState(
            read_anything::mojom::ReadAnythingDistillationState::
                kDistillationEmpty);
      }
      DrawEmptyState();
    }
  } else {
    if (features::IsImmersiveReadAnythingEnabled()) {
      SetDistillationState(read_anything::mojom::ReadAnythingDistillationState::
                               kDistillationWithContent);
    }
  }

  // AXNode's language code is BCP 47. Only the base language is needed to
  // record the metric.
  std::string language = model_.GetActiveTree()->root()->GetLanguage();
  if (!language.empty()) {
    base::UmaHistogramSparse(
        "Accessibility.ReadAnything.Language",
        base::HashMetricName(language::ExtractBaseLanguage(language)));
  }

  if (model_.is_readability_next_distillation_method()) {
    return;
  }
  // Once drawing is complete, process pending updates on the active tree if
  // there are no other factors blocking the processing of updates
  ProcessPendingUpdatesIfAllowed();
}

bool ReadAnythingAppController::PostProcessSelection() {
  // It's possible for the active tree to be destroyed in-between when
  // OnAXTreeDistilled returns early if the model doesn't contain the active
  // tree and when PostProcessSelection is called after
  // ComputeDisplayNodeIdsForDistilledTree is called. This seems to happen
  // when it takes a long time to compute the display nodes. If this happens,
  // return false rather than trying to continue to process information on a
  // destroyed tree.
  DUMP_WILL_BE_CHECK(model_.ContainsActiveTree());
  if (!model_.ContainsActiveTree()) {
    return false;
  }
  bool did_draw = false;
  // Note post `model_.PostProcessSelection` returns true if a draw is required.
  if (model_.PostProcessSelection()) {
    did_draw = true;
    if (model_.is_empty()) {
      DrawEmptyState();
    } else {
      // TODO(b/40927698): When Read Aloud is playing and content is selected
      // in the main panel, don't re-draw with the updated selection until
      // Read Aloud is paused.
      bool should_recompute_display_nodes = !model_.content_node_ids().empty();
      VLOG(1) << "In PostProcessSelection content node size: "
              << model_.content_node_ids().size()
              << " and display node size: " << model_.display_node_ids().size()
              << " and selection node size: "
              << model_.selection_node_ids().size();
      Draw(should_recompute_display_nodes);
    }
  }
  // Skip drawing the selection in the side panel if the selection originally
  // came from there.
  if (model_.unprocessed_selections_from_reading_mode() == 0) {
    DrawSelection();
  } else {
    model_.decrement_selections_from_reading_mode();
  }
  return did_draw;
}

void ReadAnythingAppController::Draw(bool recompute_display_nodes) {
  // For Google Docs, do not show any text before the doc finishing loading.
  if (IsGoogleDocs() && !model_.page_finished_loading()) {
    return;
  }
  if (recompute_display_nodes && !model_.content_node_ids().empty()) {
    model_.ComputeDisplayNodeIdsForDistilledTree();

    // If we need to recompute which nodes are displayed, reset read aloud as
    // we previously preprocessed the previous nodes and should re-process the
    // new ones.
    read_aloud_model_.ResetReadAloudState();
  } else {
    VLOG(1) << "Not recomputing display nodes, content node size: "
            << model_.content_node_ids().size();
  }
  // This call should check that the active tree isn't in an undistilled state
  // -- that is, it is awaiting distillation or never requested distillation.
  ExecuteJavaScript("chrome.readingMode.updateContent();");
}

void ReadAnythingAppController::DrawSelection() {
  // Reset read aloud state if a selection has been made in case the selected
  // nodes weren't previously distilled. Resetting isn't necessary if the
  // selection nodes were included in the distilled content.
  if (!model_.selection_node_ids().empty() &&
      !model_.SelectionNodesContainedInDistilledContent()) {
    read_aloud_model_.ResetReadAloudState();
  }

  // This call should check that the active tree isn't in an undistilled state
  // -- that is, it is awaiting distillation or never requested distillation.
  ExecuteJavaScript("chrome.readingMode.updateSelection();");
}

void ReadAnythingAppController::DrawEmptyState() {
  ExecuteJavaScript("chrome.readingMode.showEmpty();");
}

void ReadAnythingAppController::LogEmptyState() {
  base::UmaHistogramEnumeration(ReadAnythingAppModel::kEmptyStateHistogramName,
                                ReadAnythingAppModel::EmptyState::kShown);
}

void ReadAnythingAppController::OnSettingsRestoredFromPrefs(
    read_anything::mojom::LineSpacing line_spacing,
    read_anything::mojom::LetterSpacing letter_spacing,
    const std::string& font,
    double font_size,
    bool links_enabled,
    bool images_enabled,
    read_anything::mojom::Colors color,
    double speech_rate,
    base::DictValue voices,
    base::ListValue languages_enabled_in_pref,
    read_anything::mojom::HighlightGranularity granularity,
    read_anything::mojom::LineFocus last_non_disabled_line_focus,
    bool line_focus_enabled) {
  read_aloud_model_.OnSettingsRestoredFromPrefs(
      speech_rate, &languages_enabled_in_pref, &voices, granularity);
  bool needs_redraw_for_links = model_.links_enabled() != links_enabled;
  model_.OnSettingsRestoredFromPrefs(
      line_spacing, letter_spacing, font, font_size, links_enabled,
      images_enabled, color, last_non_disabled_line_focus, line_focus_enabled);
  ExecuteJavaScript("chrome.readingMode.restoreSettingsFromPrefs();");
  // Only redraw if there is an active tree.
  if (needs_redraw_for_links &&
      model_.active_tree_id() != ui::AXTreeIDUnknown()) {
    ExecuteJavaScript("chrome.readingMode.updateLinks();");
  }
}

void ReadAnythingAppController::ScreenAIServiceReady() {
  if (features::IsDataCollectionModeForScreen2xEnabled()) {
    model_.SetScreenAIServiceReadyForDataCollection();
  }
  distiller_->ScreenAIServiceReady();
}

void ReadAnythingAppController::TogglePinState() {
  page_handler_->TogglePinState();
}

const gin::WrapperInfo* ReadAnythingAppController::wrapper_info() const {
  return &kWrapperInfo;
}

gin::ObjectTemplateBuilder ReadAnythingAppController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<ReadAnythingAppController>::GetObjectTemplateBuilder(
             isolate)
      .SetProperty("rootId", &ReadAnythingAppController::RootId)
      .SetProperty("startNodeId", &ReadAnythingAppController::StartNodeId)
      .SetProperty("startOffset", &ReadAnythingAppController::StartOffset)
      .SetProperty("endNodeId", &ReadAnythingAppController::EndNodeId)
      .SetProperty("endOffset", &ReadAnythingAppController::EndOffset)
      .SetProperty("fontName", &ReadAnythingAppController::FontName)
      .SetProperty("fontSize", &ReadAnythingAppController::FontSize)
      .SetProperty("linksEnabled", &ReadAnythingAppController::LinksEnabled)
      .SetProperty("imagesEnabled", &ReadAnythingAppController::ImagesEnabled)
      .SetProperty("imagesFeatureEnabled",
                   &ReadAnythingAppController::ImagesFeatureEnabled)
      .SetProperty("letterSpacing", &ReadAnythingAppController::LetterSpacing)
      .SetProperty("lineSpacing", &ReadAnythingAppController::LineSpacing)
      .SetProperty("standardLineSpacing",
                   &ReadAnythingAppController::StandardLineSpacing)
      .SetProperty("looseLineSpacing",
                   &ReadAnythingAppController::LooseLineSpacing)
      .SetProperty("veryLooseLineSpacing",
                   &ReadAnythingAppController::VeryLooseLineSpacing)
      .SetProperty("standardLetterSpacing",
                   &ReadAnythingAppController::StandardLetterSpacing)
      .SetProperty("wideLetterSpacing",
                   &ReadAnythingAppController::WideLetterSpacing)
      .SetProperty("veryWideLetterSpacing",
                   &ReadAnythingAppController::VeryWideLetterSpacing)
      .SetProperty("colorTheme", &ReadAnythingAppController::ColorTheme)
      .SetProperty("highlightGranularity",
                   &ReadAnythingAppController::HighlightGranularity)
      .SetProperty("lastNonDisabledLineFocus",
                   &ReadAnythingAppController::LastNonDisabledLineFocus)
      .SetProperty("isLineFocusOn", &ReadAnythingAppController::IsLineFocusOn)
      .SetProperty("defaultTheme", &ReadAnythingAppController::DefaultTheme)
      .SetProperty("lightTheme", &ReadAnythingAppController::LightTheme)
      .SetProperty("darkTheme", &ReadAnythingAppController::DarkTheme)
      .SetProperty("yellowTheme", &ReadAnythingAppController::YellowTheme)
      .SetProperty("blueTheme", &ReadAnythingAppController::BlueTheme)
      .SetProperty("highContrastTheme",
                   &ReadAnythingAppController::HighContrastTheme)
      .SetProperty("lowContrastLightTheme",
                   &ReadAnythingAppController::LowContrastLightTheme)
      .SetProperty("lowContrastDarkTheme",
                   &ReadAnythingAppController::LowContrastDarkTheme)
      .SetProperty("autoHighlighting",
                   &ReadAnythingAppController::AutoHighlighting)
      .SetProperty("wordHighlighting",
                   &ReadAnythingAppController::WordHighlighting)
      .SetProperty("phraseHighlighting",
                   &ReadAnythingAppController::PhraseHighlighting)
      .SetProperty("sentenceHighlighting",
                   &ReadAnythingAppController::SentenceHighlighting)
      .SetProperty("noHighlighting", &ReadAnythingAppController::NoHighlighting)
      .SetProperty("pauseButtonStopSource",
                   &ReadAnythingAppController::PauseButtonStopSource)
      .SetProperty("keyboardShortcutStopSource",
                   &ReadAnythingAppController::KeyboardShortcutStopSource)
      .SetProperty("engineInterruptStopSource",
                   &ReadAnythingAppController::EngineInterruptStopSource)
      .SetProperty("engineErrorStopSource",
                   &ReadAnythingAppController::EngineErrorStopSource)
      .SetProperty("contentFinishedStopSource",
                   &ReadAnythingAppController::ContentFinishedStopSource)
      .SetProperty("isSpeechTreeInitialized",
                   &ReadAnythingAppController::IsSpeechTreeInitialized)
      .SetProperty(
          "unexpectedUpdateContentStopSource",
          &ReadAnythingAppController::UnexpectedUpdateContentStopSource)
      .SetProperty("lineFocusOff", &ReadAnythingAppController::LineFocusOff)
      .SetProperty("lineFocusSmallStaticWindow",
                   &ReadAnythingAppController::LineFocusSmallStaticWindow)
      .SetProperty("lineFocusMediumStaticWindow",
                   &ReadAnythingAppController::LineFocusMediumStaticWindow)
      .SetProperty("lineFocusLargeStaticWindow",
                   &ReadAnythingAppController::LineFocusLargeStaticWindow)
      .SetProperty("lineFocusSmallCursorWindow",
                   &ReadAnythingAppController::LineFocusSmallCursorWindow)
      .SetProperty("lineFocusMediumCursorWindow",
                   &ReadAnythingAppController::LineFocusMediumCursorWindow)
      .SetProperty("lineFocusLargeCursorWindow",
                   &ReadAnythingAppController::LineFocusLargeCursorWindow)
      .SetProperty("lineFocusStaticLine",
                   &ReadAnythingAppController::LineFocusStaticLine)
      .SetProperty("lineFocusCursorLine",
                   &ReadAnythingAppController::LineFocusCursorLine)
      .SetProperty("maxLineWidth", &ReadAnythingAppController::MaxLineWidth)
      .SetProperty("inHiddenPresentationState",
                   &ReadAnythingAppController::InHiddenPresentationState)
      .SetProperty("inSidePanelPresentationState",
                   &ReadAnythingAppController::InSidePanelPresentationState)
      .SetProperty(
          "inImmersiveOverlayPresentationState",
          &ReadAnythingAppController::InImmersiveOverlayPresentationState)
      .SetProperty("speechRate", &ReadAnythingAppController::SpeechRate)
      .SetProperty("isGoogleDocs", &ReadAnythingAppController::IsGoogleDocs)
      .SetProperty("isImmersiveEnabled",
                   &ReadAnythingAppController::IsImmersiveEnabled)
      .SetProperty("isTsTextSegmentationEnabled",
                   &ReadAnythingAppController::IsTsTextSegmentationEnabled)
      .SetProperty("isReadabilityEnabled",
                   &ReadAnythingAppController::IsReadabilityEnabled)
      .SetProperty("activeDistillationMethod",
                   &ReadAnythingAppController::GetDistillationMethod)
      .SetProperty("isLineFocusEnabled",
                   &ReadAnythingAppController::IsLineFocusEnabled)
      .SetProperty("isReadabilityWithLinksEnabled",
                   &ReadAnythingAppController::IsReadabilityWithLinksEnabled)
      .SetProperty("isChromeOsAsh", &ReadAnythingAppController::IsChromeOsAsh)
      .SetProperty("baseLanguageForSpeech",
                   &ReadAnythingAppController::GetLanguageCodeForSpeech)
      .SetProperty("requiresDistillation",
                   &ReadAnythingAppController::RequiresDistillation)
      .SetProperty("defaultLanguageForSpeech",
                   &ReadAnythingAppController::GetDefaultLanguageCodeForSpeech)
      .SetProperty("isPhraseHighlightingEnabled",
                   &ReadAnythingAppController::IsPhraseHighlightingEnabled)
      .SetProperty("htmlTitle",
                   &ReadAnythingAppController::GetDomDistillerTitle)
      .SetProperty("htmlContent",
                   &ReadAnythingAppController::GetDomDistillerContentHtml)
      .SetProperty("axTreeAnchors",
                   &ReadAnythingAppController::GetDomDistillerAnchors)
      .SetProperty("distillationTypeScreen2x",
                   &ReadAnythingAppController::DistillationTypeScreen2x)
      .SetProperty("distillationTypeReadability",
                   &ReadAnythingAppController::DistillationTypeReadability)
      .SetMethod("isHighlightOn", &ReadAnythingAppController::IsHighlightOn)
      .SetMethod("getChildren", &ReadAnythingAppController::GetChildren)
      .SetMethod("getTextDirection",
                 &ReadAnythingAppController::GetTextDirection)
      .SetMethod("getHtmlTag", &ReadAnythingAppController::GetHtmlTag)
      .SetMethod("getLanguage", &ReadAnythingAppController::GetLanguage)
      .SetMethod("getTextContent", &ReadAnythingAppController::GetTextContent)
      .SetMethod("getPrefixText", &ReadAnythingAppController::GetPrefixText)
      .SetMethod("getUrl", &ReadAnythingAppController::GetUrl)
      .SetMethod("getAltText", &ReadAnythingAppController::GetAltText)
      .SetMethod("shouldBold", &ReadAnythingAppController::ShouldBold)
      .SetMethod("isOverline", &ReadAnythingAppController::IsOverline)
      .SetMethod("isLeafNode", &ReadAnythingAppController::IsLeafNode)
      .SetMethod("onConnected", &ReadAnythingAppController::OnConnected)
      .SetMethod("onCopy", &ReadAnythingAppController::OnCopy)
      .SetMethod("onNoTextContent", &ReadAnythingAppController::OnNoTextContent)
      .SetMethod("updateWordsSeen", &ReadAnythingAppController::UpdateWordsSeen)
      .SetMethod("logEmptyState", &ReadAnythingAppController::LogEmptyState)
      .SetMethod("updateWordsHeard",
                 &ReadAnythingAppController::UpdateWordsHeard)
      .SetMethod("onFontSizeChanged",
                 &ReadAnythingAppController::OnFontSizeChanged)
      .SetMethod("onFontSizeReset", &ReadAnythingAppController::OnFontSizeReset)
      .SetMethod("onLinksEnabledToggled",
                 &ReadAnythingAppController::OnLinksEnabledToggled)
      .SetMethod("onImagesEnabledToggled",
                 &ReadAnythingAppController::OnImagesEnabledToggled)
      .SetMethod("onScroll", &ReadAnythingAppController::OnScroll)
      .SetMethod("onLinkClicked", &ReadAnythingAppController::OnLinkClicked)
      .SetMethod("onLetterSpacingChange",
                 &ReadAnythingAppController::OnLetterSpacingChange)
      .SetMethod("onLineSpacingChange",
                 &ReadAnythingAppController::OnLineSpacingChange)
      .SetMethod("onThemeChange", &ReadAnythingAppController::OnThemeChange)
      .SetMethod("onFontChange", &ReadAnythingAppController::OnFontChange)
      .SetMethod("onSpeechRateChange",
                 &ReadAnythingAppController::OnSpeechRateChange)
      .SetMethod("getStoredVoice", &ReadAnythingAppController::GetStoredVoice)
      .SetMethod("onVoiceChange", &ReadAnythingAppController::OnVoiceChange)
      .SetMethod("logExtensionState",
                 &ReadAnythingAppController::LogExtensionState)
      .SetMethod("onLanguagePrefChange",
                 &ReadAnythingAppController::OnLanguagePrefChange)
      .SetMethod("getLanguagesEnabledInPref",
                 &ReadAnythingAppController::GetLanguagesEnabledInPref)
      .SetMethod("onHighlightGranularityChanged",
                 &ReadAnythingAppController::OnHighlightGranularityChanged)
      .SetMethod("onLineFocusChanged",
                 &ReadAnythingAppController::OnLineFocusChanged)
      .SetMethod("getLineSpacingValue",
                 &ReadAnythingAppController::GetLineSpacingValue)
      .SetMethod("getLetterSpacingValue",
                 &ReadAnythingAppController::GetLetterSpacingValue)
      .SetMethod("onSelectionChange",
                 &ReadAnythingAppController::OnSelectionChange)
      .SetMethod("onCollapseSelection",
                 &ReadAnythingAppController::OnCollapseSelection)
      .SetMethod("onDistilled", &ReadAnythingAppController::OnDistilled)
      .SetProperty("supportedFonts",
                   &ReadAnythingAppController::GetSupportedFonts)
      .SetProperty("allFonts", &ReadAnythingAppController::GetAllFonts)
      .SetMethod("setContentForTesting",
                 &ReadAnythingAppController::SetContentForTesting)
      .SetMethod("setAnchorsForTesting",
                 &ReadAnythingAppController::SetAnchorsForTesting)
      .SetMethod("setLanguageForTesting",
                 &ReadAnythingAppController::SetLanguageForTesting)
      .SetMethod("initAxPositionWithNode",
                 &ReadAnythingAppController::InitAXPositionWithNode)
      .SetMethod("resetGranularityIndex",
                 &ReadAnythingAppController::ResetGranularityIndex)
      .SetMethod("getCurrentTextContent",
                 &ReadAnythingAppController::GetCurrentTextContent)
      .SetMethod("shouldShowUi", &ReadAnythingAppController::ShouldShowUI)
      .SetMethod("onIsSpeechActiveChanged",
                 &ReadAnythingAppController::OnIsSpeechActiveChanged)
      .SetMethod("onIsAudioCurrentlyPlayingChanged",
                 &ReadAnythingAppController::OnIsAudioCurrentlyPlayingChanged)
      .SetMethod("getAccessibleBoundary",
                 &ReadAnythingAppController::GetAccessibleBoundary)
      .SetMethod("movePositionToNextGranularity",
                 &ReadAnythingAppController::MovePositionToNextGranularity)
      .SetMethod("movePositionToPreviousGranularity",
                 &ReadAnythingAppController::MovePositionToPreviousGranularity)
      .SetMethod("requestImageData",
                 &ReadAnythingAppController::RequestImageData)
      .SetMethod("getImageBitmap", &ReadAnythingAppController::GetImageBitmap)
      .SetMethod("getDisplayNameForLocale",
                 &ReadAnythingAppController::GetDisplayNameForLocale)
      .SetMethod("incrementMetricCount",
                 &ReadAnythingAppController::IncrementMetricCount)
      .SetMethod("logSpeechStop", &ReadAnythingAppController::LogSpeechStop)
      .SetMethod("startLineFocusSession",
                 &ReadAnythingAppController::StartLineFocusSession)
      .SetMethod("logLineFocusSession",
                 &ReadAnythingAppController::LogLineFocusSession)
      .SetMethod("addLineFocusScrollDistance",
                 &ReadAnythingAppController::AddLineFocusScrollDistance)
      .SetMethod("addLineFocusMouseDistance",
                 &ReadAnythingAppController::AddLineFocusMouseDistance)
      .SetMethod("incrementLineFocusKeyboardLines",
                 &ReadAnythingAppController::IncrementLineFocusKeyboardLines)
      .SetMethod("incrementLineFocusSpeechLines",
                 &ReadAnythingAppController::IncrementLineFocusSpeechLines)
      .SetMethod("sendGetVoicePackInfoRequest",
                 &ReadAnythingAppController::SendGetVoicePackInfoRequest)
      .SetMethod("sendInstallVoicePackRequest",
                 &ReadAnythingAppController::SendInstallVoicePackRequest)
      .SetMethod("sendUninstallVoiceRequest",
                 &ReadAnythingAppController::SendUninstallVoiceRequest)
      .SetMethod("getCurrentTextSegments",
                 &ReadAnythingAppController::GetCurrentTextSegments)
      .SetMethod("getHighlightForCurrentSegmentIndex",
                 &ReadAnythingAppController::GetHighlightForCurrentSegmentIndex)
      .SetMethod("getValidatedFontName",
                 &ReadAnythingAppController::GetValidatedFontName)
      .SetMethod("onScrolledToBottom",
                 &ReadAnythingAppController::OnScrolledToBottom)
      .SetProperty("isDocsLoadMoreButtonVisible",
                   &ReadAnythingAppController::IsDocsLoadMoreButtonVisible)
      .SetMethod("sendGetPresentationStateRequest",
                 &ReadAnythingAppController::SendGetPresentationStateRequest)
      .SetMethod("togglePresentation",
                 &ReadAnythingAppController::TogglePresentation)
      .SetMethod("close", &ReadAnythingAppController::CloseUI)
      .SetMethod("togglePinState", &ReadAnythingAppController::TogglePinState)
      .SetMethod("sendPinStateRequest",
                 &ReadAnythingAppController::SendPinStateRequest);
}

ui::AXNodeID ReadAnythingAppController::RootId() const {
  ui::AXSerializableTree* tree = model_.GetActiveTree();
  // Fail gracefully if RootId() is ever called with an invalid active tree.
  DUMP_WILL_BE_CHECK(tree);
  DUMP_WILL_BE_CHECK(tree->root());
  if (!tree || !tree->root()) {
    return ui::kInvalidAXNodeID;
  }
  return tree->root()->id();
}

ui::AXNodeID ReadAnythingAppController::StartNodeId() const {
  return model_.start_node_id();
}

int ReadAnythingAppController::StartOffset() const {
  return model_.start_offset();
}

ui::AXNodeID ReadAnythingAppController::EndNodeId() const {
  return model_.end_node_id();
}

int ReadAnythingAppController::EndOffset() const {
  return model_.end_offset();
}

std::string ReadAnythingAppController::FontName() const {
  return model_.font_name();
}

float ReadAnythingAppController::FontSize() const {
  return model_.font_size();
}

bool ReadAnythingAppController::LinksEnabled() const {
  return model_.links_enabled();
}

bool ReadAnythingAppController::ImagesEnabled() const {
  return model_.images_enabled();
}

bool ReadAnythingAppController::ImagesFeatureEnabled() const {
  return features::IsReadAnythingImagesViaAlgorithmEnabled();
}

bool ReadAnythingAppController::IsPhraseHighlightingEnabled() const {
  return features::IsReadAnythingReadAloudPhraseHighlightingEnabled();
}

void ReadAnythingAppController::OnPinStatusReceived(bool pin_state) {
  ExecuteJavaScript("chrome.readingMode.onPinStateReceived(" +
                    base::ToString(pin_state) + ")");
}

int ReadAnythingAppController::LetterSpacing() const {
  return std::to_underlying(model_.letter_spacing());
}

int ReadAnythingAppController::LineSpacing() const {
  return std::to_underlying(model_.line_spacing());
}

int ReadAnythingAppController::ColorTheme() const {
  return std::to_underlying(model_.color_theme());
}

double ReadAnythingAppController::SpeechRate() const {
  return read_aloud_model_.speech_rate();
}

std::string ReadAnythingAppController::GetStoredVoice() const {
  const std::string* const voice =
      read_aloud_model_.voices().FindString(model_.base_language_code());
  return voice ? *voice : std::string();
}

std::vector<std::string> ReadAnythingAppController::GetLanguagesEnabledInPref()
    const {
  std::vector<std::string> languages_enabled_in_pref;
  for (const base::Value& value :
       read_aloud_model_.languages_enabled_in_pref()) {
    languages_enabled_in_pref.push_back(value.GetString());
  }
  return languages_enabled_in_pref;
}

int ReadAnythingAppController::HighlightGranularity() const {
  return read_aloud_model_.highlight_granularity();
}

int ReadAnythingAppController::LastNonDisabledLineFocus() const {
  return IsLineFocusEnabled()
             ? std::to_underlying(model_.last_non_disabled_line_focus())
             : std::to_underlying(read_anything::mojom::LineFocus::kOff);
}

bool ReadAnythingAppController::IsLineFocusOn() const {
  return IsLineFocusEnabled() && model_.line_focus_enabled();
}

int ReadAnythingAppController::StandardLineSpacing() const {
  return std::to_underlying(read_anything::mojom::LineSpacing::kStandard);
}

int ReadAnythingAppController::LooseLineSpacing() const {
  return std::to_underlying(read_anything::mojom::LineSpacing::kLoose);
}

int ReadAnythingAppController::VeryLooseLineSpacing() const {
  return std::to_underlying(read_anything::mojom::LineSpacing::kVeryLoose);
}

int ReadAnythingAppController::StandardLetterSpacing() const {
  return std::to_underlying(read_anything::mojom::LetterSpacing::kStandard);
}

int ReadAnythingAppController::WideLetterSpacing() const {
  return std::to_underlying(read_anything::mojom::LetterSpacing::kWide);
}

int ReadAnythingAppController::VeryWideLetterSpacing() const {
  return std::to_underlying(read_anything::mojom::LetterSpacing::kVeryWide);
}

int ReadAnythingAppController::DefaultTheme() const {
  return std::to_underlying(read_anything::mojom::Colors::kDefault);
}

int ReadAnythingAppController::LightTheme() const {
  return std::to_underlying(read_anything::mojom::Colors::kLight);
}

int ReadAnythingAppController::DarkTheme() const {
  return std::to_underlying(read_anything::mojom::Colors::kDark);
}

int ReadAnythingAppController::YellowTheme() const {
  return std::to_underlying(read_anything::mojom::Colors::kYellow);
}

int ReadAnythingAppController::BlueTheme() const {
  return std::to_underlying(read_anything::mojom::Colors::kBlue);
}

int ReadAnythingAppController::HighContrastTheme() const {
  return std::to_underlying(read_anything::mojom::Colors::kHighContrast);
}

int ReadAnythingAppController::LowContrastLightTheme() const {
  return std::to_underlying(read_anything::mojom::Colors::kLowContrastLight);
}

int ReadAnythingAppController::LowContrastDarkTheme() const {
  return std::to_underlying(read_anything::mojom::Colors::kLowContrastDark);
}

bool ReadAnythingAppController::IsHighlightOn() {
  return read_aloud_model_.IsHighlightOn();
}

int ReadAnythingAppController::AutoHighlighting() const {
  return static_cast<int>(read_anything::mojom::HighlightGranularity::kOn);
}

int ReadAnythingAppController::WordHighlighting() const {
  return static_cast<int>(read_anything::mojom::HighlightGranularity::kWord);
}

int ReadAnythingAppController::PhraseHighlighting() const {
  return static_cast<int>(read_anything::mojom::HighlightGranularity::kPhrase);
}

int ReadAnythingAppController::SentenceHighlighting() const {
  return static_cast<int>(
      read_anything::mojom::HighlightGranularity::kSentence);
}

int ReadAnythingAppController::NoHighlighting() const {
  return static_cast<int>(read_anything::mojom::HighlightGranularity::kOff);
}

int ReadAnythingAppController::PauseButtonStopSource() const {
  return std::to_underlying(ReadAloudAppModel::ReadAloudStopSource::kButton);
}

int ReadAnythingAppController::KeyboardShortcutStopSource() const {
  return std::to_underlying(
      ReadAloudAppModel::ReadAloudStopSource::kKeyboardShortcut);
}

int ReadAnythingAppController::EngineInterruptStopSource() const {
  return std::to_underlying(
      ReadAloudAppModel::ReadAloudStopSource::kEngineInterrupt);
}

int ReadAnythingAppController::EngineErrorStopSource() const {
  return std::to_underlying(
      ReadAloudAppModel::ReadAloudStopSource::kEngineError);
}

int ReadAnythingAppController::ContentFinishedStopSource() const {
  return std::to_underlying(
      ReadAloudAppModel::ReadAloudStopSource::kFinishContent);
}

int ReadAnythingAppController::UnexpectedUpdateContentStopSource() const {
  return std::to_underlying(
      ReadAloudAppModel::ReadAloudStopSource::kUnexpectedUpdateContent);
}

int ReadAnythingAppController::LineFocusOff() const {
  return std::to_underlying(read_anything::mojom::LineFocus::kOff);
}

int ReadAnythingAppController::LineFocusSmallStaticWindow() const {
  return std::to_underlying(
      read_anything::mojom::LineFocus::kSmallStaticWindow);
}

int ReadAnythingAppController::LineFocusMediumStaticWindow() const {
  return std::to_underlying(
      read_anything::mojom::LineFocus::kMediumStaticWindow);
}

int ReadAnythingAppController::LineFocusLargeStaticWindow() const {
  return std::to_underlying(
      read_anything::mojom::LineFocus::kLargeStaticWindow);
}

int ReadAnythingAppController::LineFocusSmallCursorWindow() const {
  return std::to_underlying(
      read_anything::mojom::LineFocus::kSmallCursorWindow);
}

int ReadAnythingAppController::LineFocusMediumCursorWindow() const {
  return std::to_underlying(
      read_anything::mojom::LineFocus::kMediumCursorWindow);
}

int ReadAnythingAppController::LineFocusLargeCursorWindow() const {
  return std::to_underlying(
      read_anything::mojom::LineFocus::kLargeCursorWindow);
}

int ReadAnythingAppController::LineFocusStaticLine() const {
  return std::to_underlying(read_anything::mojom::LineFocus::kLineStatic);
}

int ReadAnythingAppController::LineFocusCursorLine() const {
  return std::to_underlying(read_anything::mojom::LineFocus::kLineCursor);
}

int ReadAnythingAppController::MaxLineWidth() const {
  return a11y::kMaxLineWidth;
}

int ReadAnythingAppController::InHiddenPresentationState() const {
  return std::to_underlying(
      read_anything::mojom::ReadAnythingPresentationState::kInactive);
}

int ReadAnythingAppController::InSidePanelPresentationState() const {
  return std::to_underlying(
      read_anything::mojom::ReadAnythingPresentationState::kInSidePanel);
}

int ReadAnythingAppController::InImmersiveOverlayPresentationState() const {
  return std::to_underlying(
      read_anything::mojom::ReadAnythingPresentationState::kInImmersiveOverlay);
}

int ReadAnythingAppController::DistillationTypeScreen2x() const {
  return static_cast<int>(ReadAnythingAppModel::DistillationMethod::kScreen2x);
}

int ReadAnythingAppController::DistillationTypeReadability() const {
  return static_cast<int>(
      ReadAnythingAppModel::DistillationMethod::kReadability);
}

std::vector<ui::AXNodeID> ReadAnythingAppController::GetChildren(
    ui::AXNodeID ax_node_id) const {
  std::vector<ui::AXNodeID> child_ids;
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);
  const std::set<ui::AXNodeID>* node_ids = model_.GetCurrentlyVisibleNodes();
  for (auto it = ax_node->UnignoredChildrenBegin();
       it != ax_node->UnignoredChildrenEnd(); ++it) {
    if (node_ids->contains(it->id())) {
      child_ids.push_back(it->id());
    }
  }
  return child_ids;
}

std::string ReadAnythingAppController::GetHtmlTag(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);

  return a11y::GetHtmlTag(ax_node, model_.is_pdf(), model_.IsDocs());
}

std::string ReadAnythingAppController::GetLanguage(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);
  if (model_.NodeIsContentNode(ax_node_id)) {
    return ax_node->GetLanguage();
  }
  return ax_node->GetStringAttribute(ax::mojom::StringAttribute::kLanguage);
}

std::u16string ReadAnythingAppController::GetTextContent(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DUMP_WILL_BE_CHECK(ax_node);
  if (!ax_node) {
    return std::u16string();
  }

  return a11y::GetTextContent(ax_node, model_.is_pdf(), IsGoogleDocs());
}

std::u16string ReadAnythingAppController::GetPrefixText(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DUMP_WILL_BE_CHECK(ax_node);
  if (!ax_node) {
    return std::u16string();
  }

  return a11y::GetPrefixText(ax_node, model_.is_pdf(), IsGoogleDocs());
}

std::string ReadAnythingAppController::GetTextDirection(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  if (!ax_node) {
    return std::string();
  }

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
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);
  const std::string& url =
      ax_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl);

  // Prevent XSS from href attribute, which could be set to a script instead
  // of a valid website.
  if (url::FindAndCompareScheme(url, "http", nullptr) ||
      url::FindAndCompareScheme(url, "https", nullptr)) {
    return url;
  }
  return "";
}

// TODO(crbug.com/463728166): Remove IsImmersiveReadAnythingEnabled flag when no
// longer flag-guarded code.
void ReadAnythingAppController::SendGetPresentationStateRequest() const {
  if (features::IsImmersiveReadAnythingEnabled()) {
    page_handler_->GetPresentationState();
  }
}

void ReadAnythingAppController::OnGetPresentationState(
    read_anything::mojom::ReadAnythingPresentationState presentation_state) {
  model_.set_active_presentation_state(presentation_state);
  // Now that the presentation state changed which is potentially one of the
  // factors blocking processing, see if we can unblock processing of the
  // updates.
  ProcessPendingUpdatesIfAllowed();
  ExecuteJavaScript("chrome.readingMode.onPresentationStateReceived(" +
                    base::ToString(static_cast<int>(presentation_state)) +
                    ");");
}

void ReadAnythingAppController::SendPinStateRequest() {
  page_handler_->SendPinStateRequest();
}

void ReadAnythingAppController::SendGetVoicePackInfoRequest(
    const std::string& language) const {
  page_handler_->GetVoicePackInfo(language);
}

void ReadAnythingAppController::OnGetVoicePackInfo(
    read_anything::mojom::VoicePackInfoPtr voice_pack_info) {
  std::string status =
      voice_pack_info->pack_state->is_installation_state()
          ? base::ToString(
                voice_pack_info->pack_state->get_installation_state())
          : base::ToString(voice_pack_info->pack_state->get_error_code());

  ExecuteJavaScript("chrome.readingMode.updateVoicePackStatus(\'" +
                    voice_pack_info->language + "\', \'" + status + "\');");
}

void ReadAnythingAppController::SendInstallVoicePackRequest(
    const std::string& language) const {
  page_handler_->InstallVoicePack(language);
}

void ReadAnythingAppController::SendUninstallVoiceRequest(
    const std::string& language) const {
  page_handler_->UninstallVoice(language);
}

std::string ReadAnythingAppController::GetAltText(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* node = model_.GetAXNode(ax_node_id);
  CHECK(node);
  return a11y::GetAltText(node);
}

bool ReadAnythingAppController::ShouldBold(ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);
  if (!ax_node) {
    return false;
  }
  bool is_bold = ax_node->HasTextStyle(ax::mojom::TextStyle::kBold);
  bool is_italic = ax_node->HasTextStyle(ax::mojom::TextStyle::kItalic);
  bool is_underline = ax_node->HasTextStyle(ax::mojom::TextStyle::kUnderline);
  return is_bold || is_italic || is_underline;
}

bool ReadAnythingAppController::IsOverline(ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);
  if (!ax_node) {
    return false;
  }
  return ax_node->HasTextStyle(ax::mojom::TextStyle::kOverline);
}

bool ReadAnythingAppController::IsLeafNode(ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);
  if (!ax_node) {
    return false;
  }
  return ax_node->IsLeaf();
}

bool ReadAnythingAppController::IsImmersiveEnabled() const {
  return features::IsImmersiveReadAnythingEnabled();
}

bool ReadAnythingAppController::IsTsTextSegmentationEnabled() const {
  return features::IsReadAnythingReadAloudTSTextSegmentationEnabled();
}

// Returns true if the experimental flag allowing testing with alternative
// distillation methods such as Readability.js is enabled.
bool ReadAnythingAppController::IsReadabilityEnabled() const {
  return features::IsReadAnythingWithReadabilityEnabled();
}

bool ReadAnythingAppController::IsLineFocusEnabled() const {
  return features::IsReadAnythingLineFocusEnabled();
}

bool ReadAnythingAppController::IsReadabilityWithLinksEnabled() const {
  return features::IsReadAnythingWithReadabilityAllowLinksEnabled();
}

bool ReadAnythingAppController::IsChromeOsAsh() const {
#if BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

bool ReadAnythingAppController::IsGoogleDocs() const {
  return model_.IsDocs();
}

std::vector<std::string> ReadAnythingAppController::GetSupportedFonts() {
  return model_.supported_fonts();
}

std::string ReadAnythingAppController::GetValidatedFontName(
    const std::string& font) const {
  if (!std::ranges::contains(GetAllFonts(), font)) {
    return GetAllFonts().front();
  }
  if (font == "Serif" || font == "Sans-serif") {
    return base::ToLowerASCII(font);
  }
  return font.contains(' ') ? base::StrCat({"\"", font, "\""}) : font;
}

std::vector<std::string> ReadAnythingAppController::GetAllFonts() const {
  return ::GetSupportedFonts({});
}

void ReadAnythingAppController::RequestImageData(ui::AXNodeID node_id) const {
  if (features::IsReadAnythingImagesViaAlgorithmEnabled()) {
    DUMP_WILL_BE_CHECK(model_.GetAXNode(node_id));
    auto target_tree_id = model_.active_tree_id();
    CHECK_NE(target_tree_id, ui::AXTreeIDUnknown());
    page_handler_->OnImageDataRequested(target_tree_id, node_id);
  }
}

void ReadAnythingAppController::OnImageDataDownloaded(
    const ui::AXTreeID& tree_id,
    ui::AXNodeID node_id,
    const SkBitmap& image) {
  // If the tree has changed since the request, do nothing with the downloaded
  // image.
  if (tree_id != model_.active_tree_id()) {
    return;
  }
  // Temporarily store the image so that javascript can fetch it.
  downloaded_images_[node_id] = image;
  // Notify javascript to fetch the image.
  ExecuteJavaScript("chrome.readingMode.onImageDownloaded(" +
                    base::ToString(node_id) + ")");
}

v8::Local<v8::Value> ReadAnythingAppController::GetImageBitmap(
    ui::AXNodeID node_id) {
  // Get the isolate for reading mode.
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();

  if (auto itr = downloaded_images_.find(node_id);
      itr != downloaded_images_.end()) {
    // Don't reference itr again.
    SkBitmap bitmap = std::move(itr->second);
    // Remove the downloaded image from the map.
    downloaded_images_.erase(node_id);
    // Ensure that the bitmap is in the correct color format.
    if (bitmap.colorType() != SkColorType::kRGBA_8888_SkColorType) {
      bitmap = CorrectColorOfBitMap(bitmap);
    }

    // Get the pixmap to compute the bytes.
    auto pixmap = std::move(bitmap.pixmap());
    auto size = pixmap.computeByteSize();
    // Create an array buffer with the image bytes.
    v8::Local<v8::ArrayBuffer> buffer = v8::ArrayBuffer::New(isolate, size);
    // Copy the memory in.
    pixmap.readPixels(pixmap.info(), buffer->GetBackingStore()->Data(),
                      pixmap.rowBytes());
    // Create a clamped array so we can create an ImageData object on the
    // javascript side.
    v8::Local<v8::Uint8ClampedArray> array =
        v8::Uint8ClampedArray::New(buffer, 0, size);

    // Create an object with the image data and height, as well as a scale
    // factor.
    ui::AXNode* node = model_.GetAXNode(node_id);
    CHECK(node);
    int width = bitmap.width();
    int height = bitmap.height();
    float scale = (node->data().relative_bounds.bounds.width()) / width;
    v8::Local<v8::Object> obj = v8::Object::New(isolate);
    auto created = obj->DefineOwnProperty(
        isolate->GetCurrentContext(),
        v8::String::NewFromUtf8(isolate, "data").ToLocalChecked(), array);
    created = obj->DefineOwnProperty(
        isolate->GetCurrentContext(),
        v8::String::NewFromUtf8(isolate, "width").ToLocalChecked(),
        v8::Number::New(isolate, width));
    created = obj->DefineOwnProperty(
        isolate->GetCurrentContext(),
        v8::String::NewFromUtf8(isolate, "height").ToLocalChecked(),
        v8::Number::New(isolate, height));
    created = obj->DefineOwnProperty(
        isolate->GetCurrentContext(),
        v8::String::NewFromUtf8(isolate, "scale").ToLocalChecked(),
        v8::Number::New(isolate, scale));

    return obj;
  }
  // If there wasn't an image, return undefined.
  return v8::Undefined(isolate);
}

const std::string ReadAnythingAppController::GetDisplayNameForLocale(
    const std::string& locale,
    const std::string& display_locale) const {
  bool found_valid_result = false;
  std::string locale_result;
  if (l10n_util::IsValidLocaleSyntax(locale) &&
      l10n_util::IsValidLocaleSyntax(display_locale)) {
    locale_result = base::UTF16ToUTF8(l10n_util::GetDisplayNameForLocale(
        locale, display_locale, /*is_for_ui=*/true));
    // Check for valid locales before getting the display name.
    // The ICU Locale class returns "und" for undetermined locales, and
    // returns the locale string directly if it has no translation.
    // Treat these cases as invalid results.
    found_valid_result =
        locale_result != kUndeterminedLocale && locale_result != locale;
  }

  // Return an empty string to communicate there's no display name.
  if (!found_valid_result) {
    locale_result = std::string();
  } else {
    locale_result[0] = std::toupper(locale_result[0]);
  }

  return locale_result;
}

const std::string& ReadAnythingAppController::GetLanguageCodeForSpeech() const {
  return model_.base_language_code();
}

int ReadAnythingAppController::GetDistillationMethod() const {
  return static_cast<int>(model_.current_content_distillation_method());
}

bool ReadAnythingAppController::RequiresDistillation() {
  // DOM distiller distillation doesn't queue distillations so return false.
  if (model_.is_readability_next_distillation_method()) {
    return false;
  }
  return model_.requires_distillation();
}

const std::string& ReadAnythingAppController::GetDefaultLanguageCodeForSpeech()
    const {
  return read_aloud_model_.default_language_code();
}

void ReadAnythingAppController::OnConnected() {
  // This needs to be logged here in the controller so we can base it off of
  // the controller's constructor time.
  base::UmaHistogramLongTimes(
      "Accessibility.ReadAnything.TimeFromEntryTriggeredToWebUIConnected",
      base::TimeTicks::Now() - renderer_load_triggered_time_ms_);
  mojo::PendingReceiver<read_anything::mojom::UntrustedPageHandlerFactory>
      page_handler_factory_receiver =
          page_handler_factory_.BindNewPipeAndPassReceiver();
  page_handler_factory_->CreateUntrustedPageHandler(
      receiver_.BindNewPipeAndPassRemote(),
      page_handler_.BindNewPipeAndPassReceiver());
  render_frame()->GetBrowserInterfaceBroker().GetInterface(
      std::move(page_handler_factory_receiver));

  // Get the dependency parser model used by phrase-based highlighting.
  if (read_aloud_model_.GetDependencyParserModel().IsAvailable()) {
    return;
  }

  page_handler_->GetDependencyParserModel(
      base::BindOnce(&ReadAnythingAppController::UpdateDependencyParserModel,
                     weak_ptr_factory_.GetWeakPtr()));
  SetDistillationState(
      read_anything::mojom::ReadAnythingDistillationState::kNotAttempted);
}

void ReadAnythingAppController::OnCopy() const {
  page_handler_->OnCopy();
}

void ReadAnythingAppController::OnNoTextContent() {
  if (model_.is_readability_next_distillation_method()) {
    return;
  }
  Distill();
}

void ReadAnythingAppController::OnDistilled(int word_count) {
  model_.set_words_distilled(word_count);
  if (model_.current_content_distillation_method() ==
      ReadAnythingAppModel::DistillationMethod::kReadability) {
    base::UmaHistogramCustomCounts(
        "Accessibility.ReadAnything.WordsDistilledByReadability", word_count, 1,
        kMaxWordsConsumed, kWordsConsumedBuckets);
  }
}

void ReadAnythingAppController::UpdateWordsSeen(int words_seen) {
  model_.set_words_seen(words_seen);
}

void ReadAnythingAppController::UpdateWordsHeard(int words_heard) {
  model_.set_words_heard(words_heard);
}

void ReadAnythingAppController::OnFontSizeChanged(bool increase) {
  model_.AdjustTextSize(increase ? 1 : -1);
  page_handler_->OnFontSizeChange(model_.font_size());
}

void ReadAnythingAppController::OnFontSizeReset() {
  model_.ResetTextSize();
  page_handler_->OnFontSizeChange(model_.font_size());
}

void ReadAnythingAppController::OnLinksEnabledToggled() {
  model_.set_links_enabled(!model_.links_enabled());
  page_handler_->OnLinksEnabledChanged(model_.links_enabled());
}

void ReadAnythingAppController::OnImagesEnabledToggled() {
  model_.set_images_enabled(!model_.images_enabled());
  page_handler_->OnImagesEnabledChanged(model_.images_enabled());
}

void ReadAnythingAppController::OnScroll(bool on_selection) const {
  model_.OnScroll(on_selection, /* from_reading_mode= */ true);
}

void ReadAnythingAppController::OnLinkClicked(ui::AXNodeID ax_node_id) const {
  DCHECK_NE(model_.active_tree_id(), ui::AXTreeIDUnknown());
  // Prevent link clicks while distillation is in progress, as it means that
  // the tree may have changed in an unexpected way.
  // TODO(crbug.com/40802192): Consider how to show this in a more
  // user-friendly way.
  if (model_.screen2x_distiller_running()) {
    return;
  }
  page_handler_->OnLinkClicked(model_.active_tree_id(), ax_node_id);
}

void ReadAnythingAppController::OnLetterSpacingChange(int value) {
  if (const auto maybe_enum =
          ToEnum<read_anything::mojom::LetterSpacing>(value)) {
    page_handler_->OnLetterSpaceChange(maybe_enum.value());
    model_.set_letter_spacing(maybe_enum.value());
  }
}

void ReadAnythingAppController::OnLineSpacingChange(int value) {
  if (const auto maybe_enum =
          ToEnum<read_anything::mojom::LineSpacing>(value)) {
    page_handler_->OnLineSpaceChange(maybe_enum.value());
    model_.set_line_spacing(maybe_enum.value());
  }
}

void ReadAnythingAppController::OnThemeChange(int value) {
  if (const auto maybe_enum = ToEnum<read_anything::mojom::Colors>(value)) {
    page_handler_->OnColorChange(maybe_enum.value());
    model_.set_color_theme(maybe_enum.value());
  }
}

void ReadAnythingAppController::OnFontChange(const std::string& font) {
  page_handler_->OnFontChange(font);
  model_.set_font_name(font);
}

void ReadAnythingAppController::OnSpeechRateChange(double rate) {
  page_handler_->OnSpeechRateChange(rate);
  read_aloud_model_.set_speech_rate(rate);
}

void ReadAnythingAppController::OnVoiceChange(const std::string& voice,
                                              const std::string& lang) {
  // Store the given voice with the base language. If the user prefers a voice
  // for a specific language, we should always use that voice, regardless of
  // the more specific locale. e.g. if the user prefers the en-UK voice for
  // English pages, use that voice even if the page is marked en-US.
  std::string base_lang = std::string(language::ExtractBaseLanguage(lang));
  page_handler_->OnVoiceChange(voice, base_lang);
  read_aloud_model_.SetVoice(voice, base_lang);
}

void ReadAnythingAppController::LogExtensionState() {
#if !BUILDFLAG(IS_CHROMEOS)
  page_handler_->LogExtensionState();
#endif
}

void ReadAnythingAppController::OnLanguagePrefChange(const std::string& lang,
                                                     bool enabled) {
  page_handler_->OnLanguagePrefChange(lang, enabled);
  read_aloud_model_.SetLanguageEnabled(lang, enabled);
}

void ReadAnythingAppController::OnHighlightGranularityChanged(
    const int granularity) {
  page_handler_->OnHighlightGranularityChanged(
      static_cast<read_anything::mojom::HighlightGranularity>(granularity));
  read_aloud_model_.set_highlight_granularity(granularity);
}

void ReadAnythingAppController::OnLineFocusChanged(int line_focus) {
  if (!IsLineFocusEnabled()) {
    return;
  }

  if (const auto maybe_enum =
          ToEnum<read_anything::mojom::LineFocus>(line_focus)) {
    page_handler_->OnLineFocusChanged(maybe_enum.value());
    bool line_focus_on =
        maybe_enum.value() != read_anything::mojom::LineFocus::kOff;
    model_.set_line_focus_enabled(line_focus_on);
    if (line_focus_on) {
      model_.set_last_non_disabled_line_focus(maybe_enum.value());
    }
  }
}

double ReadAnythingAppController::GetLineSpacingValue(int line_spacing) const {
  using read_anything::mojom::LineSpacing;
  static constexpr auto kEnumToValue =
      base::MakeFixedFlatMap<LineSpacing, double>({
          {LineSpacing::kTightDeprecated, 1.0},
          // This value needs to be at least 1.35 to avoid cutting off
          // descenders with the highlight with larger fonts such as Poppins.
          {LineSpacing::kStandard, 1.35},
          {LineSpacing::kLoose, 1.5},
          {LineSpacing::kVeryLoose, 2.0},
      });
  return kEnumToValue.at(
      ToEnum<LineSpacing>(line_spacing).value_or(LineSpacing::kDefaultValue));
}

double ReadAnythingAppController::GetLetterSpacingValue(
    int letter_spacing) const {
  using read_anything::mojom::LetterSpacing;
  static constexpr auto kEnumToValue =
      base::MakeFixedFlatMap<LetterSpacing, double>({
          {LetterSpacing::kTightDeprecated, -0.05},
          {LetterSpacing::kStandard, 0},
          {LetterSpacing::kWide, 0.05},
          {LetterSpacing::kVeryWide, 0.1},
      });
  return kEnumToValue.at(ToEnum<LetterSpacing>(letter_spacing)
                             .value_or(LetterSpacing::kDefaultValue));
}

void ReadAnythingAppController::OnSelectionChange(ui::AXNodeID anchor_node_id,
                                                  int anchor_offset,
                                                  ui::AXNodeID focus_node_id,
                                                  int focus_offset) {
  DCHECK_NE(model_.active_tree_id(), ui::AXTreeIDUnknown());
  // Prevent link clicks while distillation is in progress, as it means that
  // the tree may have changed in an unexpected way.
  // TODO(crbug.com/40802192): Consider how to show this in a more
  // user-friendly way.
  if (model_.screen2x_distiller_running()) {
    return;
  }

  // Ignore the new selection if it's collapsed, which is created by a simple
  // click, unless there was a previous selection, in which case the click
  // clears the selection, so we should tell the main page to clear too.
  if ((anchor_offset == focus_offset) && (anchor_node_id == focus_node_id)) {
    if (model_.has_selection()) {
      model_.increment_selections_from_reading_mode();
      OnCollapseSelection();
    }
    return;
  }

  ui::AXNode* focus_node = model_.GetAXNode(focus_node_id);
  ui::AXNode* anchor_node = model_.GetAXNode(anchor_node_id);
  if (!focus_node || !anchor_node) {
    // Sometimes when the side panel size is adjusted, a focus or anchor node
    // may be null. Return early if this happens.
    return;
  }
  // Some text fields, like Gmail, allow a <div> to be returned as a focus
  // node for selection, most frequently when a triple click causes an entire
  // range of text to be selected, including non-text nodes. This can cause
  // inconsistencies in how the selection is handled. e.g. the focus node can
  // be before the anchor node and set to a non-text node, which can cause
  // page_handler_->OnSelectionChange to be incorrectly triggered, resulting
  // in a failing DCHECK. Therefore, return early if this happens. This check
  // does not apply to pdfs.
  if (!model_.is_pdf() && (!focus_node->IsText() || !anchor_node->IsText())) {
    return;
  }

  // If the selection change matches the tree's selection, this means it was
  // set by the controller. Javascript selections set by the controller are
  // always forward selections. This means the anchor node always comes before
  // the focus node.
  if (anchor_node_id == model_.start_node_id() &&
      anchor_offset == model_.start_offset() &&
      focus_node_id == model_.end_node_id() &&
      focus_offset == model_.end_offset()) {
    return;
  }

  model_.increment_selections_from_reading_mode();
  page_handler_->OnSelectionChange(model_.active_tree_id(), anchor_node_id,
                                   anchor_offset, focus_node_id, focus_offset);
}

void ReadAnythingAppController::OnCollapseSelection() const {
  if (model_.is_pdf()) {
    // CollapseSelection does nothing in pdfs, so just set an empty selection
    // instead.
    page_handler_->OnSelectionChange(
        model_.active_tree_id(), model_.start_node_id(), model_.start_offset(),
        model_.start_node_id(), model_.start_offset());
  } else {
    page_handler_->OnCollapseSelection();
  }
}
void ReadAnythingAppController::ResetGranularityIndex() {
  read_aloud_model_.ResetGranularityIndex();
}
void ReadAnythingAppController::InitAXPositionWithNode(
    const ui::AXNodeID& starting_node_id) {
  ui::AXNode* ax_node = model_.GetAXNode(starting_node_id);
  read_aloud_model_.InitAXPositionWithNode(ax_node, model_.active_tree_id());
  // TODO: crbug.com/411198154: This should only be called if the ax position
  // is not already initialized.
  PreprocessTextForSpeech();
}

bool ReadAnythingAppController::IsSpeechTreeInitialized() {
  return read_aloud_model_.speech_tree_initialized();
}

std::u16string ReadAnythingAppController::GetCurrentTextContent() {
  return read_aloud_model_
      .GetCurrentText(model_.is_pdf(), model_.IsDocs(),
                      model_.GetCurrentlyVisibleNodes())
      .text;
}

void ReadAnythingAppController::PreprocessTextForSpeech() {
  read_aloud_model_.PreprocessTextForSpeech(model_.is_pdf(), model_.IsDocs(),
                                            model_.GetCurrentlyVisibleNodes());
}

void ReadAnythingAppController::MovePositionToNextGranularity() {
  read_aloud_model_.MovePositionToNextGranularity();
}

void ReadAnythingAppController::MovePositionToPreviousGranularity() {
  read_aloud_model_.MovePositionToPreviousGranularity();
}

void ReadAnythingAppController::SetLanguageForTesting(
    const std::string& language_code) {
  SetLanguageCode(language_code);
}

void ReadAnythingAppController::SetLanguageCode(const std::string& code) {
  if (code.empty()) {
    model_.set_requires_tree_lang(true);
    return;
  }
  std::string base_lang = std::string(language::ExtractBaseLanguage(code));
  model_.SetBaseLanguageCode(base_lang);

  ExecuteJavaScript("chrome.readingMode.languageChanged();");
}

#if BUILDFLAG(IS_CHROMEOS)
void ReadAnythingAppController::OnDeviceLocked() {
  if (read_aloud_model_.speech_playing()) {
    read_aloud_model_.LogSpeechStop(
        ReadAloudAppModel::ReadAloudStopSource::kLockChromeosDevice);
  }
  LogLineFocusSession();
  RecordEstimatedWordsSeen();
  RecordEstimatedWordsHeard();
  // Signal to the WebUI that the device has been locked. We'll only receive
  // this callback on ChromeOS.
  ExecuteJavaScript("chrome.readingMode.onLockScreen();");
}
#else
void ReadAnythingAppController::OnTtsEngineInstalled() {
  VLOG(1) << "OnTtsEngineInstalled";
  ExecuteJavaScript("chrome.readingMode.onTtsEngineInstalled()");
}
#endif

void ReadAnythingAppController::OnReadingModeHidden(bool tab_active) {
  page_handler_->AckReadingModeHidden();
  model_.set_will_hide(true);

  // If the tab is not active but RM is hidden, then the tab was switched and
  // speech was not stopped. If the tab is still active and RM is hidden, then
  // RM was closed, so log the speech stopped.
  if (tab_active) {
    ReadingModeWillClose();
    if (read_aloud_model_.speech_playing()) {
      read_aloud_model_.LogSpeechStop(
          ReadAloudAppModel::ReadAloudStopSource::kCloseReadingMode);
    }
  }
  LogLineFocusSession();
  RecordEstimatedWordsSeen();
  RecordEstimatedWordsHeard();
}

void ReadAnythingAppController::OnTabWillDetach() {
  model_.set_will_hide(true);
  if (read_aloud_model_.speech_playing()) {
    read_aloud_model_.LogSpeechStop(
        ReadAloudAppModel::ReadAloudStopSource::kCloseTabOrWindow);
    ReadingModeWillClose();
  }
  LogLineFocusSession();
  RecordEstimatedWordsSeen();
  RecordEstimatedWordsHeard();
}

void ReadAnythingAppController::ReadingModeWillClose() {
  if (!features::IsImmersiveReadAnythingEnabled()) {
    return;
  }

  ExecuteJavaScript("chrome.readingMode.readingModeWillClose();");
}

void ReadAnythingAppController::CloseUI() {
  // This CloseUI() method is only used for the immersive UI, so skip if flag is
  // not enabled
  if (!features::IsImmersiveReadAnythingEnabled()) {
    return;
  }
  page_handler_->CloseUI();
}

void ReadAnythingAppController::TogglePresentation() {
  if (!features::IsImmersiveReadAnythingEnabled()) {
    return;
  }
  page_handler_->TogglePresentation();
}

void ReadAnythingAppController::OnTabMuteStateChange(bool muted) {
  ExecuteJavaScript("chrome.readingMode.onTabMuteStateChange(" +
                    base::ToString(muted) + ")");
}

void ReadAnythingAppController::SetDefaultLanguageCode(
    const std::string& code) {
  std::string default_lang = std::string(language::ExtractBaseLanguage(code));
  // If the default language code is empty, continue to use the default
  // language code, as defined by ReadAnythingAppModel, currently 'en'
  if (default_lang.length() > 0) {
    read_aloud_model_.set_default_language_code(default_lang);
  }
}

void ReadAnythingAppController::SetContentForTesting(
    v8::Local<v8::Value> v8_snapshot_lite,
    std::vector<ui::AXNodeID> content_node_ids) {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  ui::AXTreeUpdate snapshot =
      GetSnapshotFromV8SnapshotLite(isolate, v8_snapshot_lite);
  ui::AXEvent selection_event;
  selection_event.event_type = ax::mojom::Event::kDocumentSelectionChanged;
  selection_event.event_from = ax::mojom::EventFrom::kUser;
  AccessibilityEventReceived(snapshot.tree_data.tree_id, {snapshot}, {});
  OnActiveAXTreeIDChanged(snapshot.tree_data.tree_id, ukm::kInvalidSourceId,
                          false);
  OnAXTreeDistilled(snapshot.tree_data.tree_id, content_node_ids);

  // Trigger a selection event (for testing selections).
  AccessibilityEventReceived(snapshot.tree_data.tree_id, {snapshot},
                             {selection_event});
}

void ReadAnythingAppController::SetAnchorsForTesting(
    v8::Local<v8::Value> v8_snapshot_lite,
    std::vector<ui::AXNodeID> content_node_ids) {
  SetContentForTesting(v8_snapshot_lite, content_node_ids);
  model_.set_should_extract_anchors_from_tree_for_readability(true);
  model_.ProcessAXTreeAnchors();
}

void ReadAnythingAppController::ShouldShowUI() {
  page_handler_factory_->ShouldShowUI();
}

void ReadAnythingAppController::OnIsSpeechActiveChanged(bool is_speech_active) {
  // Don't send event updates if the speech playing state hasn't actually
  // changed. This can get triggered incorrectly when changing pages.
  if (read_aloud_model_.speech_playing() == is_speech_active) {
    return;
  }
  read_aloud_model_.SetSpeechPlaying(is_speech_active);

  // If speech was just stopped, we can now process any updates that were
  // queued while speech was playing if there are no other factors blocking
  // processing
  ProcessPendingUpdatesIfAllowed();
}

void ReadAnythingAppController::OnIsAudioCurrentlyPlayingChanged(
    bool is_audio_currently_playing) {
  if (read_aloud_model_.audio_currently_playing() ==
      is_audio_currently_playing) {
    return;
  }
  read_aloud_model_.SetAudioCurrentlyPlaying(is_audio_currently_playing);
  page_handler_->OnReadAloudAudioStateChange(is_audio_currently_playing);
}

int ReadAnythingAppController::GetAccessibleBoundary(const std::u16string& text,
                                                     int max_text_length) {
  std::vector<int> offsets;
  const std::u16string shorter_string = text.substr(0, max_text_length);
  size_t sentence_ends_short = ui::FindAccessibleTextBoundary(
      shorter_string, offsets, ax::mojom::TextBoundary::kSentenceStart, 0,
      ax::mojom::MoveDirection::kForward,
      ax::mojom::TextAffinity::kDefaultValue);
  size_t sentence_ends_long = ui::FindAccessibleTextBoundary(
      text, offsets, ax::mojom::TextBoundary::kSentenceStart, 0,
      ax::mojom::MoveDirection::kForward,
      ax::mojom::TextAffinity::kDefaultValue);

  // Compare the index result for the sentence of maximum text length and of
  // the longer text string. If the two values are the same, the index is
  // correct. If they are different, the maximum text length may have
  // incorrectly spliced a word (e.g. returned "this is a sen" instead of
  // "this is a" or "this is a sentence"), so if this is the case, we'll want
  // to use the last word boundary instead.
  if (sentence_ends_short == sentence_ends_long) {
    return sentence_ends_short;
  }

  size_t word_ends = ui::FindAccessibleTextBoundary(
      shorter_string, offsets, ax::mojom::TextBoundary::kWordStart,
      shorter_string.length() - 1, ax::mojom::MoveDirection::kBackward,
      ax::mojom::TextAffinity::kDefaultValue);
  return word_ends;
}

v8::Local<v8::Value> ReadAnythingAppController::GetCurrentTextSegments() {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  auto context = isolate->GetCurrentContext();

  std::vector<ReadAloudTextSegment> nodes =
      read_aloud_model_.GetCurrentTextSegments(
          model_.is_pdf(), model_.IsDocs(), model_.GetCurrentlyVisibleNodes());

  v8::Local<v8::Array> highlight_array = v8::Array::New(isolate, nodes.size());
  for (int i = 0; i < (int)nodes.size(); i++) {
    v8::Local<v8::Object> obj = v8::Object::New(isolate);
    auto checked = obj->DefineOwnProperty(
        context, v8::String::NewFromUtf8(isolate, "nodeId").ToLocalChecked(),
        v8::Number::New(isolate, nodes[i].id));
    checked = obj->DefineOwnProperty(
        context, v8::String::NewFromUtf8(isolate, "start").ToLocalChecked(),
        v8::Number::New(isolate, nodes[i].text_start));
    checked = obj->DefineOwnProperty(
        context, v8::String::NewFromUtf8(isolate, "length").ToLocalChecked(),
        v8::Number::New(isolate, (nodes[i].text_end - nodes[i].text_start)));
    checked = highlight_array->Set(isolate->GetCurrentContext(), i, obj);
  }
  return highlight_array;
}

v8::Local<v8::Value>
ReadAnythingAppController::GetHighlightForCurrentSegmentIndex(int index,
                                                              bool phrases) {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  auto context = isolate->GetCurrentContext();

  std::vector<ReadAloudTextSegment> nodes =
      read_aloud_model_.GetHighlightForCurrentSegmentIndex(index, phrases);

  v8::Local<v8::Array> highlight_array = v8::Array::New(isolate, nodes.size());
  for (int i = 0; i < (int)nodes.size(); i++) {
    v8::Local<v8::Object> obj = v8::Object::New(isolate);
    auto checked = obj->DefineOwnProperty(
        context, v8::String::NewFromUtf8(isolate, "nodeId").ToLocalChecked(),
        v8::Number::New(isolate, nodes[i].id));
    checked = obj->DefineOwnProperty(
        context, v8::String::NewFromUtf8(isolate, "start").ToLocalChecked(),
        v8::Number::New(isolate, nodes[i].text_start));
    checked = obj->DefineOwnProperty(
        context, v8::String::NewFromUtf8(isolate, "length").ToLocalChecked(),
        v8::Number::New(isolate, (nodes[i].text_end - nodes[i].text_start)));
    checked = highlight_array->Set(isolate->GetCurrentContext(), i, obj);
  }
  return highlight_array;
}

void ReadAnythingAppController::IncrementMetricCount(
    const std::string& metric) {
  read_aloud_model_.IncrementMetric(metric);
}

void ReadAnythingAppController::LogSpeechStop(int source) {
  // Don't log speech stopping if the reading mode panel is going to hide. That
  // case is logged separately.
  if (model_.will_hide()) {
    return;
  }

  if (const auto maybe_enum =
          ToEnum<ReadAloudAppModel::ReadAloudStopSource>(source)) {
    read_aloud_model_.LogSpeechStop(maybe_enum.value());
  }
}

void ReadAnythingAppController::StartLineFocusSession() {
  if (IsLineFocusEnabled()) {
    model_.set_line_focus_session_start_time(base::TimeTicks::Now());
  }
}

void ReadAnythingAppController::LogLineFocusSession() {
  if (IsLineFocusEnabled() &&
      model_.line_focus_session_start_time().has_value()) {
    base::UmaHistogramLongTimes(
        "Accessibility.ReadAnything.LineFocusSessionLength",
        base::TimeTicks::Now() -
            model_.line_focus_session_start_time().value());
    base::UmaHistogramCounts1M(
        "Accessibility.ReadAnything.LineFocusSessionMouseDistance",
        model_.line_focus_mouse_distance());
    base::UmaHistogramCounts1M(
        "Accessibility.ReadAnything.LineFocusSessionScrollDistance",
        model_.line_focus_scroll_distance());
    base::UmaHistogramCounts100000(
        "Accessibility.ReadAnything.LineFocusSessionKeyboardLines",
        model_.line_focus_keyboard_lines());
    base::UmaHistogramCounts100000(
        "Accessibility.ReadAnything.LineFocusSessionSpeechLines",
        model_.line_focus_speech_lines());
    model_.ResetLineFocusSession();
  }
}

void ReadAnythingAppController::AddLineFocusScrollDistance(int distance) {
  if (IsLineFocusEnabled()) {
    model_.set_line_focus_scroll_distance(model_.line_focus_scroll_distance() +
                                          distance);
  }
}

void ReadAnythingAppController::AddLineFocusMouseDistance(int distance) {
  if (IsLineFocusEnabled()) {
    model_.set_line_focus_mouse_distance(model_.line_focus_mouse_distance() +
                                         distance);
  }
}

void ReadAnythingAppController::IncrementLineFocusKeyboardLines() {
  if (IsLineFocusEnabled()) {
    model_.set_line_focus_keyboard_lines(model_.line_focus_keyboard_lines() +
                                         1);
  }
}

void ReadAnythingAppController::IncrementLineFocusSpeechLines() {
  if (IsLineFocusEnabled()) {
    model_.set_line_focus_speech_lines(model_.line_focus_speech_lines() + 1);
  }
}

void ReadAnythingAppController::OnUrlInformationSet() {
  read_aloud_model_.LogSpeechStop(
      model_.IsReload() ? ReadAloudAppModel::ReadAloudStopSource::kReloadPage
                        : ReadAloudAppModel::ReadAloudStopSource::kChangePage);
}

void ReadAnythingAppController::OnScrolledToBottom() {
  if (IsGoogleDocs()) {
    // Scroll to the last display node shown on the Reading Mode side panel
    // TODO (b/356935604): Investigate optimal scroll position
    page_handler_->ScrollToTargetNode(
        model_.active_tree_id(), *model_.GetCurrentlyVisibleNodes()->rbegin());
  }
}

bool ReadAnythingAppController::IsDocsLoadMoreButtonVisible() const {
  return (features::IsReadAnythingDocsLoadMoreButtonEnabled() &&
          IsGoogleDocs());
}

void ReadAnythingAppController::UpdateDependencyParserModel(
    base::File model_file) {
  read_aloud_model_.GetDependencyParserModel().UpdateWithFile(
      std::move(model_file));
}

DependencyParserModel&
ReadAnythingAppController::GetDependencyParserModelForTesting() {
  return read_aloud_model_.GetDependencyParserModel();
}

void ReadAnythingAppController::OnTreeAdded(ui::AXTree* tree) {
  auto observation =
      std::make_unique<base::ScopedObservation<ui::AXTree, ui::AXTreeObserver>>(
          this);
  observation->Observe(tree);
  tree_observers_.push_back(std::move(observation));
}

void ReadAnythingAppController::OnTreeRemoved(ui::AXTree* tree) {
  auto it = std::ranges::find_if(tree_observers_,
                                 [tree](const auto& observation) -> bool {
                                   return observation->GetSource() == tree;
                                 });
  if (it != tree_observers_.end()) {
    tree_observers_.erase(it);
  }
}

std::string ReadAnythingAppController::GetDomDistillerTitle() const {
  return dom_distiller_title_;
}

std::string ReadAnythingAppController::GetDomDistillerContentHtml() const {
  return dom_distiller_content_html_;
}

v8::Local<v8::Value> ReadAnythingAppController::GetDomDistillerAnchors() const {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (!IsReadabilityEnabled() || !IsReadabilityWithLinksEnabled() || !isolate) {
    return v8::Undefined(isolate);
  }

  v8::EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    return v8::Undefined(isolate);
  }

  v8::Local<v8::Object> result_obj = v8::Object::New(isolate);
  auto anchors = model_.ax_tree_anchors();

  for (const auto& [url, link_data_list] : anchors) {
    v8::Local<v8::Array> v8_array =
        v8::Array::New(isolate, static_cast<int>(link_data_list.size()));
    for (size_t i = 0; i < link_data_list.size(); ++i) {
      const auto& data = link_data_list[i];
      v8::Local<v8::Object> link_obj = v8::Object::New(isolate);
      gin::Dictionary link_dict(isolate, link_obj);
      link_dict.Set("axId", data.id);

      if (!data.html_id.empty()) {
        link_dict.Set("htmlId", data.html_id);
      }
      if (!data.target.empty()) {
        link_dict.Set("target", data.target);
      }
      if (!data.title.empty()) {
        link_dict.Set("title", data.title);
      }
      if (!data.name.empty()) {
        link_dict.Set("text", data.name);
      }
      if (!data.text_before.empty()) {
        link_dict.Set("textBefore", data.text_before);
      }
      if (!data.text_after.empty()) {
        link_dict.Set("textAfter", data.text_after);
      }

      v8_array->Set(context, static_cast<uint32_t>(i), link_obj).Check();
    }
    result_obj->Set(context, gin::StringToV8(isolate, url), v8_array).Check();
  }

  return handle_scope.Escape(result_obj);
}

void ReadAnythingAppController::UpdateContent(const std::string& title,
                                              const std::string& content) {
  if (!features::IsReadAnythingWithReadabilityEnabled()) {
    return;
  }
  dom_distiller_title_ = title;
  dom_distiller_content_html_ = content;

  // Readability distillation uses the DOM and Google docs rendering is
  // canvas-based instead of DOM, so display empty.
  if (IsGoogleDocs()) {
    DrawEmptyState();
    return;
  }

  // If readability distillation returns empty content, consider distillation as
  // failure and default to Screen2X distillation.
  if (dom_distiller_content_html_.empty()) {
    // TODO(crbug.com/477090618): Record Readability failure metric.
    model_.set_next_distillation_method(
        ReadAnythingAppModel::DistillationMethod::kScreen2x);

    // Only attempt distillation if we have an active tree, otherwise only
    // update the model and wait for event to distill.
    model_.set_requires_distillation(true);
    if (model_.ContainsActiveTree()) {
      DistillNewTree();
    }
    return;
  }

  // Set both active and target distillation to readability since distillation
  // was successful.
  model_.set_next_distillation_method(
      ReadAnythingAppModel::DistillationMethod::kReadability);
  model_.set_current_content_distillation_method(
      ReadAnythingAppModel::DistillationMethod::kReadability);
  ExecuteJavaScript("chrome.readingMode.updateContent();");

  if (IsReadabilityWithLinksEnabled()) {
    model_.set_should_extract_anchors_from_tree_for_readability(true);
    bool didProcessAnchors = model_.ProcessAXTreeAnchors();
    if (didProcessAnchors) {
      ExecuteJavaScript("chrome.readingMode.onAnchorsReadyForReadability();");
    }
  }
}

void ReadAnythingAppController::OnReadabilityDistillationStateChanged(
    read_anything::mojom::ReadAnythingDistillationState new_state) {
  // Readability distillation happens in the browser
  // (ReadAnythingUntrustedPageHandler). This notification triggers a
  // "ping-pong" flow to keep logic synchronized: it updates the renderer's
  // model, which then circles back to the ReadAnythingUntrustedPageHandler to
  // update the distillation state in the ReadAnythingController.
  SetDistillationState(new_state);
}
