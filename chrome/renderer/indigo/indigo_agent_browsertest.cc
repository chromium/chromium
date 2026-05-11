// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/indigo/indigo_agent.h"

#include <optional>
#include <string_view>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/indigo/indigo.mojom.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_frame.h"
#include "gin/converter.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/image_replacement/image_replacement.mojom.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "v8/include/v8.h"

namespace indigo {
namespace {

class MockIndigoAgentHost : public chrome::mojom::IndigoAgentHost {
 public:
  MockIndigoAgentHost() = default;
  ~MockIndigoAgentHost() override = default;

  mojo::PendingAssociatedRemote<chrome::mojom::IndigoAgentHost>
  BindAndPassRemote() {
    return receiver_.BindNewEndpointAndPassRemote();
  }

  bool WaitForReplacementStarted() { return replacement_started_.Wait(); }

  // chrome::mojom::IndigoAgentHost:
  void StartImageReplacement(
      mojo::PendingRemote<blink::mojom::ImageReplacement> replacement,
      bool is_primary,
      StartImageReplacementCallback callback) override {
    last_is_primary_ = is_primary;
    replacements_.Add(std::move(replacement));
    replacement_started_.SetValue();
    std::move(callback).Run();
  }

  bool last_is_primary() const { return last_is_primary_; }

 private:
  mojo::AssociatedReceiver<chrome::mojom::IndigoAgentHost> receiver_{this};
  mojo::RemoteSet<blink::mojom::ImageReplacement> replacements_;
  base::test::TestFuture<void> replacement_started_;
  bool last_is_primary_ = false;
};

class IndigoAgentBrowserTest : public ChromeRenderViewTest {
 protected:
  IndigoAgentBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kIndigo, blink::features::kImageReplacement}, {});
  }

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

  template <typename T>
  std::optional<T> EvaluateAs(std::string_view script) {
    blink::WebLocalFrame* frame = GetMainRenderFrame()->GetWebFrame();
    v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Value> result =
        frame->ExecuteScriptInIsolatedWorldAndReturnValue(
            ISOLATED_WORLD_ID_INDIGO,
            blink::WebScriptSource(
                blink::WebString::FromUtf8(std::string(script))),
            blink::BackForwardCacheAware::kAllow);
    if (result.IsEmpty()) {
      return std::nullopt;
    }
    T converted_result;
    if (gin::Converter<T>::FromV8(isolate, result, &converted_result)) {
      return converted_result;
    }
    return std::nullopt;
  }

 protected:
  MockIndigoAgentHost host_;

 private:
  base::test::ScopedFeatureList feature_list_;
  blink::AssociatedInterfaceRegistry interface_registry_;
};

TEST_F(IndigoAgentBrowserTest, InjectScriptInIsolatedWorld) {
  mojo::AssociatedRemote<chrome::mojom::IndigoAgent> remote = BindIndigoAgent();

  const std::string kScript = "window.indigo_test_var = 'success';";
  const GURL kUrl("https://example.com/test.js");
  const url::Origin kOrigin = url::Origin::Create(kUrl);

  base::test::TestFuture<void> done;
  remote->InjectScript(kScript, kUrl, kOrigin, host_.BindAndPassRemote(),
                       done.GetCallback());
  ASSERT_TRUE(done.Wait());

  // Verify that the script was executed in the isolated world.
  EXPECT_EQ("success", EvaluateAs<std::string>("window.indigo_test_var"));

  // Verify that it was NOT executed in the main world.
  blink::WebLocalFrame* frame = GetMainRenderFrame()->GetWebFrame();
  v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> main_result = frame->ExecuteScriptAndReturnValue(
      blink::WebScriptSource(blink::WebString("window.indigo_test_var")));
  EXPECT_TRUE(main_result->IsUndefined());
}

TEST_F(IndigoAgentBrowserTest, IndigoContextIsAvailable) {
  EXPECT_EQ("object", EvaluateAs<std::string>("typeof window.indigo"));
}

