// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/element_store.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::IsEmpty;
using ::testing::Not;

class ElementStoreTest : public testing::Test {
 public:
  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);
    element_store_ = std::make_unique<ElementStore>(web_contents_.get());
  }

 protected:
  std::unique_ptr<ElementFinderResult> CreateElement(
      const std::string& object_id) {
    auto element = std::make_unique<ElementFinderResult>();
    element->SetObjectId(object_id);
    element->SetNodeFrameId(web_contents_->GetPrimaryMainFrame()
                                ->GetDevToolsFrameToken()
                                .ToString());
    element->SetRenderFrameHostGlobalId(
        web_contents_->GetPrimaryMainFrame()->GetGlobalId());
    return element;
  }

  // This consumes the element while adding it to simulate the way of the
  // result going out of life.
  void AddElement(const std::string& client_id,
                  std::unique_ptr<ElementFinderResult> element) {
    element_store_->AddElement(client_id, element->dom_object());
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<ElementStore> element_store_;
};

TEST_F(ElementStoreTest, AddElementToStore) {
  auto element = CreateElement("1");
  AddElement("1", std::move(element));

  EXPECT_TRUE(element_store_->HasElement("1"));
  EXPECT_FALSE(element_store_->HasElement("2"));
}

TEST_F(ElementStoreTest, GetElementFromStore) {
  auto element = CreateElement("1");
  element->SetBackendNodeId(1);
  AddElement("1", std::move(element));

  ElementFinderResult result;
  EXPECT_EQ(ACTION_APPLIED,
            element_store_->GetElement("1", &result).proto_status());
  EXPECT_EQ("1", result.object_id());
  EXPECT_EQ(1, *result.backend_node_id());
  EXPECT_THAT(result.node_frame_id(), Not(IsEmpty()));
  EXPECT_EQ(web_contents_->GetPrimaryMainFrame(), result.render_frame_host());
}

TEST_F(ElementStoreTest, GetElementFromStoreWithBadNodeFrameId) {
  auto element = std::make_unique<ElementFinderResult>();
  element->SetObjectId("1");
  element->SetNodeFrameId("unknown");
  AddElement("1", std::move(element));

  ElementFinderResult result;
  EXPECT_EQ(CLIENT_ID_RESOLUTION_FAILED,
            element_store_->GetElement("1", &result).proto_status());
}

TEST_F(ElementStoreTest, GetElementFromStoreWithNoNodeFrameId) {
  auto element = std::make_unique<ElementFinderResult>();
  element->SetObjectId("1");
  element->SetRenderFrameHostGlobalId(
      web_contents_->GetPrimaryMainFrame()->GetGlobalId());
  AddElement("1", std::move(element));

  ElementFinderResult result;
  EXPECT_EQ(ACTION_APPLIED,
            element_store_->GetElement("1", &result).proto_status());
  EXPECT_EQ(web_contents_->GetPrimaryMainFrame(), result.render_frame_host());
}

TEST_F(ElementStoreTest, GetElementFromStoreWithNoGlobalFrameId) {
  auto element = std::make_unique<ElementFinderResult>();
  element->SetObjectId("1");
  element->SetNodeFrameId(
      web_contents_->GetPrimaryMainFrame()->GetDevToolsFrameToken().ToString());
  AddElement("1", std::move(element));

  ElementFinderResult result;
  EXPECT_EQ(CLIENT_ID_RESOLUTION_FAILED,
            element_store_->GetElement("1", &result).proto_status());
}

TEST_F(ElementStoreTest, AddElementToStoreOverwrites) {
  auto element_1 = CreateElement("1");
  auto element_2 = CreateElement("2");

  AddElement("e", std::move(element_1));
  AddElement("e", std::move(element_2));

  ElementFinderResult result;
  EXPECT_EQ(ACTION_APPLIED,
            element_store_->GetElement("e", &result).proto_status());
  EXPECT_EQ("2", result.object_id());
}

TEST_F(ElementStoreTest, GetUnknownElementFromStore) {
  ElementFinderResult result;
  EXPECT_EQ(CLIENT_ID_RESOLUTION_FAILED,
            element_store_->GetElement("1", &result).proto_status());
}

TEST_F(ElementStoreTest, RemoveElementFromStore) {
  auto element = CreateElement("1");
  AddElement("1", std::move(element));

  EXPECT_TRUE(element_store_->RemoveElement("1"));
  EXPECT_FALSE(element_store_->RemoveElement("1"));
}

TEST_F(ElementStoreTest, ClearStore) {
  auto element = CreateElement("1");
  AddElement("1", std::move(element));

  element_store_->Clear();

  EXPECT_FALSE(element_store_->HasElement("1"));
}

}  // namespace
}  // namespace autofill_assistant
