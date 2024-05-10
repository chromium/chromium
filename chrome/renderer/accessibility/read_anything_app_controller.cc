// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_app_controller.h"

#include <climits>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/renderer/accessibility/ax_tree_distiller.h"
#include "components/language/core/common/locale_util.h"
#include "components/translate/core/common/translate_constants.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "read_anything_app_controller.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node.h"
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
#include "v8/include/v8-context.h"
#include "v8/include/v8-microtask-queue.h"

using read_anything::mojom::ReadAnythingTheme;
using read_anything::mojom::ReadAnythingThemePtr;

namespace {

constexpr char kUndeterminedLocale[] = "und";

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

}  // namespace

// static
gin::WrapperInfo ReadAnythingAppController::kWrapperInfo = {
    gin::kEmbedderNativeGin};

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
      new ReadAnythingAppController(render_frame);
  gin::Handle<ReadAnythingAppController> handle =
      gin::CreateHandle(isolate, controller);
  if (handle.IsEmpty()) {
    return nullptr;
  }

  v8::Local<v8::Object> chrome =
      content::GetOrCreateChromeObject(isolate, context);
  chrome->Set(context, gin::StringToV8(isolate, "readingMode"), handle.ToV8())
      .Check();
  return controller;
}

ReadAnythingAppController::ReadAnythingAppController(
    content::RenderFrame* render_frame)
    : frame_token_(render_frame->GetWebFrame()->GetLocalFrameToken()) {
  renderer_load_triggered_time_ms_ = base::TimeTicks::Now();
  distiller_ = std::make_unique<AXTreeDistiller>(
      base::BindRepeating(&ReadAnythingAppController::OnAXTreeDistilled,
                          weak_ptr_factory_.GetWeakPtr()));
  // TODO(crbug.com/40915547): Use a global ukm recorder instance instead.
  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
  content::RenderThread::Get()->BindHostReceiver(
      factory.BindNewPipeAndPassReceiver());
  ukm_recorder_ = ukm::MojoUkmRecorder::Create(*factory);
}

ReadAnythingAppController::~ReadAnythingAppController() {
  RecordNumSelections();
  LogSpeechEventCounts();
}

void ReadAnythingAppController::AccessibilityEventReceived(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    const std::vector<ui::AXEvent>& events) {
  // This updates the model, which may require us to start distillation based on
  // the `requires_distillation()` state below.
  //
  // Remove the const-ness of the data here so that subsequent methods can move
  // the data.
  model_.AccessibilityEventReceived(
      tree_id, const_cast<std::vector<ui::AXTreeUpdate>&>(updates),
      const_cast<std::vector<ui::AXEvent>&>(events));
  // From this point onward, `updates` and `events` should not be accessed.

  if (tree_id != model_.active_tree_id()) {
    return;
  }

  if (model_.requires_distillation()) {
    Distill();
    if (model_.is_empty() && IsGoogleDocs() && model_.page_finished_loading()) {
      ExecuteJavaScript("chrome.readingMode.showEmpty();");
    }
  }

  if (model_.image_to_update_node_id() != ui::kInvalidAXNodeID) {
    ExecuteJavaScript("chrome.readingMode.updateImage(" +
                      base::ToString(model_.image_to_update_node_id()) + ");");
    model_.reset_image_to_update_node_id();
  }

  // TODO(accessibility): it isn't clear this handles the pending updates path
  // correctly within the model.
  if (model_.requires_post_process_selection()) {
    PostProcessSelection();
  }
}

void ReadAnythingAppController::ExecuteJavaScript(const std::string& script) {
  content::RenderFrame* render_frame = GetRenderFrame();
  if (!render_frame) {
    return;
  }
  // TODO(b/1266555): Use v8::Function rather than javascript. If possible,
  // replace this function call with firing an event.
  render_frame->ExecuteJavaScript(base::ASCIIToUTF16(script));
}

void ReadAnythingAppController::OnActiveAXTreeIDChanged(
    const ui::AXTreeID& tree_id,
    ukm::SourceId ukm_source_id,
    bool is_pdf) {
  if (tree_id == model_.active_tree_id() && !is_pdf) {
    return;
  }
  RecordNumSelections();
  model_.set_active_tree_id(tree_id);
  model_.set_ukm_source_id(ukm_source_id);
  model_.set_is_pdf(is_pdf);
  // Delete all pending updates on the formerly active AXTree.
  // TODO(crbug.com/40802192): If distillation is in progress, cancel the
  // distillation request.
  model_.ClearPendingUpdates();
  model_.set_requires_distillation(false);
  model_.set_page_finished_loading(false);

  ExecuteJavaScript("chrome.readingMode.showLoading();");

  // When the UI first constructs, this function may be called before tree_id
  // has been added to the tree list in AccessibilityEventReceived. In that
  // case, do not distill.
  if (model_.active_tree_id() != ui::AXTreeIDUnknown() &&
      model_.ContainsTree(model_.active_tree_id())) {
    Distill();
  }
}

void ReadAnythingAppController::RecordNumSelections() {
  ukm::builders::Accessibility_ReadAnything_EmptyState(
      model_.ukm_source_id())
      .SetTotalNumSelections(model_.num_selections())
      .Record(ukm_recorder_.get());
  model_.set_num_selections(0);
}

void ReadAnythingAppController::OnAXTreeDestroyed(const ui::AXTreeID& tree_id) {
  model_.OnAXTreeDestroyed(tree_id);
}

