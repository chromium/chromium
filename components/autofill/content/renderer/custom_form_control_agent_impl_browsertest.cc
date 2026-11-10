// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/custom_form_control_agent_impl.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/test/render_view_test.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace autofill {
namespace {

// A minimal script that returns an empty manager.
constexpr char kValidScript[] =
    "function createCustomFormControlManager() { return {}; }";

// Any isolated world id that the test doesn't otherwise use.
constexpr int32_t kWorldId = content::ISOLATED_WORLD_ID_CONTENT_END + 1;

using Remote = mojo::AssociatedRemote<mojom::CustomFormControlAgent>;

// Destroys `agent` when the frame is destroyed, like `AutofillAgent` does.
class DestroyAgentOnFrameDestruction : public content::RenderFrameObserver {
 public:
  DestroyAgentOnFrameDestruction(
      content::RenderFrame* render_frame,
      std::unique_ptr<CustomFormControlAgentImpl>& agent)
      : content::RenderFrameObserver(render_frame), agent_(agent) {}

 private:
  void OnDestruct() override { agent_->reset(); }

  const raw_ref<std::unique_ptr<CustomFormControlAgentImpl>> agent_;
};

class CustomFormControlAgentImplTest : public content::RenderViewTest {
 protected:
  void SetUp() override {
    content::RenderViewTest::SetUp();
    agent_ = std::make_unique<CustomFormControlAgentImpl>(GetMainRenderFrame(),
                                                          kWorldId, registry_);
  }

  void TearDown() override {
    agent_.reset();
    content::RenderViewTest::TearDown();
  }

  // Binds `remote` to the agent through `registry`, as the browser does.
  // Returns false if that fails.
  [[nodiscard]] static bool Bind(Remote& remote,
                                 blink::AssociatedInterfaceRegistry& registry) {
    mojo::ScopedInterfaceEndpointHandle handle =
        remote.BindNewEndpointAndPassDedicatedReceiver().PassHandle();
    return registry.TryBindInterface(mojom::CustomFormControlAgent::Name_,
                                     &handle);
  }
  [[nodiscard]] bool Bind(Remote& remote) { return Bind(remote, registry_); }

  // Loads `script` for `document` through `remote` and returns the reply.
  bool LoadUnsafeScript(Remote& remote,
                        const blink::DocumentToken& document,
                        const std::string& script) {
    base::test::TestFuture<bool> loaded;
    remote->LoadUnsafeScript(document, script, loaded.GetCallback());
    return loaded.Get();
  }

  blink::DocumentToken GetDocumentToken() {
    return GetMainFrame()->GetDocument().Token();
  }

  CustomFormControlAgentImpl& agent() { return *agent_; }

 private:
  blink::AssociatedInterfaceRegistry registry_;
  std::unique_ptr<CustomFormControlAgentImpl> agent_;
};

// Tests that the browser can load scripts through the interface and learns
// whether they loaded.
TEST_F(CustomFormControlAgentImplTest, LoadsScripts) {
  Remote remote;
  ASSERT_TRUE(Bind(remote));
  EXPECT_TRUE(LoadUnsafeScript(remote, GetDocumentToken(), kValidScript));
  EXPECT_TRUE(agent().executor().has_manager());

  EXPECT_FALSE(
      LoadUnsafeScript(remote, GetDocumentToken(), "throw new Error();"));
  EXPECT_FALSE(agent().executor().has_manager());
}

// Tests that a script for a previous document, which the browser sent before
// the frame committed a new one, isn't loaded.
TEST_F(CustomFormControlAgentImplTest, IgnoresScriptForPreviousDocument) {
  Remote remote;
  ASSERT_TRUE(Bind(remote));
  const blink::DocumentToken previous_document = GetDocumentToken();
  LoadHTML("<div>New document</div>");
  ASSERT_NE(GetDocumentToken(), previous_document);

  EXPECT_FALSE(LoadUnsafeScript(remote, previous_document, kValidScript));
  EXPECT_FALSE(agent().executor().has_manager());
  // The connection still works for the current document.
  EXPECT_TRUE(LoadUnsafeScript(remote, GetDocumentToken(), kValidScript));
}

// Tests that binding the interface again replaces the previous connection.
TEST_F(CustomFormControlAgentImplTest, RebindReplacesConnection) {
  Remote first;
  ASSERT_TRUE(Bind(first));
  Remote second;
  ASSERT_TRUE(Bind(second));
  EXPECT_TRUE(base::test::RunUntil([&] { return !first.is_connected(); }));
  EXPECT_TRUE(LoadUnsafeScript(second, GetDocumentToken(), kValidScript));
}

// Tests that the reply is dropped, without using the agent after it is
// destroyed, if the script detaches its own frame and the owner destroys the
// agent.
TEST_F(CustomFormControlAgentImplTest, DropsReplyIfScriptDetachesFrame) {
  LoadHTML("<iframe src='about:blank'></iframe>");
  blink::WebLocalFrame* child_frame =
      GetMainFrame()->FirstChild()->ToWebLocalFrame();
  content::RenderFrame* child = content::RenderFrame::FromWebFrame(child_frame);
  blink::AssociatedInterfaceRegistry child_registry;
  auto child_agent = std::make_unique<CustomFormControlAgentImpl>(
      child, kWorldId, child_registry);
  DestroyAgentOnFrameDestruction owner(child, child_agent);
  Remote remote;
  ASSERT_TRUE(Bind(remote, child_registry));

  static constexpr char kDetachingScript[] = R"(
    frameElement.remove();
    function createCustomFormControlManager() { return {}; }
  )";
  base::test::TestFuture<bool> loaded;
  remote->LoadUnsafeScript(child_frame->GetDocument().Token(), kDetachingScript,
                           loaded.GetCallback());
  ASSERT_TRUE(
      base::test::RunUntil([&remote] { return !remote.is_connected(); }));
  EXPECT_FALSE(child_agent);
  EXPECT_FALSE(loaded.IsReady());
}

}  // namespace
}  // namespace autofill
