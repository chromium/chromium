// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_anything_app_controller.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/common/read_anything/read_anything.mojom-shared.h"
#include "chrome/common/read_anything/read_anything_util.h"
#include "chrome/renderer/accessibility/ax_tree_distiller.h"
#include "chrome/renderer/accessibility/phrase_segmentation/dependency_parser_model.h"
#include "chrome/renderer/accessibility/read_anything/read_aloud_app_model.h"
#include "chrome/renderer/accessibility/read_anything/read_aloud_traversal_utils.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_test_utils.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_frame.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/strings/grit/services_strings.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_features.mojom.h"
#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-microtask-queue.h"

namespace {}  // namespace

class MockAXTreeDistiller : public AXTreeDistiller {
 public:
  explicit MockAXTreeDistiller(content::RenderFrame* render_frame)
      : AXTreeDistiller(render_frame, base::NullCallback()) {}
  MOCK_METHOD(void,
              Distill,
              (const ui::AXTree& tree,
               const ui::AXTreeUpdate& snapshot,
               const ukm::SourceId ukm_source_id),
              (override));
};

class MockReadAnythingUntrustedPageHandler
    : public read_anything::mojom::UntrustedPageHandler {
 public:
  MockReadAnythingUntrustedPageHandler() {
    ON_CALL(*this, RequestReadabilityDistillation(testing::_))
        .WillByDefault([](RequestReadabilityDistillationCallback callback) {
          std::move(callback).Run(
              read_anything::mojom::ReadabilityDistillationResult::kEmpty, "",
              "");
        });
  }

  MOCK_METHOD(void,
              GetDependencyParserModel,
              (GetDependencyParserModelCallback mojo_callback),
              (override));
  MOCK_METHOD(void,
              GetVoicePackInfo,
              (const std::string& language),
              (override));
  MOCK_METHOD(void,
              InstallVoicePack,
              (const std::string& language),
              (override));
  MOCK_METHOD(void, UninstallVoice, (const std::string& language), (override));
  MOCK_METHOD(void,
              OnLinkClicked,
              (const ui::AXTreeID& target_tree_id, ui::AXNodeID target_node_id),
              (override));
  MOCK_METHOD(void,
              ScrollToTargetNode,
              (const ui::AXTreeID& target_tree_id, ui::AXNodeID target_node_id),
              (override));
  MOCK_METHOD(void,
              OnSelectionChange,
              (const ui::AXTreeID& target_tree_id,
               ui::AXNodeID anchor_node_id,
               int anchor_offset,
               ui::AXNodeID focus_node_id,
               int focus_offset),
              (override));
  MOCK_METHOD(void, OnCollapseSelection, (), (override));
  MOCK_METHOD(void, OnCopy, (), (override));
  MOCK_METHOD(void,
              OnLineSpaceChange,
              (read_anything::mojom::LineSpacing line_spacing),
              (override));
  MOCK_METHOD(void,
              OnLetterSpaceChange,
              (read_anything::mojom::LetterSpacing letter_spacing),
              (override));
  MOCK_METHOD(void, OnFontChange, (const std::string& font), (override));
  MOCK_METHOD(void, OnFontSizeChange, (double font_size), (override));
  MOCK_METHOD(void, OnLinksEnabledChanged, (bool enabled), (override));
  MOCK_METHOD(void, OnTranslationRequested, (), (override));
  MOCK_METHOD(void, OnImagesEnabledChanged, (bool enabled), (override));
  MOCK_METHOD(void, OnSpeechRateChange, (double rate), (override));
  MOCK_METHOD(void,
              OnVoiceChange,
              (const std::string& voice, const std::string& lang),
              (override));
  MOCK_METHOD(void,
              OnLanguagePrefChange,
              (const std::string& lang, bool enabled),
              (override));
  MOCK_METHOD(void,
              OnColorChange,
              (read_anything::mojom::Colors color),
              (override));
  MOCK_METHOD(void,
              OnHighlightGranularityChanged,
              (read_anything::mojom::HighlightGranularity granularity),
              (override));
  MOCK_METHOD(void,
              OnLineFocusChanged,
              (read_anything::mojom::LineFocus current_line_focus,
               read_anything::mojom::LineFocus last_non_disabled_line_focus),
              (override));
  MOCK_METHOD(void,
              OnImageDataRequested,
              (const ::ui::AXTreeID& target_tree_id, int32_t target_node_id),
              (override));
  MOCK_METHOD(void, OnReadAloudAudioStateChange, (bool playing), (override));
  MOCK_METHOD(void, LogExtensionState, (), (override));
  MOCK_METHOD(void,
              OnDistillationStatus,
              (read_anything::mojom::DistillationStatus, int word_count),
              (override));
  MOCK_METHOD(void, GetPresentationState, (), (override));
  MOCK_METHOD(void, CloseUI, (), (override));
  MOCK_METHOD(void, TogglePinState, (), (override));
  MOCK_METHOD(void, TogglePresentation, (), (override));
  MOCK_METHOD(void, AckReadingModeHidden, (), (override));
  MOCK_METHOD(void, SendPinStateRequest, (), (override));
  MOCK_METHOD(void,
              OnDistillationStateChanged,
              (read_anything::mojom::ReadAnythingDistillationState new_state),
              (override));
  MOCK_METHOD(void, OnSpeechEngineStalled, (), (override));
  MOCK_METHOD(void,
              RequestReadabilityDistillation,
              (RequestReadabilityDistillationCallback),
              (override));

  mojo::PendingRemote<read_anything::mojom::UntrustedPageHandler>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }
  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<read_anything::mojom::UntrustedPageHandler> receiver_{this};
};