void ReadAnythingAppController::Distill() {
  if (model_.distillation_in_progress() || model_.speech_playing()) {
    // When distillation is in progress, the model may have queued up tree
    // updates. In those cases, assume we eventually get to `OnAXTreeDistilled`,
    // where we re-request `Distill`. When speech is playing, assume it will
    // eventually stop and call `OnSpeechPlayingStateChanged` where we
    // re-request `Distill`.
    model_.set_requires_distillation(true);
    return;
  }

  // For screen2x data generation mode, chrome is open from the CLI to a
  // specific URL. The caller monitors for a dump of the distilled proto written
  // to a local file. Distill should only be called once the page is finished
  // loading, so we have the proto representing the entire webpage.
  if (features::IsDataCollectionModeForScreen2xEnabled() &&
      !model_.page_finished_loading_for_data_collection()) {
    return;
  }

  model_.set_requires_distillation(false);

  ui::AXSerializableTree* tree = model_.GetTreeFromId(model_.active_tree_id());
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
  CHECK(serializer.SerializeChanges(tree->root(), &snapshot));
  model_.SetDistillationInProgress(true);
  distiller_->Distill(*tree, snapshot, model_.ukm_source_id());
}

void ReadAnythingAppController::OnAXTreeDistilled(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  // If speech is playing, we don't want to redraw and disrupt speech. We will
  // re-distill once speech pauses.
  if (model_.speech_playing()) {
    model_.set_requires_distillation(true);
    model_.SetDistillationInProgress(false);
    return;
  }
  // Reset state, including the current side panel selection so we can update
  // it based on the new main panel selection in PostProcessSelection below.
  // This also includes Read Aloud state.
  model_.Reset(content_node_ids);

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
    return;
  }
  if (!model_.content_node_ids().empty()) {
    // If there are content_node_ids, this means the AXTree was successfully
    // distilled.
    model_.ComputeDisplayNodeIdsForDistilledTree();
  }

  // Draw the selection in the side panel (if one exists in the main panel).
  if (!PostProcessSelection()) {
    // If a draw did not occur, make sure to draw. This will happen if there is
    // no main content selection when the tree is distilled.
    Draw();
  }

  if (model_.is_empty()) {
    // For Google Docs, the initial AXTree may be empty while the document is
    // loading. Therefore, to avoid displaying an empty side panel, wait for
    // Google Docs to finish loading.
    if (!IsGoogleDocs() || model_.page_finished_loading()) {
      ExecuteJavaScript("chrome.readingMode.showEmpty();");
      base::UmaHistogramEnumeration(string_constants::kEmptyStateHistogramName,
                                    ReadAnythingEmptyState::kEmptyStateShown);
    }
  }

  // AXNode's language code is BCP 47. Only the base language is needed to
  // record the metric.
  std::string language =
      model_.GetTreeFromId(model_.active_tree_id())->root()->GetLanguage();
  if (!language.empty()) {
    base::UmaHistogramSparse(
        string_constants::kLanguageHistogramName,
        base::HashMetricName(language::ExtractBaseLanguage(language)));
  }

  // Once drawing is complete, unserialize all of the pending updates on the
  // active tree which may require more distillations (as tracked by the model's
  // `requires_distillation()` state below).
  model_.UnserializePendingUpdates(tree_id);
  if (model_.requires_distillation()) {
    Distill();
  } else {
    // Request a screenshot of the active page when no more distillations are
    // required.
    if (model_.page_finished_loading_for_data_collection()) {
      // Send a snapshot request to its browser controller using `PaintPreview`
      // to take a whole-page snapshot of the active web contents.
      page_handler_->OnSnapshotRequested();
      model_.set_page_finished_loading_for_data_collection(false);
    }
  }
}

bool ReadAnythingAppController::PostProcessSelection() {
  bool did_draw = false;
  if (model_.PostProcessSelection()) {
    did_draw = true;
    // TODO(b/40927698): When Read Aloud is playing and content is selected
    // in the main panel, don't re-draw with the updated selection until
    // Read Aloud is paused.
    Draw();
  }
  // Skip drawing the selection in the side panel if the selection originally
  // came from there.
  if (!model_.selection_from_action()) {
    DrawSelection();
  }
  model_.set_selection_from_action(false);
  return did_draw;
}

void ReadAnythingAppController::Draw() {
  // This call should check that the active tree isn't in an undistilled state
  // -- that is, it is awaiting distillation or never requested distillation.
  ExecuteJavaScript("chrome.readingMode.updateContent();");
}

void ReadAnythingAppController::DrawSelection() {
  // This call should check that the active tree isn't in an undistilled state
  // -- that is, it is awaiting distillation or never requested distillation.
  ExecuteJavaScript("chrome.readingMode.updateSelection();");
}

void ReadAnythingAppController::OnThemeChanged(ReadAnythingThemePtr new_theme) {
  bool needs_redraw_for_links =
      model_.links_enabled() != new_theme->links_enabled;
  model_.OnThemeChanged(std::move(new_theme));
  ExecuteJavaScript("chrome.readingMode.updateTheme();");

  // Only redraw if there is an active tree.
  if (needs_redraw_for_links &&
      model_.active_tree_id() != ui::AXTreeIDUnknown()) {
    ExecuteJavaScript("chrome.readingMode.updateLinks();");
  }
}

void ReadAnythingAppController::OnSettingsRestoredFromPrefs(
    read_anything::mojom::LineSpacing line_spacing,
    read_anything::mojom::LetterSpacing letter_spacing,
    const std::string& font,
    double font_size,
    bool links_enabled,
    read_anything::mojom::Colors color,
    double speech_rate,
    base::Value::Dict voices,
    base::Value::List languages_enabled_in_pref,
    read_anything::mojom::HighlightGranularity granularity) {
  bool needs_redraw_for_links = model_.links_enabled() != links_enabled;
  model_.OnSettingsRestoredFromPrefs(
      line_spacing, letter_spacing, font, font_size, links_enabled, color,
      speech_rate, &voices,
      &languages_enabled_in_pref, granularity);
  ExecuteJavaScript("chrome.readingMode.restoreSettingsFromPrefs();");
  // Only redraw if there is an active tree.
  if (needs_redraw_for_links &&
      model_.active_tree_id() != ui::AXTreeIDUnknown()) {
    ExecuteJavaScript("chrome.readingMode.updateLinks();");
  }
}

