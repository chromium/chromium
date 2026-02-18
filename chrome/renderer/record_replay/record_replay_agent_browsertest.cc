// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/record_replay/record_replay_agent.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/record_replay/aliases.h"
#include "chrome/common/record_replay/record_replay.mojom.h"
#include "chrome/common/record_replay/record_replay_features.h"
#include "chrome/renderer/record_replay/record_replay_agent_test_api.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace record_replay {

class MockRecordReplayDriver : public mojom::RecordReplayDriver {
 public:
  MockRecordReplayDriver() = default;
  ~MockRecordReplayDriver() override = default;

  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::RecordReplayDriver>(
        std::move(handle)));
  }

  MOCK_METHOD(void,
              OnClick,
              (DomNodeId dom_node_id, Selector selector),
              (override));
  MOCK_METHOD(void,
              OnSelectChanged,
              (DomNodeId dom_node_id, Selector selector, FieldValue value),
              (override));
  MOCK_METHOD(void,
              OnTextChange,
              (DomNodeId dom_node_id, Selector selector, FieldValue text),
              (override));

 private:
  mojo::AssociatedReceiver<mojom::RecordReplayDriver> receiver_{this};
};

class RecordReplayAgentTest : public ChromeRenderViewTest {
 public:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    GetMainRenderFrame()
        ->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            mojom::RecordReplayDriver::Name_,
            base::BindRepeating(&MockRecordReplayDriver::BindPendingReceiver,
                                base::Unretained(&mock_driver_)));
  }

  void TearDown() override {
    ChromeRenderViewTest::TearDown();
  }

  RecordReplayAgent& agent() { return *record_replay_agent_; }
  MockRecordReplayDriver& mock_driver() { return mock_driver_; }

  blink::WebDocument GetDocument() { return GetMainFrame()->GetDocument(); }

  blink::WebElement GetWebElementById(const std::string& id) {
    return GetDocument().GetElementById(blink::WebString::FromUTF8(id));
  }

  DomNodeId GetDomNodeId(const std::string& element_id) {
    return DomNodeId(GetWebElementById(element_id).GetDomNodeId());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kRecordReplayBase};
  MockRecordReplayDriver mock_driver_;
};

// Tests the selector generation through GetElementSelector().
TEST_F(RecordReplayAgentTest, GetElementSelector) {
  LoadHTML(R"(
    <div id="foo"></div>
    <div id="123"></div>
    <div id="!bar"></div>
    <div>
      <span></span>
      <p id="child"></p>
    </div>
  )");

  base::MockCallback<base::OnceCallback<void(Selector)>> cb;

  EXPECT_CALL(cb, Run(Selector("#foo")));
  agent().GetElementSelector(GetDomNodeId("foo"), cb.Get());

  EXPECT_CALL(cb, Run(Selector("[id=\"123\"]")));
  agent().GetElementSelector(GetDomNodeId("123"), cb.Get());

  EXPECT_CALL(cb, Run(Selector("[id=\"!bar\"]")));
  agent().GetElementSelector(GetDomNodeId("!bar"), cb.Get());

  blink::WebElement span = GetDocument().QuerySelector("span");
  EXPECT_CALL(
      cb,
      Run(Selector(
          ":root > BODY:nth-child(2) > DIV:nth-child(4) > SPAN:nth-child(1)")));
  agent().GetElementSelector(DomNodeId(span.GetDomNodeId()), cb.Get());
}

// Tests that GetMatchingElements() includes all elements that match a selector.
TEST_F(RecordReplayAgentTest, GetMatchingElements) {
  LoadHTML(R"(
    <div id=div1 class="match"></div>
    <div id=div2 class="match"></div>
    <div class="no-match"></div>
  )");

  base::MockCallback<base::OnceCallback<void(const std::vector<DomNodeId>&)>>
      cb;
  std::vector<DomNodeId> expected_ids = {GetDomNodeId("div1"),
                                         GetDomNodeId("div2")};
  EXPECT_CALL(cb, Run(expected_ids));
  agent().GetMatchingElements(Selector(".match"), cb.Get());
}