using testing::ElementsAre;
using testing::IsEmpty;
using testing::Mock;

class ReadAnythingAppControllerTest : public ChromeRenderViewTest {
 public:
  ReadAnythingAppControllerTest() = default;
  ~ReadAnythingAppControllerTest() override = default;
  ReadAnythingAppControllerTest(const ReadAnythingAppControllerTest&) = delete;
  ReadAnythingAppControllerTest& operator=(
      const ReadAnythingAppControllerTest&) = delete;

  const std::string DOCS_URL =
      "https://docs.google.com/document/d/"
      "1t6x1PQaQWjE8wb9iyYmFaoK1XAEgsl8G1Hx3rzfpoKA/"
      "edit?ouid=103677288878638916900&usp=docs_home&ths=true";

  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    content::RenderFrame* render_frame =
        content::RenderFrame::FromWebFrame(GetMainFrame());
    controller_ = ReadAnythingAppController::Install(render_frame);

    if (forced_distillation_method_) {
      controller_->set_forced_distillation_method_for_testing(
          *forced_distillation_method_);
    }

    // Set the page handler for testing.
    controller_->page_handler_.reset();
    controller_->page_handler_.Bind(page_handler_.BindNewPipeAndPassRemote());
    EXPECT_CALL(page_handler_, OnDistillationStateChanged(testing::_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(page_handler_, OnDistillationStatus(testing::_, testing::_))
        .Times(testing::AnyNumber());

    // Set distiller for testing.
    auto distiller = std::make_unique<MockAXTreeDistiller>(render_frame);
    distiller_ = distiller.get();
    controller_->distiller_ = std::move(distiller);

    // Create a tree id.
    tree_id_ = ui::AXTreeID::CreateNewAXTreeID();

    // Set presentation state to opened so distillation is not paused.
    controller_->OnGetPresentationState(
        read_anything::mojom::ReadAnythingPresentationState::kInSidePanel);

    DoInitialDistillation();
    // Stop the distillation status logging timer started during setup to
    // prevent it from firing or logging during test cases.
    controller_->distillation_status_logging_delay_timer_.Stop();
    controller_->has_logged_distillation_status_ = false;
  }

  virtual void DoInitialDistillation() {
    // Create simple AXTreeUpdate with a root node and 3 children.
    std::unique_ptr<ui::AXTreeUpdate> snapshot = test::CreateInitialUpdate();
    test::SetUpdateTreeID(snapshot.get(), tree_id_);

    // Send the snapshot to the controller and set its tree ID to be the active
    // tree ID. When the accessibility event is received and unserialized, the
    // controller will call distiller_->Distill().
    ExpectDistill(1);
    AccessibilityEventReceived({*snapshot});
    controller().OnActiveAXTreeIDChanged(tree_id_, ukm::kInvalidSourceId,
                                         false);
    OnAXTreeDistilled();
    Mock::VerifyAndClearExpectations(distiller_);
  }

  void ExpectDistill(int times) {
    // When Readability is in-use, the Screen2x distiller isn't used, so
    // there will never be a Distill callback.
    int expected_times = (model().current_content_distillation_method() ==
                          ReadAnythingAppModel::DistillationMethod::kScreen2x)
                             ? times
                             : 0;

    EXPECT_CALL(*distiller_, Distill).Times(expected_times);
  }

  void OnAXTreeDistilled() { OnAXTreeDistilled(tree_id_, {}); }

  void OnAXTreeDistilled(const ui::AXTreeID& tree_id,
                         const std::vector<ui::AXNodeID>& content_node_ids) {
    // In production code, OnAXTreeDistilled shouldn't be called when
    // Readability is being used, so tests will crash if they try to call it
    // directly.
    if (model().current_content_distillation_method() !=
        ReadAnythingAppModel::DistillationMethod::kScreen2x) {
      return;
    }
    controller().OnAXTreeDistilled(tree_id, content_node_ids);
  }

  void TearDown() override {
    // `controller_` owns `distiller_` and RenderFrame (indirectly) owns
    // `controller_`: it is a garbage-collected object owned by Oilpan and its
    // lifetime is tied to the RenderFrame's lifetime.
    controller_ = nullptr;
    distiller_ = nullptr;
    ChromeRenderViewTest::TearDown();
  }

  ReadAnythingAppController& controller() { return *controller_; }
  ReadAnythingAppModel& model() { return controller_->model_; }
  ReadAloudAppModel& read_aloud_model() {
    return controller_->read_aloud_model_;
  }

  void Distill() { controller_->Distill(); }
  void LogSpeechStop(int source) { controller_->LogSpeechStop(source); }
  void ProcessModelUpdates() { controller_->ProcessModelUpdates(); }
  bool IsControllerHidden() const { return controller_->IsHidden(); }
  bool IsPdfDrawDebouncerRunning() const {
    return controller_->pdf_draw_debouncer_->IsRunning();
  }
  bool IsNodePendingDeletion(ui::AXNodeID node_id) {
    return model().displayed_nodes_pending_deletion().contains(node_id);
  }
  void RecordSessionMetricsIfShownOrRecentlyHidden(bool recently_hidden) {
    controller_->RecordSessionMetricsIfShownOrRecentlyHidden(recently_hidden);
    page_handler_.FlushForTesting();
  }
  void RecordScreen2xDistillationStatus(bool just_hidden = false) {
    controller_->RecordScreen2xDistillationStatus(just_hidden);
    page_handler_.FlushForTesting();
  }
  void VerifyAndClearPageHandlerExpectations() {
    page_handler_.FlushForTesting();
    Mock::VerifyAndClearExpectations(&page_handler_);
    EXPECT_CALL(page_handler_, OnDistillationStateChanged(testing::_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(page_handler_, OnDistillationStatus(testing::_, testing::_))
        .Times(testing::AnyNumber());
  }

  void SendBatchUpdates() {
    std::vector<ui::AXTreeUpdate> batch_updates;
    for (int i = 2; i < 5; i++) {
      ui::AXTreeUpdate update;
      test::SetUpdateTreeID(&update, tree_id_);
      ui::AXNodeData node =
          test::TextNode(/* id= */ i, u"Node " + base::NumberToString16(i));
      update.nodes = {std::move(node)};
      batch_updates.push_back(std::move(update));
    }

    AccessibilityEventReceived(batch_updates);
  }

  std::vector<int> SendSimpleUpdateAndGetChildIds() {
    ui::AXTreeUpdate initial_update;
    test::SetUpdateTreeID(&initial_update, tree_id_);
    initial_update.root_id = 1;
    initial_update.nodes.resize(3);
    std::vector<int> child_ids;
    for (int i = 0; i < 3; i++) {
      int id = i + 2;
      child_ids.push_back(id);
      initial_update.nodes[i] = test::TextNodeWithTextFromId(id);
    }
    // No events we care about come about, so there's no distillation.
    ExpectDistill(0);
    AccessibilityEventReceived({std::move(initial_update)});
    EXPECT_EQ(u"234", controller().GetTextContent(1));
    Mock::VerifyAndClearExpectations(distiller_);
    return child_ids;
  }

  void AccessibilityEventReceived(
      const std::vector<ui::AXTreeUpdate>& updates,
      const std::vector<ui::AXEvent>& events = std::vector<ui::AXEvent>()) {
    controller().AccessibilityEventReceived(updates[0].tree_data.tree_id,
                                            updates, events);
  }

  std::u16string MoveToNextGranularityAndGetText() {
    controller().MovePositionToNextGranularity();
    return controller().GetCurrentTextContent();
  }

  std::vector<ReadAloudTextSegment> MoveToNextGranularityAndGetSegments() {
    controller().MovePositionToNextGranularity();
    return GetCurrentTextSegments(model().GetCurrentlyVisibleNodes());
  }

  std::vector<ReadAloudTextSegment> MoveToPreviousGranularityAndGetSegments() {
    controller().MovePositionToPreviousGranularity();
    return GetCurrentTextSegments(model().GetCurrentlyVisibleNodes());
  }

  void ProcessDisplayNodes(const std::vector<ui::AXNodeID>& content_node_ids) {
    model().Reset(content_node_ids);
    model().ComputeDisplayNodeIdsForDistilledTree();
  }

  void SendUpdateWithNodes(std::vector<ui::AXNodeData> nodes) {
    ui::AXTreeUpdate update;
    test::SetUpdateTreeID(&update, tree_id_);
    update.nodes = nodes;
    AccessibilityEventReceived({std::move(update)});
  }

  void SendUpdateAndDistillNodes(std::vector<ui::AXNodeData> nodes) {
    SendUpdateWithNodes(nodes);

    std::vector<int> node_ids;
    for (const ui::AXNodeData& node : nodes) {
      node_ids.push_back(node.id);
    }

    OnAXTreeDistilled(tree_id_, node_ids);
    controller().InitAXPositionWithNode(nodes[0].id);
  }

  void InitializeWithAndProcessNodes(std::vector<ui::AXNodeData> nodes) {
    SendUpdateWithNodes(nodes);

    std::vector<int> node_ids;
    for (const ui::AXNodeData& node : nodes) {
      node_ids.push_back(node.id);
    }

    ProcessDisplayNodes(node_ids);
    controller().InitAXPositionWithNode(nodes[0].id);
  }

  void EnableReadAnythingTranslateEntryPoint() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeature(
        features::kReadAnythingTranslateEntryPoint);
  }

  void EnableLineFocus() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeature(features::kReadAnythingLineFocus);
  }

  void DisableLineFocus() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(
        features::kReadAnythingLineFocus);
  }