void ReadAnythingAppController::ScreenAIServiceReady() {
  distiller_->ScreenAIServiceReady(GetRenderFrame());
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
      .SetProperty("backgroundColor",
                   &ReadAnythingAppController::BackgroundColor)
      .SetProperty("fontName", &ReadAnythingAppController::FontName)
      .SetProperty("fontSize", &ReadAnythingAppController::FontSize)
      .SetProperty("linksEnabled", &ReadAnythingAppController::LinksEnabled)
      .SetProperty("foregroundColor",
                   &ReadAnythingAppController::ForegroundColor)
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
      .SetProperty("highlightOn", &ReadAnythingAppController::HighlightOn)
      .SetProperty("defaultTheme", &ReadAnythingAppController::DefaultTheme)
      .SetProperty("lightTheme", &ReadAnythingAppController::LightTheme)
      .SetProperty("darkTheme", &ReadAnythingAppController::DarkTheme)
      .SetProperty("yellowTheme", &ReadAnythingAppController::YellowTheme)
      .SetProperty("blueTheme", &ReadAnythingAppController::BlueTheme)
      .SetProperty("speechRate", &ReadAnythingAppController::SpeechRate)
      .SetProperty("isWebUIToolbarVisible",
                   &ReadAnythingAppController::IsWebUIToolbarEnabled)
      .SetProperty("isGoogleDocs", &ReadAnythingAppController::IsGoogleDocs)
      .SetProperty("isReadAloudEnabled",
                   &ReadAnythingAppController::IsReadAloudEnabled)
      .SetProperty("isChromeOsAsh", &ReadAnythingAppController::IsChromeOsAsh)
      .SetProperty("isAutoVoiceSwitchingEnabled",
                   &ReadAnythingAppController::IsAutoVoiceSwitchingEnabled)
      .SetProperty("isLanguagePackDownloadingEnabled",
                   &ReadAnythingAppController::IsLanguagePackDownloadingEnabled)
      .SetProperty(
          "isAutomaticWordHighlightingEnabled",
          &ReadAnythingAppController::IsAutomaticWordHighlightingEnabled)
      .SetProperty("baseLanguageForSpeech",
                   &ReadAnythingAppController::GetLanguageCodeForSpeech)
      .SetProperty("requiresDistillation",
                   &ReadAnythingAppController::RequiresDistillation)
      .SetProperty("defaultLanguageForSpeech",
                   &ReadAnythingAppController::GetDefaultLanguageCodeForSpeech)
      .SetMethod("getChildren", &ReadAnythingAppController::GetChildren)
      .SetMethod("getDataFontCss", &ReadAnythingAppController::GetDataFontCss)
      .SetMethod("getTextDirection",
                 &ReadAnythingAppController::GetTextDirection)
      .SetMethod("getHtmlTag", &ReadAnythingAppController::GetHtmlTag)
      .SetMethod("getLanguage", &ReadAnythingAppController::GetLanguage)
      .SetMethod("getTextContent", &ReadAnythingAppController::GetTextContent)
      .SetMethod("getUrl", &ReadAnythingAppController::GetUrl)
      .SetMethod("getAltText", &ReadAnythingAppController::GetAltText)
      .SetMethod("shouldBold", &ReadAnythingAppController::ShouldBold)
      .SetMethod("isOverline", &ReadAnythingAppController::IsOverline)
      .SetMethod("isLeafNode", &ReadAnythingAppController::IsLeafNode)
      .SetMethod("onConnected", &ReadAnythingAppController::OnConnected)
      .SetMethod("onCopy", &ReadAnythingAppController::OnCopy)
      .SetMethod("onFontSizeChanged",
                 &ReadAnythingAppController::OnFontSizeChanged)
      .SetMethod("onFontSizeReset", &ReadAnythingAppController::OnFontSizeReset)
      .SetMethod("onLinksEnabledToggled",
                 &ReadAnythingAppController::OnLinksEnabledToggled)
      .SetMethod("onScroll", &ReadAnythingAppController::OnScroll)
      .SetMethod("onLinkClicked", &ReadAnythingAppController::OnLinkClicked)
      .SetMethod("onStandardLineSpacing",
                 &ReadAnythingAppController::OnStandardLineSpacing)
      .SetMethod("onLooseLineSpacing",
                 &ReadAnythingAppController::OnLooseLineSpacing)
      .SetMethod("onVeryLooseLineSpacing",
                 &ReadAnythingAppController::OnVeryLooseLineSpacing)
      .SetMethod("onStandardLetterSpacing",
                 &ReadAnythingAppController::OnStandardLetterSpacing)
      .SetMethod("onWideLetterSpacing",
                 &ReadAnythingAppController::OnWideLetterSpacing)
      .SetMethod("onVeryWideLetterSpacing",
                 &ReadAnythingAppController::OnVeryWideLetterSpacing)
      .SetMethod("onLightTheme", &ReadAnythingAppController::OnLightTheme)
      .SetMethod("onDefaultTheme", &ReadAnythingAppController::OnDefaultTheme)
      .SetMethod("onDarkTheme", &ReadAnythingAppController::OnDarkTheme)
      .SetMethod("onYellowTheme", &ReadAnythingAppController::OnYellowTheme)
      .SetMethod("onBlueTheme", &ReadAnythingAppController::OnBlueTheme)
      .SetMethod("onFontChange", &ReadAnythingAppController::OnFontChange)
      .SetMethod("onSpeechRateChange",
                 &ReadAnythingAppController::OnSpeechRateChange)
      .SetMethod("getStoredVoice", &ReadAnythingAppController::GetStoredVoice)
      .SetMethod("onVoiceChange", &ReadAnythingAppController::OnVoiceChange)
      .SetMethod("onLanguagePrefChange",
                 &ReadAnythingAppController::OnLanguagePrefChange)
      .SetMethod("getLanguagesEnabledInPref",
                 &ReadAnythingAppController::GetLanguagesEnabledInPref)
      .SetMethod("turnedHighlightOn",
                 &ReadAnythingAppController::TurnedHighlightOn)
      .SetMethod("turnedHighlightOff",
                 &ReadAnythingAppController::TurnedHighlightOff)
      .SetMethod("getLineSpacingValue",
                 &ReadAnythingAppController::GetLineSpacingValue)
      .SetMethod("getLetterSpacingValue",
                 &ReadAnythingAppController::GetLetterSpacingValue)
      .SetMethod("onSelectionChange",
                 &ReadAnythingAppController::OnSelectionChange)
      .SetMethod("onCollapseSelection",
                 &ReadAnythingAppController::OnCollapseSelection)
      .SetProperty("supportedFonts",
                   &ReadAnythingAppController::GetSupportedFonts)
      .SetMethod("setContentForTesting",
                 &ReadAnythingAppController::SetContentForTesting)
      .SetMethod("setThemeForTesting",
                 &ReadAnythingAppController::SetThemeForTesting)
      .SetMethod("setLanguageForTesting",
                 &ReadAnythingAppController::SetLanguageForTesting)
      .SetMethod("initAxPositionWithNode",
                 &ReadAnythingAppController::InitAXPositionWithNode)
      .SetMethod("getCurrentTextStartIndex",
                 &ReadAnythingAppController::GetCurrentTextStartIndex)
      .SetMethod("getCurrentTextEndIndex",
                 &ReadAnythingAppController::GetCurrentTextEndIndex)
      .SetMethod("getCurrentText", &ReadAnythingAppController::GetCurrentText)
      .SetMethod("shouldShowUi", &ReadAnythingAppController::ShouldShowUI)
      .SetMethod("onSpeechPlayingStateChanged",
                 &ReadAnythingAppController::OnSpeechPlayingStateChanged)
      .SetMethod("getAccessibleBoundary",
                 &ReadAnythingAppController::GetAccessibleBoundary)
      .SetMethod("movePositionToNextGranularity",
                 &ReadAnythingAppController::MovePositionToNextGranularity)
      .SetMethod("movePositionToPreviousGranularity",
                 &ReadAnythingAppController::MovePositionToPreviousGranularity)
      .SetMethod("requestImageDataUrl",
                 &ReadAnythingAppController::RequestImageDataUrl)
      .SetMethod("getImageDataUrl", &ReadAnythingAppController::GetImageDataUrl)
      .SetMethod("getDisplayNameForLocale",
                 &ReadAnythingAppController::GetDisplayNameForLocale)
      .SetMethod("logMetric", &ReadAnythingAppController::LogUmaHistogramTimes)
      .SetMethod("logLongMetric",
                 &ReadAnythingAppController::LogUmaHistogramLongTimes)
      .SetMethod("incrementMetricCount",
                 &ReadAnythingAppController::IncrementMetricCount)
      .SetMethod("logSpeechError",
                 &ReadAnythingAppController::LogSpeechErrorEvent)
      .SetMethod("sendGetVoicePackInfoRequest",
                 &ReadAnythingAppController::SendGetVoicePackInfoRequest)
      .SetMethod("sendInstallVoicePackRequest",
                 &ReadAnythingAppController::SendInstallVoicePackRequest)
      .SetMethod("getNodeIdForCurrentSegmentIndex",
                 &ReadAnythingAppController::GetNodeIdForCurrentSegmentIndex)
      .SetMethod("getNextWordHighlightLength",
                 &ReadAnythingAppController::GetNextWordHighlightLength);
}

