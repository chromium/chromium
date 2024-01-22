// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_app_controller.h"

#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/renderer/accessibility/ax_tree_distiller.h"
#include "components/language/core/common/locale_util.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "read_anything_app_controller.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_update.h"
#include "url/url_util.h"
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

bool GetSelectable(const GURL& url) {
  std::string full_url = url.spec();
  for (std::string non_selectable_url :
       string_constants::GetNonSelectableUrls()) {
    if (re2::RE2::PartialMatch(full_url, non_selectable_url)) {
      return false;
    }
  }

  return true;
}

bool GetIsGoogleDocs(const GURL& url) {
  // A Google Docs URL is in the form of "https://docs.google.com/document*" or
  // "https://docs.sandbox.google.com/document*".
  constexpr const char* kDocsURLDomain[] = {"docs.google.com",
                                            "docs.sandbox.google.com"};
  if (url.SchemeIsHTTPOrHTTPS()) {
    for (const std::string& google_docs_url : kDocsURLDomain) {
      if (url.DomainIs(google_docs_url) && url.has_path() &&
          url.path().starts_with("/document") &&
          !url.ExtractFileName().empty()) {
        return true;
      }
    }
  }

  return false;
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
  distiller_ = std::make_unique<AXTreeDistiller>(
      base::BindRepeating(&ReadAnythingAppController::OnAXTreeDistilled,
                          weak_ptr_factory_.GetWeakPtr()));
}

ReadAnythingAppController::~ReadAnythingAppController() = default;
ReadAnythingAppController::ReadAloudCurrentGranularity::
    ReadAloudCurrentGranularity() {
  segments = std::map<ui::AXNodeID, ReadAloudTextSegment>();
}

ReadAnythingAppController::ReadAloudCurrentGranularity::
    ReadAloudCurrentGranularity(const ReadAloudCurrentGranularity& other) =
        default;

ReadAnythingAppController::ReadAloudCurrentGranularity::
    ~ReadAloudCurrentGranularity() = default;

void ReadAnythingAppController::AccessibilityEventReceived(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    const std::vector<ui::AXEvent>& events) {
  // This updates the model, which may require us to start distillation based on
  // the `requires_distillation()` state below.
  model_.AccessibilityEventReceived(tree_id, updates, events);

  if (model_.is_pdf()) {
    // Asumptions made about how the PDF contents are stored are incorrect.
    // Display "RM can't show this page" screen.
    if (!model_.IsPDFFormatted()) {
      model_.SetActiveTreeSelectable(false);
      ExecuteJavaScript("chrome.readingMode.showEmpty();");
      return;
    }
    // PDFs are stored in a different web content than the main web contents.
    // Enable a11y on it to get tree information from the PDF.
    ui::AXTreeID pdf_web_contents = model_.GetPDFWebContents();
    if (pdf_web_contents != ui::AXTreeIDUnknown() &&
        !model_.ContainsTree(pdf_web_contents)) {
      page_handler_->EnablePDFContentAccessibility(pdf_web_contents);
    }
  }

  if (tree_id != model_.GetActiveTreeId()) {
    return;
  }

  if (model_.requires_distillation()) {
    Distill();
  }

  // TODO(accessibility): it isn't clear this handles the pending updates path
  // correctly within the model.
  if (model_.requires_post_process_selection()) {
    PostProcessSelection();
  }
}

void ReadAnythingAppController::ExecuteJavaScript(std::string script) {
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
    const GURL& url,
    bool force_update_state) {
  if (tree_id == model_.GetActiveTreeId() && !force_update_state) {
    return;
  }
  model_.SetActiveTreeId(tree_id);
  model_.SetActiveUkmSourceId(ukm_source_id);
  model_.SetActiveTreeSelectable(GetSelectable(url));
  model_.SetIsPdf(url);
  model_.set_is_google_docs(GetIsGoogleDocs(url));
  // Delete all pending updates on the formerly active AXTree.
  // TODO(crbug.com/1266555): If distillation is in progress, cancel the
  // distillation request.
  model_.ClearPendingUpdates();
  model_.set_requires_distillation(false);

  ExecuteJavaScript("chrome.readingMode.showLoading();");

  // When the UI first constructs, this function may be called before tree_id
  // has been added to the tree list in AccessibilityEventReceived. In that
  // case, do not distill.
  if (model_.GetActiveTreeId() != ui::AXTreeIDUnknown() &&
      model_.ContainsTree(model_.GetActiveTreeId())) {
    Distill();
  }
}

