// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_utils.h"

#include <memory>

#include "chrome/renderer/actor/click_tool.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_view.h"
#include "url/gurl.h"

namespace actor {

class ToolUtilsTest : public ChromeRenderViewTest {
 public:
  ToolUtilsTest() = default;

 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    // Create a basic HTML structure.
    LoadHTML(
        "<div id='div1'>42</div>"
        "<div id='div2'>Goodbye</div>"
        "<iframe id='iframe1' srcdoc='<div id=div3>Nested</div>'></iframe>");
  }

  blink::WebNode GetNodeByHtmlId(const std::string& html_id) {
    blink::WebElement element =
        GetMainRenderFrame()->GetWebFrame()->GetDocument().GetElementById(
            blink::WebString::FromUTF8(html_id));
    if (element.IsNull()) {
      return blink::WebNode();
    }
    return blink::WebNode(element);
  }

  blink::WebNode GetIframeNodeByHtmlId(const std::string& iframe_id_str,
                                       const std::string& html_id_str) {
    const blink::WebElement iframe_element =
        GetMainRenderFrame()->GetWebFrame()->GetDocument().GetElementById(
            blink::WebString::FromUTF8(iframe_id_str));

    const blink::WebElement child_element =
        blink::WebFrame::FromFrameOwnerElement(iframe_element)
            ->ToWebLocalFrame()
            ->GetDocument()
            .GetElementById(blink::WebString::FromUTF8(html_id_str));
    if (child_element.IsNull()) {
      return blink::WebNode();
    }
    return blink::WebNode(child_element);
  }
};

// Tests with valid frame and node ID.
TEST_F(ToolUtilsTest, GetNodeFromId_Valid) {
  blink::WebNode div1 = GetNodeByHtmlId("div1");
  ASSERT_FALSE(div1.IsNull());
  int32_t div1_node_id = div1.GetDomNodeId();

  blink::WebNode node = GetNodeFromId(*GetMainRenderFrame(), div1_node_id);
  ASSERT_FALSE(node.IsNull());
  EXPECT_EQ(div1_node_id, node.GetDomNodeId());
  EXPECT_EQ(div1, node);

  blink::WebNode div2 = GetNodeByHtmlId("div2");
  ASSERT_FALSE(div2.IsNull());
  int32_t div2_node_id = div2.GetDomNodeId();

  blink::WebNode node2 = GetNodeFromId(*GetMainRenderFrame(), div2_node_id);
  ASSERT_FALSE(node2.IsNull());
  EXPECT_EQ(div2_node_id, node2.GetDomNodeId());
  EXPECT_EQ(div2, node2);
}

// Tests with invalid node ID (node does not exist).
TEST_F(ToolUtilsTest, GetNodeFromId_InvalidNodeId) {
  blink::WebNode node = GetNodeFromId(*GetMainRenderFrame(), 123456);
  EXPECT_TRUE(node.IsNull());
}

// Tests a node in a nested frame (iframe).
TEST_F(ToolUtilsTest, GetNodeFromId_NestedFrame) {
  // #div3 exists only in the subframe.
  ASSERT_FALSE(GetNodeByHtmlId("div3"));

  // Get node within the iframe
  blink::WebNode div3_node = GetIframeNodeByHtmlId("iframe1", "div3");
  ASSERT_TRUE(div3_node);
  int32_t div3_node_id = div3_node.GetDomNodeId();
  ASSERT_NE(0, div3_node_id);

  // GetNodeFromId operates on subtrees within a local root. Ensure the
  // subframe node can be found from our local root.
  blink::WebNode node_found_in_child =
      GetNodeFromId(*GetMainRenderFrame(), div3_node_id);
  ASSERT_TRUE(node_found_in_child);
  EXPECT_EQ(div3_node_id, node_found_in_child.GetDomNodeId());
  EXPECT_EQ(div3_node, node_found_in_child);
}

}  // namespace actor