ui::AXNodeID ReadAnythingAppController::RootId() const {
  ui::AXSerializableTree* tree = model_.GetTreeFromId(model_.active_tree_id());
  DCHECK(tree->root());
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

SkColor ReadAnythingAppController::BackgroundColor() const {
  return model_.background_color();
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

SkColor ReadAnythingAppController::ForegroundColor() const {
  return model_.foreground_color();
}

float ReadAnythingAppController::LetterSpacing() const {
  return model_.letter_spacing();
}

float ReadAnythingAppController::LineSpacing() const {
  return model_.line_spacing();
}

int ReadAnythingAppController::ColorTheme() const {
  return model_.color_theme();
}

float ReadAnythingAppController::SpeechRate() const {
  return model_.speech_rate();
}

std::string ReadAnythingAppController::GetStoredVoice() const {
  if (features::IsReadAloudAutoVoiceSwitchingEnabled()) {
    std::string lang = model_.base_language_code();
    if (model_.voices().contains(lang)) {
      return *model_.voices().FindString(lang);
    }
  } else {
    if (!model_.voices().empty()) {
      return model_.voices().begin()->second.GetString();
    }
  }

  return string_constants::kReadAnythingPlaceholderVoiceName;
}

std::vector<std::string> ReadAnythingAppController::GetLanguagesEnabledInPref()
    const {
  std::vector<std::string> languages_enabled_in_pref;

  for (const base::Value& value : model_.languages_enabled_in_pref()) {
    languages_enabled_in_pref.push_back(value.GetString());
  }
  return languages_enabled_in_pref;
}

int ReadAnythingAppController::HighlightGranularity() const {
  return model_.highlight_granularity();
}

int ReadAnythingAppController::StandardLineSpacing() const {
  return static_cast<int>(read_anything::mojom::LineSpacing::kStandard);
}

int ReadAnythingAppController::LooseLineSpacing() const {
  return static_cast<int>(read_anything::mojom::LineSpacing::kLoose);
}

int ReadAnythingAppController::VeryLooseLineSpacing() const {
  return static_cast<int>(read_anything::mojom::LineSpacing::kVeryLoose);
}

int ReadAnythingAppController::StandardLetterSpacing() const {
  return static_cast<int>(read_anything::mojom::LetterSpacing::kStandard);
}

int ReadAnythingAppController::WideLetterSpacing() const {
  return static_cast<int>(read_anything::mojom::LetterSpacing::kWide);
}

int ReadAnythingAppController::VeryWideLetterSpacing() const {
  return static_cast<int>(read_anything::mojom::LetterSpacing::kVeryWide);
}

int ReadAnythingAppController::DefaultTheme() const {
  return static_cast<int>(read_anything::mojom::Colors::kDefault);
}

int ReadAnythingAppController::LightTheme() const {
  return static_cast<int>(read_anything::mojom::Colors::kLight);
}

int ReadAnythingAppController::DarkTheme() const {
  return static_cast<int>(read_anything::mojom::Colors::kDark);
}

int ReadAnythingAppController::YellowTheme() const {
  return static_cast<int>(read_anything::mojom::Colors::kYellow);
}

int ReadAnythingAppController::BlueTheme() const {
  return static_cast<int>(read_anything::mojom::Colors::kBlue);
}

int ReadAnythingAppController::HighlightOn() const {
  return static_cast<int>(read_anything::mojom::HighlightGranularity::kOn);
}

std::vector<ui::AXNodeID> ReadAnythingAppController::GetChildren(
    ui::AXNodeID ax_node_id) const {
  std::vector<ui::AXNodeID> child_ids;
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);
  const std::set<ui::AXNodeID>* node_ids = model_.selection_node_ids().empty()
                                               ? &model_.display_node_ids()
                                               : &model_.selection_node_ids();
  for (auto it = ax_node->UnignoredChildrenBegin();
       it != ax_node->UnignoredChildrenEnd(); ++it) {
    if (base::Contains(*node_ids, it->id())) {
      child_ids.push_back(it->id());
    }
  }
  return child_ids;
}

std::string ReadAnythingAppController::GetDataFontCss(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);

  std::string data_font_css;
  ax_node->GetHtmlAttribute("data-font-css", &data_font_css);
  return data_font_css;
}