// Tests that DoPaste() changes the value of a text form control.
TEST_F(RecordReplayAgentTest, DoPaste) {
  LoadHTML(R"(<input id="input">)");

  base::MockCallback<base::OnceCallback<void(bool)>> cb;
  EXPECT_CALL(cb, Run(true));
  agent().DoPaste(GetDomNodeId("input"), FieldValue("hello world"), cb.Get());

  blink::WebInputElement input =
      GetWebElementById("input").DynamicTo<blink::WebInputElement>();
  EXPECT_EQ(input.Value().Utf8(), "hello world");
}

// Tests that recording is disabled by default.
TEST_F(RecordReplayAgentTest, NoRecordingByDefault) {
  LoadHTML(R"(<button id="button">Click me</button>)");

  EXPECT_CALL(mock_driver(), OnClick).Times(0);
  test_api(agent()).DidReceiveLeftMouseDownOrGestureTapInNode(
      GetWebElementById("button"));
}

// Tests that recording stops after StopRecording().
TEST_F(RecordReplayAgentTest, NoRecordingAfterStopRecording) {
  LoadHTML(R"(<button id="button">Click me</button>)");
  agent().StartRecording();
  agent().StopRecording();

  EXPECT_CALL(mock_driver(), OnClick).Times(0);
  test_api(agent()).DidReceiveLeftMouseDownOrGestureTapInNode(
      GetWebElementById("button"));
}

// Tests that DoSelect() changes the value of a <select>.
TEST_F(RecordReplayAgentTest, DoSelect) {
  LoadHTML(R"(
    <select id="select">
      <option value="value1">Option 1</option>
      <option value="value2">Option 2</option>
    </select>
  )");

  base::MockCallback<base::OnceCallback<void(bool)>> cb;
  EXPECT_CALL(cb, Run(true));
  agent().DoSelect(GetDomNodeId("select"), FieldValue("value2"), cb.Get());

  blink::WebFormControlElement select =
      GetWebElementById("select").DynamicTo<blink::WebFormControlElement>();
  EXPECT_EQ(select.Value().Utf8(), "value2");
}

// Tests that clicks are recorded.
TEST_F(RecordReplayAgentTest, RecordingClick) {
  LoadHTML(R"(<button id="button">Click me</button>)");
  agent().StartRecording();

  EXPECT_CALL(mock_driver(),
              OnClick(GetDomNodeId("button"), Selector("#button")));
  // TODO(b/483386299): Consider faking the click in Blink once
  // WebRecordReplayClient has landed.
  test_api(agent()).DidReceiveLeftMouseDownOrGestureTapInNode(
      GetWebElementById("button"));
}

// Tests that value changes of <select> elements are recorded.
TEST_F(RecordReplayAgentTest, RecordingSelect) {
  LoadHTML(R"(
    <select id="select">
      <option value="value1">A</option>
      <option value="value2">B</option>
    </select>
  )");
  agent().StartRecording();

  EXPECT_CALL(mock_driver(),
              OnSelectChanged(GetDomNodeId("select"), Selector("#select"),
                              FieldValue("value2")));
  blink::WebFormControlElement select =
      GetWebElementById("select").DynamicTo<blink::WebFormControlElement>();
  select.SetValue("value2");
}

// Tests that value changes of text form controls are recorded.
TEST_F(RecordReplayAgentTest, RecordingTextChange) {
  LoadHTML(R"(<input id="input">)");
  agent().StartRecording();

  EXPECT_CALL(mock_driver(),
              OnTextChange(GetDomNodeId("input"), Selector("#input"),
                           FieldValue("new text")));
  blink::WebInputElement input =
      GetWebElementById("input").DynamicTo<blink::WebInputElement>();
  input.SetValue("new text");
  test_api(agent()).TextFieldDidEndEditing(input);
}

}  // namespace record_replay