  void StartLineFocusSession() { controller_->StartLineFocusSession(); }

  void LogLineFocusSession() { controller_->LogLineFocusSession(); }

  void AddLineFocusScrollDistance(int distance) {
    controller_->AddLineFocusScrollDistance(distance);
  }

  void AddLineFocusMouseDistance(int distance) {
    controller_->AddLineFocusMouseDistance(distance);
  }

  void IncrementLineFocusKeyboardLines() {
    controller_->IncrementLineFocusKeyboardLines();
  }

  void IncrementLineFocusSpeechLines() {
    controller_->IncrementLineFocusSpeechLines();
  }

  void ExpectNodesMapToEntireText(std::vector<ReadAloudTextSegment> segments,
                                  std::vector<ui::AXNodeID> node_ids,
                                  std::vector<std::u16string> texts) {
    EXPECT_EQ(segments.size(), node_ids.size());
    EXPECT_EQ(segments.size(), texts.size());
    for (int i = 0; i < segments.size(); i++) {
      EXPECT_EQ(segments.at(i).id, node_ids.at(i));
      EXPECT_EQ(segments.at(i).text_start, 0);
      EXPECT_EQ(segments.at(i).text_end, texts.at(i).length());
    }
  }

  void ExpectCurrentSegments(
      std::vector<ReadAloudTextSegment> expected_segments) {
    std::vector<ReadAloudTextSegment> segments = GetCurrentTextSegments();
    EXPECT_EQ(segments.size(), expected_segments.size());
    for (int i = 0; i < segments.size(); i++) {
      EXPECT_EQ(segments.at(i).id, expected_segments.at(i).id);
      EXPECT_EQ(segments.at(i).text_start, expected_segments.at(i).text_start);
      EXPECT_EQ(segments.at(i).text_end, expected_segments.at(i).text_end);
    }
  }