std::string ReadAnythingAppController::GetHtmlTag(
    ui::AXNodeID ax_node_id) const {
  return model_.GetHtmlTag(ax_node_id);
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

std::string ReadAnythingAppController::GetNameAttributeText(
    ui::AXNode* ax_node) const {
  DCHECK(ax_node);
  std::string node_text;
  if (ax_node->HasStringAttribute(ax::mojom::StringAttribute::kName)) {
    node_text = ax_node->GetStringAttribute(ax::mojom::StringAttribute::kName);
  }

  for (auto it = ax_node->UnignoredChildrenBegin();
       it != ax_node->UnignoredChildrenEnd(); ++it) {
    if (node_text.empty()) {
      node_text = GetNameAttributeText(it.get());
    } else {
      node_text += " " + GetNameAttributeText(it.get());
    }
  }
  return node_text;
}

std::string ReadAnythingAppController::GetTextContent(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);
  // For Google Docs, because the content is rendered in canvas, we distill
  // text from the "Annotated Canvas"
  // (https://sites.google.com/corp/google.com/docs-canvas-migration/home)
  // instead of the HTML.
  if (IsGoogleDocs()) {
    // With 'Annotated Canvas', text is stored within the aria-labels of SVG
    // elements. To retrieve this text, we need to access the 'name' attribute
    // of these elements.
    if ((ax_node->GetTextContentUTF8()).empty()) {
      std::string nodeText = GetNameAttributeText(ax_node);
      if (!nodeText.empty()) {
        // Add a space between the text of two annotated canvas elements.
        // Otherwise, there is no space separating two lines of text.
        return nodeText + " ";
      }
    } else {
      // We ignore all text in the HTML. These text are either from comments or
      // from off-screen divs that contain hidden information information that
      // only is intended for screen readers and braille support. These are not
      // actual text in the doc.
      // TODO(b/324143642): Reading Mode handles Doc comments.
      if (ax_node->GetRole() == ax::mojom::Role::kStaticText) {
        return "";
      }
    }
  }
  return ax_node->GetTextContentUTF8();
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
  const char* url =
      ax_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl).c_str();

  // Prevent XSS from href attribute, which could be set to a script instead
  // of a valid website.
  if (url::FindAndCompareScheme(url, static_cast<int>(strlen(url)), "http",
                                nullptr) ||
      url::FindAndCompareScheme(url, static_cast<int>(strlen(url)), "https",
                                nullptr)) {
    return url;
  }
  return "";
}

void ReadAnythingAppController::SendGetVoicePackInfoRequest(
    const std::string& language) const {
  page_handler_->GetVoicePackInfo(
      language,
      base::BindOnce(&ReadAnythingAppController::OnGetVoicePackInfoResponse,
                     weak_ptr_factory_.GetSafeRef()));
}

void ReadAnythingAppController::OnGetVoicePackInfoResponse(
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
  page_handler_->InstallVoicePack(
      language,
      base::BindOnce(&ReadAnythingAppController::OnInstallVoicePackResponse,
                     weak_ptr_factory_.GetSafeRef()));
}