void ReadAnythingAppController::OnAXTreeDestroyed(const ui::AXTreeID& tree_id) {
  model_.OnAXTreeDestroyed(tree_id);
}

void ReadAnythingAppController::Distill() {
  if (model_.distillation_in_progress()) {
    // When distillation is in progress, the model may have queued up tree
    // updates. In those cases, assume we eventually get to `OnAXTreeDistilled`,
    // where we re-request `Distill`.
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

  ui::AXSerializableTree* tree = model_.GetTreeFromId(model_.GetActiveTreeId());
  std::unique_ptr<ui::AXTreeSource<const ui::AXNode*>> tree_source(
      tree->CreateTreeSource());
  ui::AXTreeSerializer<const ui::AXNode*, std::vector<const ui::AXNode*>>
      serializer(tree_source.get());
  ui::AXTreeUpdate snapshot;
  if (!tree->root()) {
    return;
  }
  CHECK(serializer.SerializeChanges(tree->root(), &snapshot));
  model_.SetDistillationInProgress(true);
  distiller_->Distill(*tree, snapshot, model_.active_ukm_source_id());
}

void ReadAnythingAppController::OnAXTreeDistilled(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  // Update Read Aloud state.
  ax_position_ = ui::AXNodePosition::AXPosition::CreateNullPosition();
  current_text_index_ = 0;
  processed_granularity_index_ = -1;
  processed_granularities_on_current_page_.clear();

  // Reset state, including the current side panel selection so we can update
  // it based on the new main panel selection in PostProcessSelection below.
  model_.Reset(content_node_ids);

  // Return early if any of the following scenarios occurred while waiting for
  // distillation to complete:
  // 1. tree_id != model_.GetActiveTreeId(): The active tree was changed.
  // 2. model_.GetActiveTreeId()== ui::AXTreeIDUnknown(): The active tree was
  // change to
  //    an unknown tree id.
  // 3. !model_.ContainsTree(tree_id): The distilled tree was destroyed.
  // 4. tree_id == ui::AXTreeIDUnknown(): The distiller sent back an unknown
  //    tree id which occurs when there was an error.
  if (tree_id != model_.GetActiveTreeId() ||
      model_.GetActiveTreeId() == ui::AXTreeIDUnknown() ||
      !model_.ContainsTree(tree_id) || tree_id == ui::AXTreeIDUnknown()) {
    return;
  }
  if (!model_.content_node_ids().empty()) {
    // If there are content_node_ids, this means the AXTree was successfully
    // distilled.
    model_.ComputeDisplayNodeIdsForDistilledTree();
  }

  // Draw the selection in the side panel (if one exists in the main panel) and
  // the content if the selection is not in the distilled content.
  PostProcessSelection();

  if (model_.is_empty()) {
    ExecuteJavaScript("chrome.readingMode.showEmpty();");
    if (IsSelectable()) {
      base::UmaHistogramEnumeration(string_constants::kEmptyStateHistogramName,
                                    ReadAnythingEmptyState::kEmptyStateShown);
    }
  }

  // AXNode's language code is BCP 47. Only the base language is needed to
  // record the metric.
  std::string language =
      model_.GetTreeFromId(model_.GetActiveTreeId())->root()->GetLanguage();
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
  }
}