  void MoveToNextAndAssertEmpty() {
    EXPECT_EQ(MoveToNextGranularityAndGetText(), u"");
  }

  std::vector<ReadAloudTextSegment> GetCurrentTextSegments(
      bool is_pdf = false,
      bool is_docs = false) {
    return read_aloud_model().GetCurrentTextSegments(
        is_pdf, is_docs, model().GetCurrentlyVisibleNodes());
  }

  struct V8Environment {
    explicit V8Environment(blink::WebLocalFrame* frame)
        : isolate(frame->GetAgentGroupScheduler()->Isolate()),
          handle_scope(isolate),
          context(frame->MainWorldScriptContext()),
          context_scope(context) {}

    raw_ptr<v8::Isolate> isolate;
    v8::HandleScope handle_scope;
    v8::Local<v8::Context> context;
    v8::Context::Scope context_scope;
  };

  std::unique_ptr<V8Environment> SetUpV8Environment() {
    return std::make_unique<V8Environment>(GetMainFrame());
  }

  static constexpr ui::AXNodeID kId1 = 2;
  static constexpr ui::AXNodeID kId2 = 3;
  static constexpr ui::AXNodeID kId3 = 4;
  static constexpr ui::AXNodeID kId4 = 12;
  static constexpr ui::AXNodeID kId5 = 100;