void ReadAnythingAppController::OnInstallVoicePackResponse(
    read_anything::mojom::VoicePackInfoPtr voice_pack_info) {
  // TODO (b/40927698) Investigate the fact that VoicePackManager doesn't return
  // the expected pack_state. Even when a voice is unavailable and not
  // installed, it responds "INSTALLED" in the InstallVoicePackCallback. So we
  // probably need to rely on GetVoicePackInfo for the pack_state.

  std::string status =
      voice_pack_info->pack_state->is_installation_state()
          ? base::ToString(
                voice_pack_info->pack_state->get_installation_state())
          : base::ToString(voice_pack_info->pack_state->get_error_code());

  ExecuteJavaScript(
      "chrome.readingMode.updateVoicePackStatusFromInstallResponse(\'" +
      voice_pack_info->language + "\', \'" + status + "\');");
}

std::string ReadAnythingAppController::GetAltText(
    ui::AXNodeID ax_node_id) const {
  return model_.GetAltText(ax_node_id);
}

bool ReadAnythingAppController::ShouldBold(ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);
  bool is_bold = ax_node->HasTextStyle(ax::mojom::TextStyle::kBold);
  bool is_italic = ax_node->HasTextStyle(ax::mojom::TextStyle::kItalic);
  bool is_underline = ax_node->HasTextStyle(ax::mojom::TextStyle::kUnderline);
  return is_bold || is_italic || is_underline;
}

bool ReadAnythingAppController::IsOverline(ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);
  return ax_node->HasTextStyle(ax::mojom::TextStyle::kOverline);
}

bool ReadAnythingAppController::IsLeafNode(ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);
  return ax_node->IsLeaf();
}

bool ReadAnythingAppController::IsWebUIToolbarEnabled() const {
  return features::IsReadAnythingWebUIToolbarEnabled();
}

bool ReadAnythingAppController::IsReadAloudEnabled() const {
  return features::IsReadAnythingReadAloudEnabled();
}

bool ReadAnythingAppController::IsChromeOsAsh() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else
  return false;
#endif
}

bool ReadAnythingAppController::IsAutoVoiceSwitchingEnabled() const {
  return features::IsReadAloudAutoVoiceSwitchingEnabled();
}

bool ReadAnythingAppController::IsLanguagePackDownloadingEnabled() const {
  return features::IsReadAloudLanguagePackDownloadingEnabled();
}

bool ReadAnythingAppController::IsAutomaticWordHighlightingEnabled() const {
   return features::IsReadAnythingReadAloudAutomaticWordHighlightingEnabled();
}

bool ReadAnythingAppController::IsGoogleDocs() const {
  return model_.IsDocs();
}

std::vector<std::string> ReadAnythingAppController::GetSupportedFonts() const {
  return model_.GetSupportedFonts();
}

void ReadAnythingAppController::RequestImageDataUrl(
    ui::AXNodeID node_id) const {
  if (features::IsReadAnythingImagesViaAlgorithmEnabled()) {
    auto target_tree_id = model_.active_tree_id();
    CHECK_NE(target_tree_id, ui::AXTreeIDUnknown());
    page_handler_->OnImageDataRequested(target_tree_id, node_id);
  }
}