TEST_F(IndigoAgentBrowserTest, SetupAndInvoke) {
  mojo::AssociatedRemote<chrome::mojom::IndigoAgent> remote = BindIndigoAgent();

  // Inject a script that sets up the indigo agent.
  const std::string kScript = R"(
    window.invoked_count = 0;
    window.indigo.setup({
      invoke: function() {
        window.invoked_count++;
      }
    });
  )";
  const GURL kUrl("https://example.com/test.js");
  const url::Origin kOrigin = url::Origin::Create(kUrl);

  base::test::TestFuture<void> inject_done;
  remote->InjectScript(kScript, kUrl, kOrigin, host_.BindAndPassRemote(),
                       inject_done.GetCallback());
  ASSERT_TRUE(inject_done.Wait());

  // Now trigger invoke from the native side.
  base::test::TestFuture<void> invoke_done;
  remote->Invoke(invoke_done.GetCallback());
  ASSERT_TRUE(invoke_done.Wait());

  // Verify invoked_count in isolated world.
  EXPECT_EQ(1, EvaluateAs<int32_t>("window.invoked_count"));

  // Trigger again.
  invoke_done.Clear();
  remote->Invoke(invoke_done.GetCallback());
  ASSERT_TRUE(invoke_done.Wait());

  EXPECT_EQ(2, EvaluateAs<int32_t>("window.invoked_count"));
}

TEST_F(IndigoAgentBrowserTest, StartImageReplacementWithNullThrows) {
  mojo::AssociatedRemote<chrome::mojom::IndigoAgent> remote = BindIndigoAgent();

  const std::string kScript = R"(
    window.indigo.setup({
      invoke: function() {
        try {
          window.indigo.startImageReplacement(null);
        } catch (e) {
          window.exception_name = e.name;
          window.exception_message = e.message;
        }
      }
    });
  )";
  const GURL kUrl("https://example.com/test.js");
  const url::Origin kOrigin = url::Origin::Create(kUrl);

  base::test::TestFuture<void> inject_done;
  remote->InjectScript(kScript, kUrl, kOrigin, host_.BindAndPassRemote(),
                       inject_done.GetCallback());
  ASSERT_TRUE(inject_done.Wait());

  base::test::TestFuture<void> invoke_done;
  remote->Invoke(invoke_done.GetCallback());
  ASSERT_TRUE(invoke_done.Wait());

  EXPECT_EQ("TypeError", EvaluateAs<std::string>("window.exception_name"));
  EXPECT_EQ("Invalid element wrapper.",
            EvaluateAs<std::string>("window.exception_message"));
}

TEST_F(IndigoAgentBrowserTest, StartImageReplacementWithValidElement) {
  mojo::AssociatedRemote<chrome::mojom::IndigoAgent> remote = BindIndigoAgent();

  const std::string kScript = R"(
    window.indigo.setup({
      invoke: function() {
        const img = document.createElement('img');
        img.src = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+ip1sAAAAASUVORK5CYII=";
        document.body.appendChild(img);
        window.indigo.startImageReplacement(img);
      }
    });
  )";
  const GURL kUrl("https://example.com/test.js");
  const url::Origin kOrigin = url::Origin::Create(kUrl);

  base::test::TestFuture<void> inject_done;
  remote->InjectScript(kScript, kUrl, kOrigin, host_.BindAndPassRemote(),
                       inject_done.GetCallback());
  ASSERT_TRUE(inject_done.Wait());

  base::test::TestFuture<void> invoke_done;
  remote->Invoke(invoke_done.GetCallback());
  ASSERT_TRUE(invoke_done.Wait());

  // Verify that the host received the replacement start request.
  ASSERT_TRUE(host_.WaitForReplacementStarted());
  EXPECT_TRUE(host_.last_is_primary());
}

TEST_F(IndigoAgentBrowserTest, StartImageReplacementWithPrimaryDisposition) {
  mojo::AssociatedRemote<chrome::mojom::IndigoAgent> remote = BindIndigoAgent();

  const std::string kScript = R"(
    window.indigo.setup({
      invoke: function() {
        const img = document.createElement('img');
        img.src = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+ip1sAAAAASUVORK5CYII=";
        document.body.appendChild(img);
        window.indigo.startImageReplacement(img, {disposition: 'primary'});
      }
    });
  )";
  const GURL kUrl("https://example.com/test.js");
  const url::Origin kOrigin = url::Origin::Create(kUrl);

  base::test::TestFuture<void> inject_done;
  remote->InjectScript(kScript, kUrl, kOrigin, host_.BindAndPassRemote(),
                       inject_done.GetCallback());
  ASSERT_TRUE(inject_done.Wait());

  base::test::TestFuture<void> invoke_done;
  remote->Invoke(invoke_done.GetCallback());
  ASSERT_TRUE(invoke_done.Wait());

  ASSERT_TRUE(host_.WaitForReplacementStarted());
  EXPECT_TRUE(host_.last_is_primary());
}