  std::optional<ReadAnythingAppModel::DistillationMethod>
      forced_distillation_method_;

  ui::AXTreeID tree_id_;
  raw_ptr<MockAXTreeDistiller> distiller_ = nullptr;
  testing::StrictMock<MockReadAnythingUntrustedPageHandler> page_handler_;
  base::test::ScopedFeatureList scoped_feature_list_;

  // ReadAnythingAppController constructor and destructor are protected so
  // it's not accessible by std::make_unique.
  raw_ptr<ReadAnythingAppController> controller_ = nullptr;
};

TEST_F(ReadAnythingAppControllerTest, GetImageBitmap_ValidNode) {
  ui::AXNodeData node;
  node.id = 2;
  node.role = ax::mojom::Role::kImage;
  node.relative_bounds.bounds = gfx::RectF(0, 0, 100, 100);
  SendUpdateWithNodes({std::move(node)});

  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  controller().OnImageDataDownloaded(tree_id_, 2, bitmap);

  auto v8_env = SetUpV8Environment();

  v8::Local<v8::Value> result = controller().GetImageBitmap(2);
  EXPECT_FALSE(result->IsUndefined());
  EXPECT_TRUE(result->IsObject());
}

class ReadAnythingAppControllerReadabilityTest
    : public ReadAnythingAppControllerTest {
 public:
  ReadAnythingAppControllerReadabilityTest() = default;
  ~ReadAnythingAppControllerReadabilityTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnythingWithReadability}, {});

    ChromeRenderViewTest::SetUp();
    content::RenderFrame* render_frame =
        content::RenderFrame::FromWebFrame(GetMainFrame());
    controller_ = ReadAnythingAppController::Install(render_frame);

    // Set the page handler for testing.
    controller_->page_handler_.reset();
    controller_->page_handler_.Bind(page_handler_.BindNewPipeAndPassRemote());
    EXPECT_CALL(page_handler_, OnDistillationStateChanged(testing::_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(page_handler_, OnDistillationStatus(testing::_, testing::_))
        .Times(testing::AnyNumber());

    // Set distiller for testing.
    auto distiller = std::make_unique<MockAXTreeDistiller>(render_frame);
    distiller_ = distiller.get();
    controller_->distiller_ = std::move(distiller);

    // Create a tree id and tell the controller it's the active one.
    tree_id_ = ui::AXTreeID::CreateNewAXTreeID();
    ui::AXTreeUpdate snapshot;
    ui::AXNodeData root;
    root.id = 1;
    snapshot.root_id = root.id;
    snapshot.nodes = {std::move(root)};
    test::SetUpdateTreeID(&snapshot, tree_id_);
    AccessibilityEventReceived({std::move(snapshot)});
    controller().OnGetPresentationState(
        read_anything::mojom::ReadAnythingPresentationState::kInSidePanel);
    controller().OnActiveAXTreeIDChanged(tree_id_, ukm::kInvalidSourceId,
                                         /*is_pdf=*/false);
  }
};