void ReadAnythingAppController::PostProcessSelection() {
  if (model_.PostProcessSelection()) {
    Draw();
  }
  // Skip drawing the selection in the side panel if the selection originally
  // came from there.
  if (!model_.selection_from_action()) {
    DrawSelection();
  }
  model_.set_selection_from_action(false);
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
      model_.GetActiveTreeId() != ui::AXTreeIDUnknown()) {
    Draw();
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
    read_anything::mojom::HighlightGranularity granularity) {
  bool needs_redraw_for_links = model_.links_enabled() != links_enabled;
  model_.OnSettingsRestoredFromPrefs(line_spacing, letter_spacing, font,
                                     font_size, links_enabled, color,
                                     speech_rate, &voices, granularity);
  ExecuteJavaScript("chrome.readingMode.restoreSettingsFromPrefs();");
  // Only redraw if there is an active tree.
  if (needs_redraw_for_links &&
      model_.GetActiveTreeId() != ui::AXTreeIDUnknown()) {
    Draw();
  }
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void ReadAnythingAppController::ScreenAIServiceReady() {
  distiller_->ScreenAIServiceReady(GetRenderFrame());
}
#endif

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
      .SetProperty("isReadAloudEnabled",
                   &ReadAnythingAppController::IsReadAloudEnabled)
      .SetProperty("isSelectable", &ReadAnythingAppController::IsSelectable)
      .SetProperty("speechSynthesisLanguageCode",
                   &ReadAnythingAppController::GetLanguageCodeForSpeech)
      .SetMethod("getChildren", &ReadAnythingAppController::GetChildren)
      .SetMethod("getDataFontCss", &ReadAnythingAppController::GetDataFontCss)
      .SetMethod("getTextDirection",
                 &ReadAnythingAppController::GetTextDirection)
      .SetMethod("getHtmlTag", &ReadAnythingAppController::GetHtmlTag)
      .SetMethod("getLanguage", &ReadAnythingAppController::GetLanguage)
      .SetMethod("getTextContent", &ReadAnythingAppController::GetTextContent)
      .SetMethod("getUrl", &ReadAnythingAppController::GetUrl)
      .SetMethod("shouldBold", &ReadAnythingAppController::ShouldBold)
      .SetMethod("isOverline", &ReadAnythingAppController::IsOverline)
      .SetMethod("isLeafNode", &ReadAnythingAppController::IsLeafNode)
      .SetMethod("isGoogleDocs", &ReadAnythingAppController::IsGoogleDocs)
      .SetMethod("onConnected", &ReadAnythingAppController::OnConnected)
      .SetMethod("onCopy", &ReadAnythingAppController::OnCopy)
      .SetMethod("onFontSizeChanged",
                 &ReadAnythingAppController::OnFontSizeChanged)
      .SetMethod("onFontSizeReset", &ReadAnythingAppController::OnFontSizeReset)
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
      .SetMethod("initAXPositionWithNode",
                 &ReadAnythingAppController::InitAXPositionWithNode)
      .SetMethod("getNextTextStartIndex",
                 &ReadAnythingAppController::GetNextTextStartIndex)
      .SetMethod("getNextTextEndIndex",
                 &ReadAnythingAppController::GetNextTextEndIndex)
      .SetMethod("getNextText", &ReadAnythingAppController::GetNextText)
      .SetMethod("getPreviousText", &ReadAnythingAppController::GetPreviousText)
      .SetMethod("shouldShowUI", &ReadAnythingAppController::ShouldShowUI);
}

ui::AXNodeID ReadAnythingAppController::RootId() const {
  ui::AXSerializableTree* tree = model_.GetTreeFromId(model_.GetActiveTreeId());
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

std::string ReadAnythingAppController::GetStoredVoice(
    const std::string& lang) const {
  if (model_.voices().contains(lang)) {
    return *model_.voices().FindString(lang);
  }

  return string_constants::kReadAnythingPlaceholderVoiceName;
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
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);

  std::string html_tag =
      ax_node->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);

  if (model_.is_pdf()) {
    return GetHtmlTagForPDF(ax_node, html_tag);
  }

  if (ui::IsTextField(ax_node->GetRole())) {
    return "div";
  }

  // Some divs are marked with role=heading and aria-level=# to indicate
  // the heading level, so use the <h#> tag directly.
  if (ax_node->GetRole() == ax::mojom::Role::kHeading) {
    std::string aria_level = GetAriaLevel(ax_node);
    if (!aria_level.empty()) {
      return "h" + aria_level;
    }
  }

  if (html_tag == ui::ToString(ax::mojom::Role::kMark)) {
    // Replace mark element with bold element for readability.
    html_tag = "b";
  } else if (IsGoogleDocs()) {
    // Change HTML tags for SVG elements to allow Reading Mode to render text
    // for the Annotated Canvas elements in a Google Doc.
    if (html_tag == "svg") {
      html_tag = "div";
    }
    if (html_tag == "g" && ax_node->GetRole() == ax::mojom::Role::kParagraph) {
      html_tag = "p";
    }
  }

  return html_tag;
}

