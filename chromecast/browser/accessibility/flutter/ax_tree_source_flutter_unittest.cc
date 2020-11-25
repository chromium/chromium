// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/accessibility/flutter/ax_tree_source_flutter.h"

#include <string>

#include "chromecast/browser/accessibility/flutter/flutter_semantics_node_wrapper.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/api/automation_internal/automation_event_router_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"

using ::testing::StrictMock;

using ::gallium::castos::ActionProperties;
using ::gallium::castos::BooleanProperties;
using ::gallium::castos::OnAccessibilityEventRequest;
using ::gallium::castos::OnAccessibilityEventRequest_EventType_ANNOUNCEMENT;
using ::gallium::castos::OnAccessibilityEventRequest_EventType_CONTENT_CHANGED;
using ::gallium::castos::OnAccessibilityEventRequest_EventType_FOCUSED;
using ::gallium::castos::Rect;

namespace content {

class MockTtsPlatformImpl : public TtsPlatform {
 public:
  MockTtsPlatformImpl() = default;
  virtual ~MockTtsPlatformImpl() = default;

  void set_voices(const std::vector<VoiceData>& voices) { voices_ = voices; }

  void set_run_speak_callback(bool value) { run_speak_callback_ = value; }
  void set_is_speaking(bool value) { is_speaking_ = value; }

  // TtsPlatform:
  bool PlatformImplSupported() override { return true; }
  bool PlatformImplInitialized() override { return true; }
  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const VoiceData& voice,
             const UtteranceContinuousParameters& params,
             base::OnceCallback<void(bool)> on_speak_finished) override {
    if (run_speak_callback_)
      std::move(on_speak_finished).Run(true);
    last_spoken_utterance_ = utterance;
  }
  bool IsSpeaking() override { return is_speaking_; }
  bool StopSpeaking() override { return true; }
  void Pause() override {}
  void Resume() override {}
  void GetVoices(std::vector<VoiceData>* out_voices) override {
    *out_voices = voices_;
  }
  bool LoadBuiltInTtsEngine(BrowserContext* browser_context) override {
    return false;
  }
  void WillSpeakUtteranceWithVoice(TtsUtterance* utterance,
                                   const VoiceData& voice_data) override {}
  void SetError(const std::string& error) override {}
  std::string GetError() override { return std::string(); }
  void ClearError() override {}
  const std::string& GetLastSpokenUtterance() { return last_spoken_utterance_; }
  void ClearLastSpokenUtterance() { last_spoken_utterance_ = ""; }
  void Shutdown() override {}

 private:
  std::vector<VoiceData> voices_;
  bool run_speak_callback_ = true;
  bool is_speaking_ = false;
  std::string last_spoken_utterance_;
};

class MockContentBrowserClient : public ContentBrowserClient {
 public:
  ~MockContentBrowserClient() override {}
};

class MockContentClient : public ContentClient {
 public:
  ~MockContentClient() override {}
};

}  // namespace content