std::string ReadAnythingAppController::GetImageDataUrl(
    ui::AXNodeID node_id) const {
  return model_.GetImageDataUrl(node_id);
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

bool ReadAnythingAppController::RequiresDistillation() {
  return model_.requires_distillation();
}

const std::string& ReadAnythingAppController::GetDefaultLanguageCodeForSpeech()
    const {
  return model_.default_language_code();
}

void ReadAnythingAppController::OnConnected() {
  web_ui_connected_time_ms_ = base::TimeTicks::Now();
  base::UmaHistogramLongTimes(
      "Accessibility.ReadAnything.TimeFromEntryTriggeredToWebUIConnected",
      base::TimeTicks::Now() - renderer_load_triggered_time_ms_);
  mojo::PendingReceiver<read_anything::mojom::UntrustedPageHandlerFactory>
      page_handler_factory_receiver =
          page_handler_factory_.BindNewPipeAndPassReceiver();
  page_handler_factory_->CreateUntrustedPageHandler(
      receiver_.BindNewPipeAndPassRemote(),
      page_handler_.BindNewPipeAndPassReceiver());
  content::RenderFrame* render_frame = GetRenderFrame();
  if (!render_frame) {
    return;
  }
  render_frame->GetBrowserInterfaceBroker()->GetInterface(
      std::move(page_handler_factory_receiver));
}

void ReadAnythingAppController::OnCopy() const {
  page_handler_->OnCopy();
}

void ReadAnythingAppController::OnFontSizeChanged(bool increase) {
  if (increase) {
    model_.IncreaseTextSize();
  } else {
    model_.DecreaseTextSize();
  }

  page_handler_->OnFontSizeChange(model_.font_size());
}

void ReadAnythingAppController::OnFontSizeReset() {
  model_.ResetTextSize();
  page_handler_->OnFontSizeChange(model_.font_size());
}

void ReadAnythingAppController::OnLinksEnabledToggled() {
  model_.ToggleLinksEnabled();
  page_handler_->OnLinksEnabledChanged(model_.links_enabled());
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
  if (model_.distillation_in_progress()) {
    return;
  }
  page_handler_->OnLinkClicked(model_.active_tree_id(), ax_node_id);
}

void ReadAnythingAppController::OnStandardLineSpacing() {
  page_handler_->OnLineSpaceChange(
      read_anything::mojom::LineSpacing::kStandard);
}

void ReadAnythingAppController::OnLooseLineSpacing() {
  page_handler_->OnLineSpaceChange(read_anything::mojom::LineSpacing::kLoose);
}

void ReadAnythingAppController::OnVeryLooseLineSpacing() {
  page_handler_->OnLineSpaceChange(
      read_anything::mojom::LineSpacing::kVeryLoose);
}

void ReadAnythingAppController::OnStandardLetterSpacing() {
  page_handler_->OnLetterSpaceChange(
      read_anything::mojom::LetterSpacing::kStandard);
}

void ReadAnythingAppController::OnWideLetterSpacing() {
  page_handler_->OnLetterSpaceChange(
      read_anything::mojom::LetterSpacing::kWide);
}

void ReadAnythingAppController::OnVeryWideLetterSpacing() {
  page_handler_->OnLetterSpaceChange(
      read_anything::mojom::LetterSpacing::kVeryWide);
}

void ReadAnythingAppController::OnLightTheme() {
  page_handler_->OnColorChange(read_anything::mojom::Colors::kLight);
}

void ReadAnythingAppController::OnDefaultTheme() {
  page_handler_->OnColorChange(read_anything::mojom::Colors::kDefault);
}

void ReadAnythingAppController::OnDarkTheme() {
  page_handler_->OnColorChange(read_anything::mojom::Colors::kDark);
}

void ReadAnythingAppController::OnYellowTheme() {
  page_handler_->OnColorChange(read_anything::mojom::Colors::kYellow);
}

void ReadAnythingAppController::OnBlueTheme() {
  page_handler_->OnColorChange(read_anything::mojom::Colors::kBlue);
}

void ReadAnythingAppController::OnFontChange(const std::string& font) {
  page_handler_->OnFontChange(font);
  model_.set_font_name(font);
}

void ReadAnythingAppController::OnSpeechRateChange(double rate) {
  page_handler_->OnSpeechRateChange(rate);
}

void ReadAnythingAppController::OnVoiceChange(const std::string& voice,
                                              const std::string& lang) {
  // Store the given voice with the base language. If the user prefers a voice
  // for a specific language, we should always use that voice, regardless of the
  // more specific locale. e.g. if the user prefers the en-UK voice for English
  // pages, use that voice even if the page is marked en-US.
  std::string base_lang = std::string(language::ExtractBaseLanguage(lang));
  page_handler_->OnVoiceChange(voice, base_lang);
  model_.SetVoice(voice, base_lang);
}

void ReadAnythingAppController::OnLanguagePrefChange(const std::string& lang,
                                                     bool enabled) {
  page_handler_->OnLanguagePrefChange(lang, enabled);
}

void ReadAnythingAppController::TurnedHighlightOn() {
  page_handler_->OnHighlightGranularityChanged(
      read_anything::mojom::HighlightGranularity::kOn);
}

void ReadAnythingAppController::TurnedHighlightOff() {
  page_handler_->OnHighlightGranularityChanged(
      read_anything::mojom::HighlightGranularity::kOff);
}

double ReadAnythingAppController::GetLineSpacingValue(int line_spacing) const {
  if (line_spacing >
      static_cast<int>(read_anything::mojom::LineSpacing::kMaxValue)) {
    return model_.GetLineSpacingValue(
        read_anything::mojom::LineSpacing::kDefaultValue);
  }

  return model_.GetLineSpacingValue(
      static_cast<read_anything::mojom::LineSpacing>(line_spacing));
}

double ReadAnythingAppController::GetLetterSpacingValue(
    int letter_spacing) const {
  if (letter_spacing >
      static_cast<int>(read_anything::mojom::LetterSpacing::kMaxValue)) {
    return model_.GetLetterSpacingValue(
        read_anything::mojom::LetterSpacing::kDefaultValue);
  }

  return model_.GetLetterSpacingValue(
      static_cast<read_anything::mojom::LetterSpacing>(letter_spacing));
}

void ReadAnythingAppController::OnSelectionChange(ui::AXNodeID anchor_node_id,
                                                  int anchor_offset,
                                                  ui::AXNodeID focus_node_id,
                                                  int focus_offset) const {
  DCHECK_NE(model_.active_tree_id(), ui::AXTreeIDUnknown());
  // Prevent link clicks while distillation is in progress, as it means that
  // the tree may have changed in an unexpected way.
  // TODO(crbug.com/40802192): Consider how to show this in a more
  // user-friendly way.
  if (model_.distillation_in_progress()) {
    return;
  }

  // Ignore the new selection if it's collapsed, which is created by a simple
  // click, unless there was a previous selection, in which case the click
  // clears the selection, so we should tell the main page to clear too.
  if ((anchor_offset == focus_offset) && (anchor_node_id == focus_node_id)) {
    if (model_.has_selection()) {
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

  page_handler_->OnSelectionChange(model_.active_tree_id(), anchor_node_id,
                                   anchor_offset, focus_node_id, focus_offset);
}

void ReadAnythingAppController::OnCollapseSelection() const {
  page_handler_->OnCollapseSelection();
}
void ReadAnythingAppController::InitAXPositionWithNode(
    const ui::AXNodeID& starting_node_id) {
  model_.InitAXPositionWithNode(starting_node_id);
}

std::vector<ui::AXNodeID> ReadAnythingAppController::GetCurrentText() {
  return model_.GetCurrentText();
}

void ReadAnythingAppController::MovePositionToNextGranularity() {
  model_.MovePositionToNextGranularity();
}

void ReadAnythingAppController::MovePositionToPreviousGranularity() {
  model_.MovePositionToPreviousGranularity();
}

int ReadAnythingAppController::GetCurrentTextStartIndex(ui::AXNodeID node_id) {
  return model_.GetCurrentTextStartIndex(node_id);
}

int ReadAnythingAppController::GetCurrentTextEndIndex(ui::AXNodeID node_id) {
  return model_.GetCurrentTextEndIndex(node_id);
}

// TODO(crbug.com/40802192): Change line_spacing and letter_spacing types from
// int to their corresponding enums.
void ReadAnythingAppController::SetThemeForTesting(const std::string& font_name,
                                                   float font_size,
                                                   bool links_enabled,
                                                   SkColor foreground_color,
                                                   SkColor background_color,
                                                   int line_spacing,
                                                   int letter_spacing) {
  auto line_spacing_enum =
      static_cast<read_anything::mojom::LineSpacing>(line_spacing);
  auto letter_spacing_enum =
      static_cast<read_anything::mojom::LetterSpacing>(letter_spacing);
  OnThemeChanged(ReadAnythingTheme::New(
      font_name, font_size, links_enabled, foreground_color, background_color,
      line_spacing_enum, letter_spacing_enum));
}

void ReadAnythingAppController::SetLanguageForTesting(
    const std::string& language_code) {
  SetLanguageCode(language_code);
}

void ReadAnythingAppController::SetLanguageCode(const std::string& code) {
  std::string base_lang = std::string(language::ExtractBaseLanguage(code));
  model_.set_base_language_code(base_lang);

  ExecuteJavaScript("chrome.readingMode.languageChanged();");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ReadAnythingAppController::OnDeviceLocked() {
  // Signal to the WebUI that the device has been locked. We'll only receive
  // this callback on ChromeOS.
  ExecuteJavaScript("chrome.readingMode.onLockScreen();");
}
#endif

// TODO(b/336596926): Use the default language code as a fallback when
// we get a language-unavailable error.
void ReadAnythingAppController::SetDefaultLanguageCode(
    const std::string& code) {
  std::string default_lang = std::string(language::ExtractBaseLanguage(code));
  // If the default language code is empty, continue to use the default
  // language code, as defined by ReadAnythingAppModel, currently 'en'
  if (default_lang.length() > 0) {
    model_.set_default_language_code(default_lang);
  }
}

void ReadAnythingAppController::SetContentForTesting(
    v8::Local<v8::Value> v8_snapshot_lite,
    std::vector<ui::AXNodeID> content_node_ids) {
  content::RenderFrame* render_frame = GetRenderFrame();
  if (!render_frame) {
    return;
  }
  v8::Isolate* isolate =
      render_frame->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
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

content::RenderFrame* ReadAnythingAppController::GetRenderFrame() {
  auto* web_frame = blink::WebLocalFrame::FromFrameToken(frame_token_);
  if (!web_frame) {
    return nullptr;
  }
  return content::RenderFrame::FromWebFrame(web_frame);
}

void ReadAnythingAppController::ShouldShowUI() {
  base::UmaHistogramLongTimes(
      "Accessibility.ReadAnything.TimeFromEntryTriggeredToContentLoaded",
      base::TimeTicks::Now() - renderer_load_triggered_time_ms_);
  base::UmaHistogramLongTimes(
      "Accessibility.ReadAnything.TimeFromWebUIConnectToContentLoaded",
      base::TimeTicks::Now() - web_ui_connected_time_ms_);
  page_handler_factory_->ShouldShowUI();
}

void ReadAnythingAppController::OnSpeechPlayingStateChanged(bool paused) {
  model_.set_speech_playing(!paused);
  if (paused && model_.requires_distillation()) {
    // TODO: b/40927698 - Do something smarter than completely re-distilling
    // when the update is small. Right now this resets the speech position to
    // the beginning which is annoying if the page is mostly the same.
    Distill();
  }
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

ui::AXNodeID ReadAnythingAppController::GetNodeIdForCurrentSegmentIndex(
    int index) {
  return model_.GetNodeIdForCurrentSegmentIndex(index);
}

int ReadAnythingAppController::GetNextWordHighlightLength(int index) {
  return model_.GetNextWordHighlightLength(index);
}

void ReadAnythingAppController::LogUmaHistogramTimes(int64_t time,
                                                     std::string metric) {
  base::UmaHistogramTimes(metric, base::Milliseconds(time));
}

void ReadAnythingAppController::LogUmaHistogramLongTimes(int64_t time,
                                                         std::string metric) {
  base::UmaHistogramLongTimes(metric, base::Milliseconds(time));
}

void ReadAnythingAppController::IncrementMetricCount(std::string metric) {
  model_.IncrementMetric(metric);
}

void ReadAnythingAppController::LogSpeechEventCounts() {
  model_.LogSpeechEventCounts();
}

void ReadAnythingAppController::LogSpeechErrorEvent(std::string error_code) {
  std::optional<ReadAnythingSpeechError> error;
  if (error_code == "text-too-long") {
    error = ReadAnythingSpeechError::kTextTooLong;
  } else if (error_code == "voice-unavailable") {
    error = ReadAnythingSpeechError::kVoiceUnavailabe;
  } else if (error_code == "language-unavailable") {
    error = ReadAnythingSpeechError::kLanguageUnavailable;
  } else if (error_code == "invalid-argument") {
    error = ReadAnythingSpeechError::kInvalidArgument;
  } else if (error_code == "synthesis-failed") {
    error = ReadAnythingSpeechError::kSynthesisFailed;
  } else if (error_code == "synthesis-unavailable") {
    error = ReadAnythingSpeechError::kSynthesisUnvailable;
  } else if (error_code == "audio-busy") {
    error = ReadAnythingSpeechError::kAudioBusy;
  } else if (error_code == "audio-hardware") {
    error = ReadAnythingSpeechError::kAudioHardware;
  } else if (error_code == "network") {
    error = ReadAnythingSpeechError::kNetwork;
  }

  // There are more error code possibilities, but right now, we only care
  // about tracking the above error codes.
  if (error.has_value()) {
    base::UmaHistogramEnumeration(
        string_constants::kReadAnythingSpeechErrorHistogramName, error.value());
  }
}