TEST_F(ReadAnythingAppControllerReadabilityTest,
       GetDomDistillerAnchors_ReturnsCorrectMapping) {
  std::string url = "https://www.google.com";
  std::string link_text = "Google Homepage";
  std::string title_text = "Google Search Tooltip";
  std::string text_before = "Visit ";
  std::string text_after = " now.";

  ui::AXNodeData text_prev;
  text_prev.id = 2;
  text_prev.role = ax::mojom::Role::kStaticText;
  text_prev.SetName(text_before);

  ui::AXNodeData link_node;
  link_node.id = 3;
  link_node.role = ax::mojom::Role::kLink;
  link_node.SetName(link_text);
  link_node.AddStringAttribute(ax::mojom::StringAttribute::kUrl, url);
  link_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlId,
                               "link-id-1");
  link_node.AddStringAttribute(ax::mojom::StringAttribute::kLinkTarget,
                               "_blank");
  link_node.AddStringAttribute(ax::mojom::StringAttribute::kTooltip,
                               title_text);

  ui::AXNodeData text_next;
  text_next.id = 4;
  text_next.role = ax::mojom::Role::kStaticText;
  text_next.SetName(text_after);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {text_prev.id, link_node.id, text_next.id};

  SendUpdateWithNodes({std::move(root), std::move(text_prev),
                       std::move(link_node), std::move(text_next)});
  model().set_should_extract_anchors_from_tree_for_readability(true);
  model().ProcessAXTreeAnchors();
  auto v8_env = SetUpV8Environment();
  v8::MicrotasksScope microtasks_scope(
      v8_env->isolate, v8_env->context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::Value> result = controller().GetDomDistillerAnchors();

  // Verify that the result is a V8 object mapping URLs to arrays of anchor
  // objects extracted from the active accessibility tree.
  ASSERT_TRUE(result->IsObject());
  v8::Local<v8::Object> result_obj = result.As<v8::Object>();
  gin::Dictionary result_dict(v8_env->isolate, result_obj);
  v8::Local<v8::Value> array_val;

  // Verify that the object contains an entry for the anchor URL.
  EXPECT_TRUE(result_dict.Get(url, &array_val));
  ASSERT_TRUE(array_val->IsArray());
  v8::Local<v8::Array> array = array_val.As<v8::Array>();
  EXPECT_EQ(array->Length(), 1u);

  // Verify that the link object properties match the AX node data.
  v8::Local<v8::Value> item = array->Get(v8_env->context, 0).ToLocalChecked();
  ASSERT_TRUE(item->IsObject());
  v8::Local<v8::Object> link_obj = item.As<v8::Object>();
  gin::Dictionary link_dict(v8_env->isolate, link_obj);
  int axId;
  EXPECT_TRUE(link_dict.Get("axId", &axId));
  EXPECT_EQ(axId, 3);
  std::string htmlId, target, title, text, textBefore, textAfter;
  EXPECT_TRUE(link_dict.Get("htmlId", &htmlId));
  EXPECT_EQ(htmlId, "link-id-1");
  EXPECT_TRUE(link_dict.Get("target", &target));
  EXPECT_EQ(target, "_blank");
  EXPECT_TRUE(link_dict.Get("title", &title));
  EXPECT_EQ(title, title_text);
  EXPECT_TRUE(link_dict.Get("text", &text));
  EXPECT_EQ(text, link_text);
  EXPECT_TRUE(link_dict.Get("textBefore", &textBefore));
  EXPECT_EQ(textBefore, text_before);
  EXPECT_TRUE(link_dict.Get("textAfter", &textAfter));
  EXPECT_EQ(textAfter, text_after);
}

class ReadAnythingAppControllerReadabilitySelectTextTest
    : public ReadAnythingAppControllerTest {
 public:
  ReadAnythingAppControllerReadabilitySelectTextTest() = default;
  ~ReadAnythingAppControllerReadabilitySelectTextTest() override = default;

  void SetUp() override {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnythingWithReadability,
         features::kReadAnythingReadabilitySelectText},
        {});
    ReadAnythingAppControllerTest::SetUp();
    model().set_next_distillation_method(
        ReadAnythingAppModel::DistillationMethod::kReadability);
    model().set_current_content_distillation_method(
        ReadAnythingAppModel::DistillationMethod::kReadability);
    VerifyAndClearPageHandlerExpectations();
  }

 protected:
  void DoInitialDistillation() override {
    // Perform basic navigation setup to initialize timers and the AXTree root,
    // but skip the manual Screen2x distillation callback.
    std::unique_ptr<ui::AXTreeUpdate> snapshot = test::CreateInitialUpdate();
    test::SetUpdateTreeID(snapshot.get(), tree_id_);
    AccessibilityEventReceived({*snapshot});
    controller().OnActiveAXTreeIDChanged(tree_id_, ukm::kInvalidSourceId,
                                         false);
  }
};