namespace chromecast {
namespace accessibility {

class MockAutomationEventRouter
    : public extensions::AutomationEventRouterInterface {
 public:
  MockAutomationEventRouter() {}
  ~MockAutomationEventRouter() override = default;
  void DispatchAccessibilityEvents(
      const ExtensionMsg_AccessibilityEventBundleParams& events) override {
    for (const auto& event : events.events)
      event_count_[event.event_type]++;
  }
  MOCK_METHOD(void,
              DispatchAccessibilityLocationChange,
              (const ExtensionMsg_AccessibilityLocationChangeParams&),
              (override));
  MOCK_METHOD(void,
              DispatchTreeDestroyedEvent,
              (ui::AXTreeID, content::BrowserContext*),
              (override));
  MOCK_METHOD(void,
              DispatchActionResult,
              (const ui::AXActionData&, bool, content::BrowserContext*),
              (override));
  std::map<ax::mojom::Event, int> event_count_;
};

class AXTreeSourceFlutterTest : public testing::Test,
                                public AXTreeSourceFlutter::Delegate {
 public:
  AXTreeSourceFlutterTest()
      : tree_(std::make_unique<AXTreeSourceFlutter>(this,
                                                    &browser_context_,
                                                    &router_)) {
    // Enable by default.
    tree_->SetAccessibilityEnabled(true);
  }
  AXTreeSourceFlutterTest(const AXTreeSourceFlutterTest&) = delete;
  ~AXTreeSourceFlutterTest() override {
    EXPECT_CALL(router_, DispatchTreeDestroyedEvent).Times(1);
  }
  AXTreeSourceFlutterTest& operator=(const AXTreeSourceFlutterTest&) = delete;

 protected:
  void CallNotifyAccessibilityEvent(OnAccessibilityEventRequest* event_data) {
    tree_->NotifyAccessibilityEvent(event_data);
  }

  void SetAccessibilityEnabled(bool value) {
    tree_->SetAccessibilityEnabled(value);
  }

  void CallGetChildren(SemanticsNode* node,
                       std::vector<FlutterSemanticsNode*>* out_children) const {
    FlutterSemanticsNodeWrapper node_data(tree_.get(), node);
    tree_->GetChildren(&node_data, out_children);
  }

  void CallSerializeNode(SemanticsNode* node,
                         std::unique_ptr<ui::AXNodeData>* out_data) const {
    ASSERT_TRUE(out_data);
    FlutterSemanticsNodeWrapper node_data(tree_.get(), node);
    *out_data = std::make_unique<ui::AXNodeData>();
    tree_->SerializeNode(&node_data, out_data->get());
  }

  FlutterSemanticsNode* CallGetFromId(int32_t id) const {
    return tree_->GetFromId(id);
  }

  bool CallGetTreeData(ui::AXTreeData* data) {
    return tree_->GetTreeData(data);
  }

  int GetDispatchedEventCount(ax::mojom::Event type) {
    return router_.event_count_[type];
  }

  void SetRect(Rect* rect, int left, int top, int right, int bottom) {
    rect->set_left(left);
    rect->set_top(top);
    rect->set_right(right);
    rect->set_bottom(bottom);
  }

  SemanticsNode* AddChild(OnAccessibilityEventRequest* event,
                          SemanticsNode* parent,
                          int id,
                          int x,
                          int y,
                          int w,
                          int h,
                          bool click) {
    BooleanProperties* boolean_properties;
    ActionProperties* action_properties;

    SemanticsNode* new_node = event->add_node_data();
    new_node->set_node_id(id);
    Rect* bounds = new_node->mutable_bounds_in_screen();
    SetRect(bounds, x, y, x + w, y + h);
    boolean_properties = new_node->mutable_boolean_properties();
    boolean_properties->set_is_button(click);
    if (click) {
      new_node->set_label("Button");
    }
    action_properties = new_node->mutable_action_properties();
    action_properties->set_tap(click);

    parent->add_child_node_ids(id);
    return new_node;
  }

 private:
  void OnAction(const ui::AXActionData& data) override {}

  // Required for the TestBrowserContext.
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  MockAutomationEventRouter router_;
  std::unique_ptr<AXTreeSourceFlutter> tree_;
};

TEST_F(AXTreeSourceFlutterTest, ReorderChildrenByLayout) {
  BooleanProperties* boolean_properties;
  ActionProperties* action_properties;

  OnAccessibilityEventRequest event;
  event.set_source_id(0);
  event.set_window_id(1);
  event.set_event_type(OnAccessibilityEventRequest_EventType_FOCUSED);

  //     0
  //    / \
  //   1   2
  SemanticsNode* root = event.add_node_data();
  root->set_node_id(0);
  root->add_child_node_ids(1);
  root->add_child_node_ids(2);

  // Child button 1.
  SemanticsNode* button1 = event.add_node_data();
  button1->set_node_id(1);
  button1->set_label("Button");
  boolean_properties = button1->mutable_boolean_properties();
  boolean_properties->set_is_button(true);
  boolean_properties->set_is_hidden(false);
  action_properties = button1->mutable_action_properties();
  action_properties->set_tap(true);

  // Another child button.
  SemanticsNode* button2 = event.add_node_data();
  button2->set_node_id(2);
  button2->set_label("Button");
  boolean_properties = button2->mutable_boolean_properties();
  boolean_properties->set_is_button(true);
  boolean_properties->set_is_hidden(false);
  action_properties = button2->mutable_action_properties();
  action_properties->set_tap(true);

  // Non-overlapping: 2
  //                   1
  // Expect left(2) to right(1)
  Rect* bounds1 = button1->mutable_bounds_in_screen();
  Rect* bounds2 = button2->mutable_bounds_in_screen();
  SetRect(bounds1, 100, 100, 200, 200);
  SetRect(bounds2, 0, 0, 50, 50);

  // Trigger an update which refreshes the computed bounds used for reordering.
  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  std::vector<FlutterSemanticsNode*> order;
  order.clear();
  CallGetChildren(root, &order);
  ASSERT_EQ(2U, order.size());
  EXPECT_EQ(2, order[0]->GetId());
  EXPECT_EQ(1, order[1]->GetId());

  // Non-overlapping:  2
  //                  1
  // Expect left(1) to right(2)
  SetRect(bounds1, 0, 50, 50, 50);
  SetRect(bounds2, 100, 0, 200, 200);
  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(2, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  order.clear();
  CallGetChildren(root, &order);
  ASSERT_EQ(2U, order.size());
  EXPECT_EQ(1, order[0]->GetId());
  EXPECT_EQ(2, order[1]->GetId());

  // Non-overlapping: 1
  //                   2
  // Expect left(1) to right(2)
  SetRect(bounds1, 0, 0, 50, 50);
  SetRect(bounds2, 100, 100, 200, 200);
  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(3, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  order.clear();
  CallGetChildren(root, &order);
  ASSERT_EQ(2U, order.size());
  EXPECT_EQ(1, order[0]->GetId());
  EXPECT_EQ(2, order[1]->GetId());

  // Non-overlapping:  1
  //                  2
  // Expect left(2) to right(1)
  SetRect(bounds1, 100, 0, 200, 50);
  SetRect(bounds2, 0, 100, 50, 200);
  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(4, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  order.clear();
  CallGetChildren(root, &order);
  ASSERT_EQ(2U, order.size());
  EXPECT_EQ(2, order[0]->GetId());
  EXPECT_EQ(1, order[1]->GetId());

  // Overlapping: Same y, 2_x < 1_x
  // Expect left(2) to right(1)
  SetRect(bounds1, 101, 100, 200, 200);
  SetRect(bounds2, 100, 100, 200, 200);
  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(5, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  order.clear();
  CallGetChildren(root, &order);
  ASSERT_EQ(2U, order.size());
  EXPECT_EQ(2, order[0]->GetId());
  EXPECT_EQ(1, order[1]->GetId());

  // Overlapping: Same y, 1_x < 2_x
  // Expect left(1) to right(2)
  SetRect(bounds1, 100, 100, 200, 200);
  SetRect(bounds2, 101, 100, 200, 200);
  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(6, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  order.clear();
  CallGetChildren(root, &order);
  ASSERT_EQ(2U, order.size());
  EXPECT_EQ(1, order[0]->GetId());
  EXPECT_EQ(2, order[1]->GetId());

  // Overlapping, 1st_x == 2nd_x
  // Expect top(2) to bottom(1)
  SetRect(bounds1, 100, 100, 200, 200);
  SetRect(bounds2, 100, 99, 200, 199);
  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(7, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  order.clear();
  CallGetChildren(root, &order);
  ASSERT_EQ(2U, order.size());
  EXPECT_EQ(2, order[0]->GetId());
  EXPECT_EQ(1, order[1]->GetId());

  // Overlapping, 1st_x > 2nd_x
  // Expect left(1) to right(2)
  SetRect(bounds1, 100, 100, 200, 200);
  SetRect(bounds2, 99, 100, 200, 199);
  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(8, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  order.clear();
  CallGetChildren(root, &order);
  ASSERT_EQ(2U, order.size());
  EXPECT_EQ(2, order[0]->GetId());
  EXPECT_EQ(1, order[1]->GetId());

  // Same top,left,x
  // Expect larger(2) to smaller(1)
  SetRect(bounds1, 100, 100, 200, 110);
  SetRect(bounds2, 100, 100, 200, 200);
  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(9, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  order.clear();
  CallGetChildren(root, &order);
  ASSERT_EQ(2U, order.size());
  EXPECT_EQ(2, order[0]->GetId());
  EXPECT_EQ(1, order[1]->GetId());

  // Same top,left,y
  // Expect larger(2) to smaller(1)
  SetRect(bounds1, 100, 100, 110, 200);
  SetRect(bounds2, 100, 100, 200, 200);
  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(10, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  order.clear();
  CallGetChildren(root, &order);
  ASSERT_EQ(2U, order.size());
  EXPECT_EQ(2, order[0]->GetId());
  EXPECT_EQ(1, order[1]->GetId());

  // Same top,left,x
  // Expect larger(1) to smaller(2)
  SetRect(bounds1, 100, 100, 200, 200);
  SetRect(bounds2, 100, 100, 200, 110);
  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(11, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  order.clear();
  CallGetChildren(root, &order);
  ASSERT_EQ(2U, order.size());
  EXPECT_EQ(1, order[0]->GetId());
  EXPECT_EQ(2, order[1]->GetId());

  // Same top,left,y
  // Expect larger(1) to smaller(2)
  SetRect(bounds1, 100, 100, 200, 200);
  SetRect(bounds2, 100, 100, 110, 200);
  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(12, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  order.clear();
  CallGetChildren(root, &order);
  ASSERT_EQ(2U, order.size());
  EXPECT_EQ(1, order[0]->GetId());
  EXPECT_EQ(2, order[1]->GetId());
}

// This tree was taken from real tree data. When the enclosing
// bounds have been computed for children and they don't overlap,
// we should prefer left to right ordering as it aligns
// better with the way the UI is laid out. Preferring top
// to bottom usually results in a strange ordering which
// may make sense if you knew what the enclosing bounds were
// but does not behave with what you would expect visually.
TEST_F(AXTreeSourceFlutterTest, ReorderChildrenByLayoutPreferLeftToRight) {
  OnAccessibilityEventRequest event;

  event.set_source_id(0);
  event.set_window_id(1);
  event.set_event_type(OnAccessibilityEventRequest_EventType_FOCUSED);

  SemanticsNode* root = event.add_node_data();
  root->set_node_id(0);
  Rect* bounds = root->mutable_bounds_in_screen();
  SetRect(bounds, 0, 0, 1280, 800);

  SemanticsNode* child;
  AddChild(&event, root, 17, 0, 0, 422, 800, true);

  child = AddChild(&event, root, 29, 1022, 0, 257, 800, false);
  child = AddChild(&event, child, 30, 1022, 55, 257, 690, true);
  AddChild(&event, child, 31, 1232, 165, 47, 150, false);
  AddChild(&event, child, 32, 1232, 165, 47, 150, false);

  child = AddChild(&event, root, 18, 422, 0, 600, 800, false);
  child = AddChild(&event, child, 19, 422, 55, 570, 690, false);
  AddChild(&event, child, 20, 507, 95, 445, 40, false);
  AddChild(&event, child, 21, 463, 186, 488, 70, true);
  AddChild(&event, child, 22, 463, 283, 488, 70, true);
  AddChild(&event, child, 23, 463, 381, 488, 70, true);
  AddChild(&event, child, 24, 463, 478, 488, 70, true);

  // Enclosing bounds for 29 end up being smaller y
  // than enclosing bounds for 18 but 18 is left of 29
  // so order should be 17,18,29

  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  std::vector<FlutterSemanticsNode*> ordered;
  CallGetChildren(root, &ordered);
  ASSERT_EQ(3U, ordered.size());
  EXPECT_EQ(17, ordered[0]->GetId());
  EXPECT_EQ(18, ordered[1]->GetId());
  EXPECT_EQ(29, ordered[2]->GetId());
}

TEST_F(AXTreeSourceFlutterTest, AccessibleNameComputation) {
  ActionProperties* action_properties;

  OnAccessibilityEventRequest event;
  event.set_source_id(0);
  event.set_window_id(1);
  event.set_event_type(OnAccessibilityEventRequest_EventType_FOCUSED);

  //     0
  //    / \
  //   1   2
  SemanticsNode* root = event.add_node_data();
  root->set_node_id(0);
  root->add_child_node_ids(1);
  root->add_child_node_ids(2);

  // Child.
  SemanticsNode* child1 = event.add_node_data();
  child1->set_node_id(1);

  // Another child.
  SemanticsNode* child2 = event.add_node_data();
  child2->set_node_id(2);

  // Populate the tree source with the data.
  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));

  // No attributes.
  std::unique_ptr<ui::AXNodeData> data;
  CallSerializeNode(root, &data);
  std::string name;
  ASSERT_FALSE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("", name);

  // Label (empty).
  root->set_label("");
  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("", name);

  // Label (non-empty).
  root->set_label("label text");
  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("label text", name);

  // Hint (empty), Label (non-empty).
  root->set_hint("");
  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("label text", name);

  // Hint (non-empty), Label (empty).
  root->set_hint("hint");
  root->clear_label();
  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("hint", name);

  // Name from contents.

  // Root node has no name, but has descendants with name.
  root->clear_hint();
  root->clear_label();
  // Name from contents only happens if a node is clickable.
  action_properties = root->mutable_action_properties();
  action_properties->set_tap(true);
  child1->set_label("child1 label text");
  child2->set_label("child2 label text");
  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("child1 label text child2 label text", name);

  // If the node has a name, it should override the contents.
  root->set_label("root label text");
  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("root label text", name);

  // Clearing both clickable and name from root, the name should not be
  // populated.
  root->clear_label();
  action_properties->clear_tap();
  CallSerializeNode(root, &data);
  ASSERT_FALSE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
}

// Flutter's 'hidden' attribute should not translate to the node being
// ignored or in any way not a11y focusable.
TEST_F(AXTreeSourceFlutterTest, NeverHidden) {
  OnAccessibilityEventRequest event;
  event.set_source_id(0);
  event.set_window_id(1);
  event.set_event_type(OnAccessibilityEventRequest_EventType_FOCUSED);

  BooleanProperties* boolean_properties;
  SemanticsNode* root = event.add_node_data();
  root->add_child_node_ids(1);
  SemanticsNode* child1 = event.add_node_data();
  child1->set_node_id(1);
  child1->set_label("some label text");
  boolean_properties = child1->mutable_boolean_properties();
  boolean_properties->set_is_hidden(true);

  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));

  std::unique_ptr<ui::AXNodeData> data;
  CallSerializeNode(child1, &data);

  ASSERT_TRUE(data->role == ax::mojom::Role::kStaticText);
  ASSERT_FALSE(data->HasState(ax::mojom::State::kInvisible));
}

TEST_F(AXTreeSourceFlutterTest, GetTreeDataAppliesFocus) {
  OnAccessibilityEventRequest event;
  event.set_source_id(5);
  event.set_window_id(1);
  event.set_event_type(OnAccessibilityEventRequest_EventType_FOCUSED);

  SemanticsNode* root = event.add_node_data();
  root->set_node_id(5);

  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  ui::AXTreeData data;

  // If no node claimed focus, the root node should get it.
  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(5, data.focus_id);

  // Add a child node with focus.
  root->add_child_node_ids(6);
  SemanticsNode* child = event.add_node_data();
  child->set_node_id(6);
  child->mutable_boolean_properties()->set_is_focused(true);

  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(2, GetDispatchedEventCount(ax::mojom::Event::kFocus));

  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(6, data.focus_id);
}

TEST_F(AXTreeSourceFlutterTest, LiveRegion) {
  OnAccessibilityEventRequest event;
  event.set_source_id(1);
  event.set_window_id(1);
  event.set_event_type(OnAccessibilityEventRequest_EventType_FOCUSED);

  SemanticsNode* root = event.add_node_data();
  root->set_node_id(10);
  root->add_child_node_ids(1);
  root->add_child_node_ids(2);
  BooleanProperties* boolean_properties = root->mutable_boolean_properties();
  boolean_properties->set_is_live_region(true);

  // Add child nodes.
  SemanticsNode* node1 = event.add_node_data();
  node1->set_node_id(1);
  node1->set_label("text 1");

  SemanticsNode* node2 = event.add_node_data();
  node2->set_node_id(2);
  node2->set_label("text 2");

  CallNotifyAccessibilityEvent(&event);
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));

  std::unique_ptr<ui::AXNodeData> data;
  CallSerializeNode(root, &data);
  std::string status;
  ASSERT_TRUE(data->GetStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, &status));
  ASSERT_EQ(status, "polite");

  EXPECT_EQ(0, GetDispatchedEventCount(ax::mojom::Event::kLiveRegionChanged));

  // Modify text of node1.
  node1->set_label("modified text 1");
  CallNotifyAccessibilityEvent(&event);

  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kLiveRegionChanged));
}

TEST_F(AXTreeSourceFlutterTest, ResetFocus) {
  //
  // tree 1: no child tree
  //
  OnAccessibilityEventRequest event;

  event.set_source_id(0);
  event.set_window_id(1);
  event.set_event_type(OnAccessibilityEventRequest_EventType_FOCUSED);

  SemanticsNode* root = event.add_node_data();
  root->set_node_id(0);
  Rect* bounds = root->mutable_bounds_in_screen();
  SetRect(bounds, 0, 0, 1280, 800);

  SemanticsNode* child;
  AddChild(&event, root, 1, 0, 0, 800, 600, false);

  child = AddChild(&event, root, 2, 0, 0, 400, 600, false);
  child = AddChild(&event, root, 3, 400, 0, 400, 600, false);

  CallNotifyAccessibilityEvent(&event);

  // initial focus on root
  ui::AXTreeData tree_data;
  CallGetTreeData(&tree_data);
  ASSERT_EQ(0, tree_data.focus_id);

  //
  // tree 2: add a node with a child tree id
  //
  OnAccessibilityEventRequest event2;

  event2.set_source_id(0);
  event2.set_window_id(1);
  event2.set_event_type(OnAccessibilityEventRequest_EventType_FOCUSED);

  SemanticsNode* root2 = event2.add_node_data();
  root2->set_node_id(0);
  Rect* bounds2 = root2->mutable_bounds_in_screen();
  SetRect(bounds2, 0, 0, 1280, 800);

  AddChild(&event2, root2, 1, 0, 0, 800, 600, false);

  child = AddChild(&event2, root2, 2, 0, 0, 400, 600, false);
  child = AddChild(&event2, root2, 3, 400, 0, 400, 600, false);
  child = AddChild(&event2, root2, 4, 0, 0, 200, 200, false);
  child->set_plugin_id("1234");

  // focus should move to node with child tree
  CallNotifyAccessibilityEvent(&event2);
  CallGetTreeData(&tree_data);
  ASSERT_EQ(4, tree_data.focus_id);

  //
  // tree 2: back to initial tree
  //
  CallNotifyAccessibilityEvent(&event);
  CallGetTreeData(&tree_data);

  // focus back to root
  ASSERT_EQ(0, tree_data.focus_id);
}

TEST_F(AXTreeSourceFlutterTest, NoClickable) {
  OnAccessibilityEventRequest event;

  event.set_source_id(0);
  event.set_window_id(1);
  event.set_event_type(OnAccessibilityEventRequest_EventType_FOCUSED);

  SemanticsNode* root = event.add_node_data();
  root->set_node_id(0);
  ActionProperties* action_properties = root->mutable_action_properties();
  action_properties->set_tap(true);
  Rect* bounds = root->mutable_bounds_in_screen();
  SetRect(bounds, 0, 0, 1280, 800);

  CallNotifyAccessibilityEvent(&event);
  std::unique_ptr<ui::AXNodeData> data;
  CallSerializeNode(root, &data);

  // No node should get the clickable attribute.
  ASSERT_FALSE(data->GetBoolAttribute(ax::mojom::BoolAttribute::kClickable));
}

// Tests a new node with scopes route will focus and speak
// a child with names route set.
TEST_F(AXTreeSourceFlutterTest, ScopesRoute) {
  // Add node with scopes route and child with names route. Focus
  // should move to node with names route.
  //
  OnAccessibilityEventRequest event;

  event.set_source_id(0);
  event.set_window_id(1);
  event.set_event_type(OnAccessibilityEventRequest_EventType_CONTENT_CHANGED);

  SemanticsNode* root = event.add_node_data();
  root->set_node_id(0);

  SemanticsNode* child1;
  SemanticsNode* child2;
  SemanticsNode* child3;
  SemanticsNode* child4;
  SemanticsNode* child5;
  SemanticsNode* child6;

  child1 = AddChild(&event, root, 1, 0, 0, 1, 1, false);
  child2 = AddChild(&event, child1, 2, 0, 0, 1, 1, false);
  child3 = AddChild(&event, child2, 3, 0, 0, 1, 1, false);
  std::string child_3_label = "Speak This";
  child3->set_label(child_3_label);

  child4 = AddChild(&event, root, 4, 0, 0, 1, 1, false);
  child5 = AddChild(&event, child4, 5, 0, 0, 1, 1, false);
  child6 = AddChild(&event, child5, 6, 0, 0, 1, 1, false);
  std::string child_6_label = "Speak That";
  child6->set_label(child_6_label);

  BooleanProperties* boolean_properties;

  // Set scopes on child2, names on child3
  boolean_properties = child2->mutable_boolean_properties();
  boolean_properties->set_scopes_route(true);
  boolean_properties = child3->mutable_boolean_properties();
  boolean_properties->set_names_route(true);

  // Set scopes on child5, names on child6
  boolean_properties = child5->mutable_boolean_properties();
  boolean_properties->set_scopes_route(true);
  boolean_properties = child6->mutable_boolean_properties();
  boolean_properties->set_names_route(true);

  CallNotifyAccessibilityEvent(&event);

  // focus should have moved to child 6 since that is the first
  // node that will be found with scopes
  ui::AXTreeData tree_data;
  CallGetTreeData(&tree_data);
  ASSERT_EQ(6, tree_data.focus_id);

  // Same tree should not speak the same scopes_route/names_route
  CallNotifyAccessibilityEvent(&event);

  // Now setup another tree but with 5&6 removed. This should
  // make the tree source focus (but not speak) 3

  OnAccessibilityEventRequest event2;
  event2.set_source_id(0);
  event2.set_window_id(1);
  event2.set_event_type(OnAccessibilityEventRequest_EventType_CONTENT_CHANGED);

  root = event2.add_node_data();
  root->set_node_id(0);

  child1 = AddChild(&event2, root, 1, 0, 0, 1, 1, false);
  child2 = AddChild(&event2, child1, 2, 0, 0, 1, 1, false);
  child3 = AddChild(&event2, child2, 3, 0, 0, 1, 1, false);
  child3->set_label(child_3_label);

  // Set scopes on child2, names on child3
  boolean_properties = child2->mutable_boolean_properties();
  boolean_properties->set_scopes_route(true);
  boolean_properties = child3->mutable_boolean_properties();
  boolean_properties->set_names_route(true);

  CallNotifyAccessibilityEvent(&event2);

  // Focus moves to 3
  CallGetTreeData(&tree_data);
  ASSERT_EQ(3, tree_data.focus_id);

  // Now step to have removed node 3 and re-add node 3 back in the tree. In this
  // case, node 3 should be refocused and spoken.

  OnAccessibilityEventRequest event3;
  event3.set_source_id(0);
  event3.set_window_id(1);
  event3.set_event_type(OnAccessibilityEventRequest_EventType_CONTENT_CHANGED);

  root = event3.add_node_data();
  root->set_node_id(0);

  child1 = AddChild(&event3, root, 1, 0, 0, 1, 1, false);
  child2 = AddChild(&event3, child1, 2, 0, 0, 1, 1, false);

  // Set scopes on child2
  boolean_properties = child2->mutable_boolean_properties();
  boolean_properties->set_scopes_route(true);

  CallNotifyAccessibilityEvent(&event3);

  OnAccessibilityEventRequest event4;
  event4.set_source_id(0);
  event4.set_window_id(1);
  event4.set_event_type(OnAccessibilityEventRequest_EventType_CONTENT_CHANGED);

  root = event4.add_node_data();
  root->set_node_id(0);

  child1 = AddChild(&event4, root, 1, 0, 0, 1, 1, false);
  child2 = AddChild(&event4, child1, 2, 0, 0, 1, 1, false);
  child3 = AddChild(&event4, child2, 3, 0, 0, 1, 1, false);
  child3->set_label(child_3_label);

  // Set scopes on child2, names on child3
  boolean_properties = child2->mutable_boolean_properties();
  boolean_properties->set_scopes_route(true);
  boolean_properties = child3->mutable_boolean_properties();
  boolean_properties->set_names_route(true);

  CallNotifyAccessibilityEvent(&event4);

  // Focus moves to 3
  CallGetTreeData(&tree_data);
  ASSERT_EQ(3, tree_data.focus_id);

  // Finally, remove 2
  //
  OnAccessibilityEventRequest event5;
  event5.set_source_id(0);
  event5.set_window_id(1);
  event5.set_event_type(OnAccessibilityEventRequest_EventType_CONTENT_CHANGED);

  root = event5.add_node_data();
  root->set_node_id(0);

  CallNotifyAccessibilityEvent(&event5);

  CallGetTreeData(&tree_data);
  ASSERT_EQ(0, tree_data.focus_id);
}

// A test to ensure a node that had scopes route but never
// had any names route descendant does not cause a refocus.
TEST_F(AXTreeSourceFlutterTest, ScopesRouteNoNames) {
  // Install a mock tts platform
  auto* tts_controller = content::TtsController::GetInstance();
  content::MockTtsPlatformImpl mock_tts_platform;
  tts_controller->SetTtsPlatform(&mock_tts_platform);

  // Setup some mocks required for tts platform impl
  content::MockContentBrowserClient mock_content_browser_client;
  content::MockContentClient client;
  content::SetContentClient(&client);
  content::SetBrowserClientForTesting(&mock_content_browser_client);

  // Add node with scopes route but no names route descendant.
  OnAccessibilityEventRequest event;

  event.set_source_id(0);
  event.set_window_id(1);
  event.set_event_type(OnAccessibilityEventRequest_EventType_CONTENT_CHANGED);

  SemanticsNode* root = event.add_node_data();
  root->set_node_id(0);

  SemanticsNode* child1;
  SemanticsNode* child2;

  child1 = AddChild(&event, root, 1, 0, 0, 1, 1, false);
  child2 = AddChild(&event, child1, 2, 0, 0, 1, 1, false);
  std::string child_2_label = "Don't Speak This";
  child2->set_label(child_2_label);

  BooleanProperties* boolean_properties;

  // Set scopes on child1 but no names on child 2
  boolean_properties = child1->mutable_boolean_properties();
  boolean_properties->set_scopes_route(true);

  mock_tts_platform.ClearLastSpokenUtterance();

  CallNotifyAccessibilityEvent(&event);

  // focus should remain on root
  ui::AXTreeData tree_data;
  CallGetTreeData(&tree_data);

  // Confirm no spoken TTS or focus change
  ASSERT_TRUE(mock_tts_platform.GetLastSpokenUtterance() == "");
  ASSERT_EQ(0, tree_data.focus_id);
  EXPECT_EQ(0, GetDispatchedEventCount(ax::mojom::Event::kFocus));

  // Remove the node with scopes route. Again, no focus or spoken
  // tts is expected.
  OnAccessibilityEventRequest event2;

  event2.set_source_id(0);
  event2.set_window_id(1);
  event2.set_event_type(OnAccessibilityEventRequest_EventType_CONTENT_CHANGED);

  SemanticsNode* root2 = event2.add_node_data();
  root2->set_node_id(0);

  CallNotifyAccessibilityEvent(&event2);

  // Again, confirm no spoken TTS or focus change
  ASSERT_EQ(0, tree_data.focus_id);
  ASSERT_TRUE(mock_tts_platform.GetLastSpokenUtterance() == "");
  EXPECT_EQ(0, GetDispatchedEventCount(ax::mojom::Event::kFocus));

  // Cleanup since the mock will expire at the end of this test.
  tts_controller->SetTtsPlatform(content::TtsPlatform::GetInstance());
}

TEST_F(AXTreeSourceFlutterTest, Announce) {
  // Install a mock tts platform
  auto* tts_controller = content::TtsController::GetInstance();
  content::MockTtsPlatformImpl mock_tts_platform;
  tts_controller->SetTtsPlatform(&mock_tts_platform);

  // Setup some mocks required for tts platform impl
  content::MockContentBrowserClient mock_content_browser_client;
  content::MockContentClient client;
  content::SetContentClient(&client);
  content::SetBrowserClientForTesting(&mock_content_browser_client);

  // Setup an announcement event
  OnAccessibilityEventRequest event;
  SetAccessibilityEnabled(false);
  event.set_event_type(OnAccessibilityEventRequest_EventType_ANNOUNCEMENT);
  event.set_text("Say this please");
  CallNotifyAccessibilityEvent(&event);

  // Child 3 should NOT have been spoken
  ASSERT_EQ(mock_tts_platform.GetLastSpokenUtterance(), "");

  // This time with a11y enabled.
  SetAccessibilityEnabled(true);
  CallNotifyAccessibilityEvent(&event);

  // Child 3 should have been spoken
  ASSERT_EQ(mock_tts_platform.GetLastSpokenUtterance(), "Say this please");

  // Cleanup since the mock will expire at the end of this test.
  tts_controller->SetTtsPlatform(content::TtsPlatform::GetInstance());
}

}  // namespace accessibility
}  // namespace chromecast