std::string ReadAnythingAppController::GetAriaLevel(ui::AXNode* ax_node) const {
  std::string aria_level;
  ax_node->GetHtmlAttribute("aria-level", &aria_level);
  return aria_level;
}

std::string ReadAnythingAppController::GetHtmlTagForPDF(
    ui::AXNode* ax_node,
    std::string html_tag) const {
  ax::mojom::Role role = ax_node->GetRole();

  // Some nodes in PDFs don't have an HTML tag so use role instead.
  switch (role) {
    case ax::mojom::Role::kEmbeddedObject:
    case ax::mojom::Role::kRegion:
    case ax::mojom::Role::kPdfRoot:
    case ax::mojom::Role::kRootWebArea:
      return "span";
    case ax::mojom::Role::kParagraph:
      return "p";
    case ax::mojom::Role::kLink:
      return "a";
    case ax::mojom::Role::kStaticText:
      return "";
    case ax::mojom::Role::kHeading:
      return GetHeadingHtmlTagForPDF(ax_node, html_tag);
    // Add a line break after each page of an inaccessible PDF for readability
    // since there is no other formatting included in the OCR output.
    case ax::mojom::Role::kContentInfo:
      if (ax_node->GetTextContentUTF8() == string_constants::kPDFPageEnd) {
        return "br";
      }
      ABSL_FALLTHROUGH_INTENDED;
    default:
      return html_tag;
  }
}