TEST_F(ReadAnythingAppControllerReadabilitySelectTextTest,
       GetAXMapping_ReturnsCorrectMapping) {
  ui::AXNodeData node;
  node.id = 2;
  node.role = ax::mojom::Role::kStaticText;
  node.SetName("Hello world");
  SendUpdateWithNodes({std::move(node)});

  controller().OnRenderedTextBlocksAvailable({u"Hello world"});

  auto v8_env = SetUpV8Environment();

  v8::MicrotasksScope microtasks_scope(
      v8_env->isolate, v8_env->context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::Value> result = controller().GetAXMapping(0);

  ASSERT_TRUE(result->IsArray());
  v8::Local<v8::Array> array = result.As<v8::Array>();
  EXPECT_EQ(array->Length(), 1u);

  // Verify the dictionary contents
  v8::Local<v8::Value> item = array->Get(v8_env->context, 0).ToLocalChecked();
  ASSERT_TRUE(item->IsObject());
  v8::Local<v8::Object> obj = item.As<v8::Object>();
  gin::Dictionary dict(v8_env->isolate, obj);
  int axNodeId, start, end, axNodeOffset;
  EXPECT_TRUE(dict.Get("axNodeId", &axNodeId));
  EXPECT_TRUE(dict.Get("start", &start));
  EXPECT_TRUE(dict.Get("end", &end));
  EXPECT_TRUE(dict.Get("axNodeOffset", &axNodeOffset));
  EXPECT_EQ(axNodeId, 2);
  EXPECT_EQ(start, 0);
  EXPECT_EQ(end, 11);
  EXPECT_EQ(axNodeOffset, 0);
}

TEST_F(ReadAnythingAppControllerTest,
       OnReadingModeShown_ListenTrigger_ExecutesSetPlayOnOpen) {
  ExecuteJavaScriptForTests(
      "var setPlayOnOpenCalledCount = 0;"
      "chrome.readingMode.setPlayOnOpen = () => { setPlayOnOpenCalledCount++; "
      "};");

  controller().OnReadingModeShown(
      read_anything::mojom::ReadAnythingOpenTrigger::
          kListenToThisPageContextMenu);

  int set_play_on_open_called_count = 0;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"setPlayOnOpenCalledCount", &set_play_on_open_called_count));
  EXPECT_EQ(1, set_play_on_open_called_count);
}

TEST_F(ReadAnythingAppControllerTest,
       OnReadingModeShown_OtherTrigger_DoesNotExecutePlayOnOpen) {
  ExecuteJavaScriptForTests(
      "var playOnOpenCalledCount = 0;"
      "chrome.readingMode.playOnOpen = () => { playOnOpenCalledCount++; };");

  controller().OnReadingModeShown(
      read_anything::mojom::ReadAnythingOpenTrigger::kOmniboxChip);

  int play_on_open_called_count = 0;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"playOnOpenCalledCount",
                                                 &play_on_open_called_count));
  EXPECT_EQ(0, play_on_open_called_count);
}

TEST_F(
    ReadAnythingAppControllerTest,
    OnReadingModeShown_DuplicateCalls_PreservesAudioPlaybackState_WhenPlaying) {
  ExecuteJavaScriptForTests(
      "var setPlayOnOpenCalledCount = 0;"
      "chrome.readingMode.setPlayOnOpen = () => { setPlayOnOpenCalledCount++; "
      "};");

  EXPECT_CALL(page_handler_, OnReadAloudAudioStateChange(true)).Times(1);
  controller().OnIsSpeechActiveChanged(true);
  controller().OnIsAudioCurrentlyPlayingChanged(true);
  ASSERT_TRUE(read_aloud_model().speech_playing());
  ASSERT_TRUE(read_aloud_model().audio_currently_playing());

  // Duplicate calls with non-listen triggers should preserve playing audio
  // state and not execute setPlayOnOpen.
  controller().OnReadingModeShown(
      read_anything::mojom::ReadAnythingOpenTrigger::kOmniboxChip);
  EXPECT_TRUE(read_aloud_model().speech_playing());
  EXPECT_TRUE(read_aloud_model().audio_currently_playing());
  EXPECT_FALSE(model().will_hide());

  controller().OnReadingModeShown(
      read_anything::mojom::ReadAnythingOpenTrigger::kAppMenu);
  EXPECT_TRUE(read_aloud_model().speech_playing());
  EXPECT_TRUE(read_aloud_model().audio_currently_playing());
  EXPECT_FALSE(model().will_hide());

  int set_play_on_open_called_count = 0;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"setPlayOnOpenCalledCount", &set_play_on_open_called_count));
  EXPECT_EQ(0, set_play_on_open_called_count);

  // Duplicate calls with the listen trigger should preserve playing audio state
  // and execute setPlayOnOpen each time.
  controller().OnReadingModeShown(
      read_anything::mojom::ReadAnythingOpenTrigger::
          kListenToThisPageContextMenu);
  EXPECT_TRUE(read_aloud_model().speech_playing());
  EXPECT_TRUE(read_aloud_model().audio_currently_playing());
  EXPECT_FALSE(model().will_hide());
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"setPlayOnOpenCalledCount", &set_play_on_open_called_count));
  EXPECT_EQ(1, set_play_on_open_called_count);

  controller().OnReadingModeShown(
      read_anything::mojom::ReadAnythingOpenTrigger::
          kListenToThisPageContextMenu);
  EXPECT_TRUE(read_aloud_model().speech_playing());
  EXPECT_TRUE(read_aloud_model().audio_currently_playing());
  EXPECT_FALSE(model().will_hide());
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"setPlayOnOpenCalledCount", &set_play_on_open_called_count));
  EXPECT_EQ(2, set_play_on_open_called_count);
}

