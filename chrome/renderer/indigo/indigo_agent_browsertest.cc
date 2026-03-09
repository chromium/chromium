// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/indigo/indigo_agent.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/indigo/indigo.mojom.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_frame.h"
#include "gin/converter.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "v8/include/v8.h"

namespace indigo {
namespace {

class IndigoAgentBrowserTest : public ChromeRenderViewTest {
 protected:
  mojo::AssociatedRemote<chrome::mojom::IndigoAgent> BindIndigoAgent() {
    // Because this is all done in process without a true browser process,
    // this doesn't end up associated with the same IPC channel as other frame
    // messages. That's sufficient for this test, but too limited to properly
    // test orderings with other events, like frame navigation.
    mojo::AssociatedRemote<chrome::mojom::IndigoAgent> remote;
    mojo::ScopedInterfaceEndpointHandle handle =
        remote.BindNewEndpointAndPassDedicatedReceiver().PassHandle();
    CHECK(interface_registry_.TryBindInterface(
        chrome::mojom::IndigoAgent::Name_, &handle));
    return remote;
  }

  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    // It's a bit inconvenient to extract the IndigoAgent instance created in
    // ChromeContentRendererClient, so we just create and bind a new one here.
    new IndigoAgent(GetMainRenderFrame(), &interface_registry_);
  }

 private:
  base::test::ScopedFeatureList feature_list_{features::kIndigo};
  blink::AssociatedInterfaceRegistry interface_registry_;
};

TEST_F(IndigoAgentBrowserTest, InjectScriptInIsolatedWorld) {
  mojo::AssociatedRemote<chrome::mojom::IndigoAgent> remote = BindIndigoAgent();

  const std::string kScript = "window.indigo_test_var = 'success';";
  const GURL kUrl("https://example.com/test.js");
  const url::Origin kOrigin = url::Origin::Create(kUrl);

  base::test::TestFuture<void> done;
  remote->InjectScript(kScript, kUrl, kOrigin, done.GetCallback());
  ASSERT_TRUE(done.Wait());

  // Verify that the script was executed in the isolated world.
  blink::WebLocalFrame* frame = GetMainRenderFrame()->GetWebFrame();
  v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> check_result =
      frame->ExecuteScriptInIsolatedWorldAndReturnValue(
          ISOLATED_WORLD_ID_INDIGO,
          blink::WebScriptSource(
              blink::WebString::FromUTF8("window.indigo_test_var")),
          blink::BackForwardCacheAware::kAllow);
  ASSERT_FALSE(check_result.IsEmpty());

  std::string result_str;
  ASSERT_TRUE(
      gin::Converter<std::string>::FromV8(isolate, check_result, &result_str));
  EXPECT_EQ("success", result_str);

  // Verify that it was NOT executed in the main world.
  v8::Local<v8::Value> main_result =
      frame->ExecuteScriptAndReturnValue(blink::WebScriptSource(
          blink::WebString::FromUTF8("window.indigo_test_var")));
  EXPECT_TRUE(main_result->IsUndefined());
}

}  // namespace
}  // namespace indigo