std::string ReadAnythingAppController::GetHeadingHtmlTagForPDF(
    ui::AXNode* ax_node,
    std::string html_tag) const {
  // Sometimes whole paragraphs can be formatted as a heading. If the text is
  // longer than 2 lines, assume it was meant to be a paragragh,
  if (ax_node->GetTextContentUTF8().length() > (2 * kMaxLineWidth)) {
    return "p";
  }

  // A single block of text could be incorrectly formatted with multiple heading
  // nodes (one for each line of text) instead of a single paragraph node. This
  // case should be detected to improve readability. If there are multiple
  // consecutive nodes with the same heading level, assume that they are all a
  // part of one paragraph.
  ui::AXNode* next = ax_node->GetNextUnignoredSibling();
  ui::AXNode* prev = ax_node->GetPreviousUnignoredSibling();

  if ((next && next->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag) ==
                   html_tag) ||
      (prev && prev->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag) ==
                   html_tag)) {
    return "span";
  }

  std::string aria_level = GetAriaLevel(ax_node);
  return !aria_level.empty() ? "h" + aria_level : html_tag;
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
  if ((ax_node->GetTextContentUTF8()).empty() && IsGoogleDocs()) {
    // For Google Docs, we distill text from the aria-labels of annotated
    // canvas's rect elements. Therefore, we need to explicitly read the name
    // attribute to get the text.
    return GetNameAttributeText(ax_node);
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

  // Prevent XSS from href attribute, which could be set to a script instead of
  // a valid website.
  if (url::FindAndCompareScheme(url, static_cast<int>(strlen(url)), "http",
                                nullptr) ||
      url::FindAndCompareScheme(url, static_cast<int>(strlen(url)), "https",
                                nullptr)) {
    return url;
  }
  return "";
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

bool ReadAnythingAppController::IsSelectable() const {
  return model_.active_tree_selectable();
}

bool ReadAnythingAppController::IsWebUIToolbarEnabled() const {
  return features::IsReadAnythingWebUIToolbarEnabled();
}

bool ReadAnythingAppController::IsReadAloudEnabled() const {
  return features::IsReadAnythingReadAloudEnabled();
}

bool ReadAnythingAppController::IsGoogleDocs() const {
  return model_.is_docs();
}

std::vector<std::string> ReadAnythingAppController::GetSupportedFonts() const {
  return model_.GetSupportedFonts();
}

const std::string& ReadAnythingAppController::GetLanguageCodeForSpeech() const {
  // TODO(crbug.com/1474951): Instead of returning the default browser language
  // we should use the page language.
  return model_.default_language_code();
}

void ReadAnythingAppController::OnConnected() {
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

void ReadAnythingAppController::OnScroll(bool on_selection) const {
  model_.OnScroll(on_selection, /* from_reading_mode= */ true);
}

void ReadAnythingAppController::OnLinkClicked(ui::AXNodeID ax_node_id) const {
  DCHECK_NE(model_.GetActiveTreeId(), ui::AXTreeIDUnknown());
  // Prevent link clicks while distillation is in progress, as it means that the
  // tree may have changed in an unexpected way.
  // TODO(crbug.com/1266555): Consider how to show this in a more user-friendly
  // way.
  if (model_.distillation_in_progress()) {
    return;
  }
  page_handler_->OnLinkClicked(model_.GetActiveTreeId(), ax_node_id);
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
}

void ReadAnythingAppController::OnSpeechRateChange(double rate) {
  page_handler_->OnSpeechRateChange(rate);
}

void ReadAnythingAppController::OnVoiceChange(const std::string& voice,
                                              const std::string& lang) {
  page_handler_->OnVoiceChange(voice, lang);
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
  DCHECK_NE(model_.GetActiveTreeId(), ui::AXTreeIDUnknown());
  // Prevent link clicks while distillation is in progress, as it means that the
  // tree may have changed in an unexpected way.
  // TODO(crbug.com/1266555): Consider how to show this in a more user-friendly
  // way.
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
  // page_handler_->OnSelectionChange to be incorrectly triggered, resulting in
  // a failing DCHECK. Therefore, return early if this happens.
  // This check does not apply to pdfs.
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

  page_handler_->OnSelectionChange(model_.GetActiveTreeId(), anchor_node_id,
                                   anchor_offset, focus_node_id, focus_offset);
}

void ReadAnythingAppController::OnCollapseSelection() const {
  page_handler_->OnCollapseSelection();
}
void ReadAnythingAppController::InitAXPositionWithNode(
    const ui::AXNodeID starting_node_id) {
  ui::AXNode* ax_node = model_.GetAXNode(starting_node_id);

  // If instance is Null or Empty, create the next AxPosition
  if (ax_node != nullptr && (!ax_position_ || ax_position_->IsNullPosition())) {
    ax_position_ =
        ui::AXNodePosition::CreateTreePositionAtStartOfAnchor(*ax_node);
    current_text_index_ = 0;
    processed_granularity_index_ = -1;
    processed_granularities_on_current_page_.clear();
  }
}

bool ReadAnythingAppController::NodeBeenOrWillBeSpoken(
    ReadAnythingAppController::ReadAloudCurrentGranularity current_granularity,
    ui::AXNodeID id) {
  if (base::Contains(current_granularity.segments, id)) {
    return true;
  }
  for (ReadAnythingAppController::ReadAloudCurrentGranularity granularity :
       processed_granularities_on_current_page_) {
    if (base::Contains(granularity.segments, id)) {
      return true;
    }
  }

  return false;
}

std::vector<ui::AXNodeID> ReadAnythingAppController::GetNextText(
    int max_text_length) {
  bool was_previously_processed =
      processed_granularity_index_ <
      processed_granularities_on_current_page_.size() - 1;

  // If we've previously processed the triples at this location, return the
  // previously processed node information. Otherwise, get this information
  // GetNextNodes.
  ReadAnythingAppController::ReadAloudCurrentGranularity current_granularity =
      (was_previously_processed) ? processed_granularities_on_current_page_
                                       [processed_granularity_index_ + 1]
                                 : GetNextNodes(max_text_length);

  // If the list of nodes is empty, don't adjust the processed nodes
  // information.
  if (current_granularity.node_ids.size() == 0) {
    return current_granularity.node_ids;
  }

  if (!was_previously_processed) {
    processed_granularities_on_current_page_.push_back(current_granularity);
  }
  processed_granularity_index_++;

  return current_granularity.node_ids;
}

// TODO(crbug.com/1474951): Update to use AXRange to better handle multiple
// nodes. This may require updating GetText in ax_range.h to return AXNodeIds.
// AXRangeType#ExpandToEnclosingTextBoundary may also be useful.
ReadAnythingAppController::ReadAloudCurrentGranularity
ReadAnythingAppController::GetNextNodes(int max_text_length) {
  ReadAnythingAppController::ReadAloudCurrentGranularity current_granularity =
      ReadAnythingAppController::ReadAloudCurrentGranularity();

  // Make sure we're adequately returning at the end of content.
  if (!ax_position_ || ax_position_->AtEndOfAXTree() ||
      ax_position_->IsNullPosition()) {
    return current_granularity;
  }

  std::u16string current_text;

  // Loop through the tree in order to group nodes together into the same
  // granularity segment until there are no more pieces that can be added
  // to the current segment or we've reached the end of the tree.
  // e.g. if the following two nodes are next to one another in the tree:
  //  AXNode: id=1, text = "This is a "
  //  AXNode: id=2, text = "link. "
  // both AXNodes should be added to the current granularity, as the
  // combined text across the two nodes forms a complete sentence with sentence
  // granularity.
  // This allows text to be spoken smoothly across nodes with broken sentences,
  // such as links and formatted text.
  // TODO(crbug.com/1474951): Investigate how much of this can be pulled into
  // AXPosition to simplify Read Aloud-specific code and allow improvements
  // to be used by other places where AXPosition is used.
  while (!ax_position_->IsNullPosition() && !ax_position_->AtEndOfAXTree()) {
    ui::AXNode* anchor_node = GetNodeFromCurrentPosition();
    std::u16string text = anchor_node->GetTextContentUTF16();
    std::u16string text_substr = text.substr(current_text_index_);
    int prev_index = current_text_index_;
    // Gets the starting index for the next sentence in the current node.
    int next_sentence_index =
        GetNextSentence(text_substr, max_text_length) + prev_index;
    // If our current index within the current node is greater than that node's
    // text, look at the next node. If the starting index of the next sentence
    // in the node is the same the current index within the node, this means
    // that we've reached the end of all possible sentences within the current
    // node, and should move to the next node.
    if ((size_t)current_text_index_ >= text.size() ||
        (current_text_index_ == next_sentence_index)) {
      // Move the AXPosition to the next node.
      ax_position_ =
          GetNextValidPositionFromCurrentPosition(current_granularity);
      // Reset the current text index within the current node since we just
      // moved to a new node.
      current_text_index_ = 0;
      // If we've reached the end of the content, go ahead and return the
      // current list of nodes because there are no more nodes to look through.
      if (ax_position_->IsNullPosition() || ax_position_->AtEndOfAXTree() ||
          !ax_position_->GetAnchor()) {
        return current_granularity;
      }

      // If the position is now at the start of a paragraph and we already have
      // nodes to return, return the current list of nodes so that we don't
      // cross paragraph boundaries with text.
      if (ax_position_->AtStartOfParagraph() &&
          current_granularity.node_ids.size() > 0) {
        return current_granularity;
      }

      std::u16string base_text =
          GetNodeFromCurrentPosition()->GetTextContentUTF16();

      // Look at the text of the items we've already added to the
      // current sentence (current_text) combined with the text of the next
      // node (base_text).
      const std::u16string& combined_text = current_text + base_text;
      // Get the index of the next sentence if we're looking at the combined
      // previous and current node text.
      int combined_sentence_index =
          GetNextSentence(combined_text, max_text_length);
      // If the combined_sentence_index is the same as the current_text length,
      // the new node should not be considered part of the current sentence.
      // If these values differ, add the current node's text to the list of
      // nodes in the current sentence.
      // Consider these two examples:
      // Example 1:
      //  current text: Hello
      //  current node's text: , how are you?
      //    The current text length is 5, but the index of the next sentence of
      //    the combined text is 19, so the current node should be added to
      //    the current sentence.
      // Example 2:
      //  current text: Hello.
      //  current node: Goodbye.
      //    The current text length is 6, and the next sentence index of
      //    "Hello. Goodbye." is still 6, so the current node's text shouldn't
      //    be added to the current sentence.
      if ((int)current_text.length() != combined_sentence_index) {
        anchor_node = GetNodeFromCurrentPosition();
        // Calculate the new sentence index.
        int index_in_new_node = combined_sentence_index - current_text.length();
        // Add the current node to the list of nodes to be returned, with a
        // text range from 0 to the start of the next sentence
        // (index_in_new_node);
        ReadAnythingAppController::ReadAloudTextSegment segment;
        segment.id = anchor_node->id();
        segment.text_start = 0;
        segment.text_end = index_in_new_node;
        current_granularity.AddSegment(segment);
        current_text +=
            anchor_node->GetTextContentUTF16().substr(0, index_in_new_node);
        current_text_index_ = index_in_new_node;
        if (current_text_index_ != (int)base_text.length()) {
          // If we're in the middle of the node, there's no need to attempt
          // to find another segment, as we're at the end of the current
          // segment.
          return current_granularity;
        }
        continue;
      } else if (current_granularity.node_ids.size() > 0) {
        // If nothing has been added to the list of current nodes, we should
        // look at the next sentence within the current node. However, if
        // there have already been nodes added to the list of nodes to return
        // and we determine that the next node shouldn't be added to the
        // current sentence, we've completed the current sentence, so we can
        // return the current list.
        return current_granularity;
      }
    }

    // Add the next granularity piece within the current node.
    anchor_node = GetNodeFromCurrentPosition();
    text = anchor_node->GetTextContentUTF16();
    prev_index = current_text_index_;
    text_substr = text.substr(current_text_index_);
    // Find the next sentence within the current node.
    int new_current_text_index =
        GetNextSentence(text_substr, max_text_length) + prev_index;
    // If adding the next piece of the sentence from the current node doesn't
    // make the returned text too long, add it to the list of nodes.
    if ((current_text.length() + new_current_text_index - prev_index) <
        (size_t)max_text_length) {
      int start_index = current_text_index_;
      current_text_index_ = new_current_text_index;
      // Add the current node to the list of nodes to be returned, with a
      // text range from the starting index (the end of the previous piece of
      // the sentence) to the start of the next sentence.
      ReadAnythingAppController::ReadAloudTextSegment segment;
      segment.id = anchor_node->id();
      segment.text_start = start_index;
      segment.text_end = new_current_text_index;
      current_granularity.AddSegment(segment);
      current_text += anchor_node->GetTextContentUTF16().substr(
          start_index, current_text_index_ - start_index);
    } else {
      // If adding the next segment to the list of nodes is greater than the
      // maximum text length, return the current nodes.
      // TODO(crbug.com/1474951): Find a better way of segmenting granularities
      // that are too long.
      return current_granularity;
    }

    // After adding the most recent granularity segment, if we're not at the
    //  end of the node, the current nodes can be returned, as we know there's
    // no further segments remaining.
    if ((size_t)current_text_index_ != text.length()) {
      return current_granularity;
    }
  }
  return current_granularity;
}

// TODO(crbug.com/1474951): Random access to processed nodes might not always
// work (e.g. if we're switching granularities or jumping to a specific node),
// so we should implement a method of retrieving previous text from AXPosition
std::vector<ui::AXNodeID> ReadAnythingAppController::GetPreviousText(
    int max_text_length) {
  // If GetPreviousText is called before the tree is initialized or before
  // there are any processed granularities, return an empty vector.
  if (processed_granularities_on_current_page_.size() == 0) {
    return std::vector<ui::AXNodeID>();
  }

  // If we've reached the beginning of the content, we should continue to return
  // the text grouping, so don't decrement below 0.
  if (processed_granularity_index_ > 0) {
    processed_granularity_index_--;
  }

  return processed_granularities_on_current_page_[processed_granularity_index_]
      .node_ids;
}

// Returns either the node or the lowest platform ancestor of the node, if it's
// a leaf.
ui::AXNode* ReadAnythingAppController::GetNodeFromCurrentPosition() {
  if (ax_position_->GetAnchor()->IsChildOfLeaf()) {
    return ax_position_->GetAnchor()->GetLowestPlatformAncestor();
  }

  return ax_position_->GetAnchor();
}

// Gets the next valid position from our current position within AXPosition
// AXPosition returns nodes that aren't supported by Reading Mode, so we
// need to have a bit of extra logic to ensure we're only passing along valid
// nodes.
// Some of the checks here right now are probably unneeded.
ui::AXNodePosition::AXPositionInstance
ReadAnythingAppController::GetNextValidPositionFromCurrentPosition(
    ReadAnythingAppController::ReadAloudCurrentGranularity
        current_granularity) {
  ui::AXNodePosition::AXPositionInstance new_position =
      ui::AXNodePosition::CreateNullPosition();

  ui::AXMovementOptions movement_options(
      ui::AXBoundaryBehavior::kCrossBoundary,
      ui::AXBoundaryDetection::kDontCheckInitialPosition);

  new_position = ax_position_->CreatePositionAtTextBoundary(
      ax::mojom::TextBoundary::kSentenceStart,
      ax::mojom::MoveDirection::kForward, movement_options);

  if (new_position->IsNullPosition() || new_position->AtEndOfAXTree() ||
      !new_position->GetAnchor()) {
    return new_position;
  }

  bool is_leaf = new_position->GetAnchor()->IsChildOfLeaf();
  // If the node is a leaf, use the parent node instead.
  ui::AXNode* anchor_node =
      is_leaf ? new_position->GetAnchor()->GetLowestPlatformAncestor()
              : new_position->GetAnchor();
  bool was_previously_spoken =
      NodeBeenOrWillBeSpoken(current_granularity, anchor_node->id());
  // TODO(crbug.com/1474951): Can this be updated to IsText() instead?
  bool is_text_node = (GetHtmlTag((anchor_node->id())).length() == 0);
  const std::set<ui::AXNodeID>* node_ids = model_.selection_node_ids().empty()
                                               ? &model_.display_node_ids()
                                               : &model_.selection_node_ids();
  bool contains_node = base::Contains(*node_ids, anchor_node->id());

  while (was_previously_spoken || !is_text_node || !contains_node) {
    ui::AXNodePosition::AXPositionInstance possible_new_position =
        new_position->CreateNextSentenceStartPosition(movement_options);
    anchor_node = possible_new_position->GetAnchor();
    if (!anchor_node) {
      if (was_previously_spoken) {
        // If the previous position we were looking at was previously spoken,
        // go ahead and return the null position to avoid duplicate nodes
        // being added.
        return possible_new_position;
      }
      return new_position;
    }

    new_position =
        new_position->CreateNextSentenceStartPosition(movement_options);

    is_leaf = anchor_node->IsChildOfLeaf();
    if (is_leaf) {
      anchor_node = anchor_node->GetLowestPlatformAncestor();
    }
    was_previously_spoken =
        NodeBeenOrWillBeSpoken(current_granularity, anchor_node->id());
    is_text_node = (GetHtmlTag((anchor_node->id())).length() == 0);
    contains_node = base::Contains(*node_ids, anchor_node->id());
  }

  return new_position;
}

int ReadAnythingAppController::GetNextTextStartIndex(ui::AXNodeID node_id) {
  if (processed_granularities_on_current_page_.size() < 1) {
    return -1;
  }

  ReadAnythingAppController::ReadAloudCurrentGranularity current_granularity =
      processed_granularities_on_current_page_[processed_granularity_index_];
  if (!current_granularity.segments.count(node_id)) {
    return -1;
  }
  ReadAnythingAppController::ReadAloudTextSegment segment =
      current_granularity.segments[node_id];

  return segment.text_start;
}

int ReadAnythingAppController::GetNextTextEndIndex(ui::AXNodeID node_id) {
  if (processed_granularities_on_current_page_.size() < 1) {
    return -1;
  }

  ReadAnythingAppController::ReadAloudCurrentGranularity current_granularity =
      processed_granularities_on_current_page_[processed_granularity_index_];
  if (!current_granularity.segments.count(node_id)) {
    return -1;
  }
  ReadAnythingAppController::ReadAloudTextSegment segment =
      current_granularity.segments[node_id];

  return segment.text_end;
}

int ReadAnythingAppController::GetNextSentence(const std::u16string& text,
                                               int max_text_length) {
  // TODO(crbug.com/1474941): Investigate providing correct line breaks
  // or alternatively making adjustments to ax_text_utils to return boundaries
  // that minimize choppiness.
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

// TODO(crbug.com/1266555): Change line_spacing and letter_spacing types from
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
  SetDefaultLanguageCode(language_code);
}

void ReadAnythingAppController::SetDefaultLanguageCode(
    const std::string& code) {
  model_.set_default_language_code(code);

  // Signal to the WebUI that the supported fonts may have changed.
  ExecuteJavaScript("chrome.readingMode.updateFonts();");
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
                          GURL::EmptyGURL(), false);
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
  page_handler_factory_->ShouldShowUI();
}