TEST_F(IndigoAgentBrowserTest, StartImageReplacementWithMirrorDisposition) {
  mojo::AssociatedRemote<chrome::mojom::IndigoAgent> remote = BindIndigoAgent();

  const std::string kScript = R"(
    window.indigo.setup({
      invoke: function() {
        const img = document.createElement('img');
        img.src = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+ip1sAAAAASUVORK5CYII=";
        document.body.appendChild(img);
        window.indigo.startImageReplacement(img, {disposition: 'mirror'});
      }
    });
  )";
  const GURL kUrl("https://example.com/test.js");
  const url::Origin kOrigin = url::Origin::Create(kUrl);

  base::test::TestFuture<void> inject_done;
  remote->InjectScript(kScript, kUrl, kOrigin, host_.BindAndPassRemote(),
                       inject_done.GetCallback());
  ASSERT_TRUE(inject_done.Wait());

  base::test::TestFuture<void> invoke_done;
  remote->Invoke(invoke_done.GetCallback());
  ASSERT_TRUE(invoke_done.Wait());

  ASSERT_TRUE(host_.WaitForReplacementStarted());
  EXPECT_FALSE(host_.last_is_primary());
}

TEST_F(IndigoAgentBrowserTest,
       StartImageReplacementWithInvalidDispositionThrows) {
  mojo::AssociatedRemote<chrome::mojom::IndigoAgent> remote = BindIndigoAgent();

  const std::string kScript = R"(
    window.indigo.setup({
      invoke: function() {
        try {
          const img = document.createElement('img');
          img.src = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+ip1sAAAAASUVORK5CYII=";
          document.body.appendChild(img);
          window.indigo.startImageReplacement(img, {disposition: 'invalid'});
        } catch (e) {
          window.exception_name = e.name;
          window.exception_message = e.message;
        }
      }
    });
  )";
  const GURL kUrl("https://example.com/test.js");
  const url::Origin kOrigin = url::Origin::Create(kUrl);

  base::test::TestFuture<void> inject_done;
  remote->InjectScript(kScript, kUrl, kOrigin, host_.BindAndPassRemote(),
                       inject_done.GetCallback());
  ASSERT_TRUE(inject_done.Wait());

  base::test::TestFuture<void> invoke_done;
  remote->Invoke(invoke_done.GetCallback());
  ASSERT_TRUE(invoke_done.Wait());

  EXPECT_EQ("Error", EvaluateAs<std::string>("window.exception_name"));
  EXPECT_EQ("Invalid disposition value \"invalid\".",
            EvaluateAs<std::string>("window.exception_message"));
}

TEST_F(IndigoAgentBrowserTest,
       StartImageReplacementWithInvalidParamsTypeThrows) {
  mojo::AssociatedRemote<chrome::mojom::IndigoAgent> remote = BindIndigoAgent();

  const std::string kScript = R"(
    window.indigo.setup({
      invoke: function() {
        try {
          const img = document.createElement('img');
          img.src = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+ip1sAAAAASUVORK5CYII=";
          document.body.appendChild(img);
          window.indigo.startImageReplacement(img, 'not an object');
        } catch (e) {
          window.exception_name = e.name;
          window.exception_message = e.message;
        }
      }
    });
  )";
  const GURL kUrl("https://example.com/test.js");
  const url::Origin kOrigin = url::Origin::Create(kUrl);

  base::test::TestFuture<void> inject_done;
  remote->InjectScript(kScript, kUrl, kOrigin, host_.BindAndPassRemote(),
                       inject_done.GetCallback());
  ASSERT_TRUE(inject_done.Wait());

  base::test::TestFuture<void> invoke_done;
  remote->Invoke(invoke_done.GetCallback());
  ASSERT_TRUE(invoke_done.Wait());

  EXPECT_EQ("TypeError", EvaluateAs<std::string>("window.exception_name"));
  EXPECT_EQ("Invalid params object.",
            EvaluateAs<std::string>("window.exception_message"));
}

}  // namespace
}  // namespace indigo