TEST_F(
    ReadAnythingAppControllerTest,
    OnReadingModeShown_DuplicateCalls_PreservesAudioPlaybackState_WhenPaused) {
  ExecuteJavaScriptForTests(
      "var setPlayOnOpenCalledCount = 0;"
      "chrome.readingMode.setPlayOnOpen = () => { setPlayOnOpenCalledCount++; "
      "};");

  // Simulate audio having been started and then stopped/paused.
  EXPECT_CALL(page_handler_, OnReadAloudAudioStateChange(true)).Times(1);
  EXPECT_CALL(page_handler_, OnReadAloudAudioStateChange(false)).Times(1);
  controller().OnIsSpeechActiveChanged(true);
  controller().OnIsAudioCurrentlyPlayingChanged(true);
  controller().OnIsSpeechActiveChanged(false);
  controller().OnIsAudioCurrentlyPlayingChanged(false);
  ASSERT_FALSE(read_aloud_model().speech_playing());
  ASSERT_FALSE(read_aloud_model().audio_currently_playing());

  // Duplicate calls with non-listen triggers should preserve paused audio state
  // and not execute setPlayOnOpen.
  controller().OnReadingModeShown(
      read_anything::mojom::ReadAnythingOpenTrigger::kOmniboxChip);
  EXPECT_FALSE(read_aloud_model().speech_playing());
  EXPECT_FALSE(read_aloud_model().audio_currently_playing());
  EXPECT_FALSE(model().will_hide());

  controller().OnReadingModeShown(
      read_anything::mojom::ReadAnythingOpenTrigger::kAppMenu);
  EXPECT_FALSE(read_aloud_model().speech_playing());
  EXPECT_FALSE(read_aloud_model().audio_currently_playing());
  EXPECT_FALSE(model().will_hide());

  int set_play_on_open_called_count = 0;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"setPlayOnOpenCalledCount", &set_play_on_open_called_count));
  EXPECT_EQ(0, set_play_on_open_called_count);

  // Duplicate calls with the listen trigger should preserve paused audio state
  // and execute setPlayOnOpen each time.
  controller().OnReadingModeShown(
      read_anything::mojom::ReadAnythingOpenTrigger::
          kListenToThisPageContextMenu);
  EXPECT_FALSE(read_aloud_model().speech_playing());
  EXPECT_FALSE(read_aloud_model().audio_currently_playing());
  EXPECT_FALSE(model().will_hide());
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"setPlayOnOpenCalledCount", &set_play_on_open_called_count));
  EXPECT_EQ(1, set_play_on_open_called_count);

  controller().OnReadingModeShown(
      read_anything::mojom::ReadAnythingOpenTrigger::
          kListenToThisPageContextMenu);
  EXPECT_FALSE(read_aloud_model().speech_playing());
  EXPECT_FALSE(read_aloud_model().audio_currently_playing());
  EXPECT_FALSE(model().will_hide());
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"setPlayOnOpenCalledCount", &set_play_on_open_called_count));
  EXPECT_EQ(2, set_play_on_open_called_count);
}
