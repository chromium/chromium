// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/indigo/indigo_agent.h"

#include <optional>
#include <string_view>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_discardable_memory_allocator.h"
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
  bool WaitForInvokeError() { return invoke_error_future_.Wait(); }

  // chrome::mojom::IndigoAgentHost:
  void StartImageReplacement(
      mojo::PendingRemote<blink::mojom::ImageReplacement> replacement,
      bool is_primary,
      StartImageReplacementCallback callback) override {
    last_is_primary_ = is_primary;
    last_replacement_remote_.reset();
    last_replacement_remote_.Bind(std::move(replacement));
    replacement_started_.SetValue();
    std::move(callback).Run();
  }

  void ReportInvokeError(chrome::mojom::IndigoInvokeError error) override {
    invoke_error_reported_ = true;
    last_invoke_error_ = error;
    invoke_error_future_.SetValue();
  }

  bool last_is_primary() const { return last_is_primary_; }
  bool invoke_error_reported() const { return invoke_error_reported_; }
  std::optional<chrome::mojom::IndigoInvokeError> last_invoke_error() const {
    return last_invoke_error_;
  }
  mojo::Remote<blink::mojom::ImageReplacement>& last_replacement_remote() {
    return last_replacement_remote_;
  }

 private:
  mojo::AssociatedReceiver<chrome::mojom::IndigoAgentHost> receiver_{this};
  mojo::Remote<blink::mojom::ImageReplacement> last_replacement_remote_;
  base::test::TestFuture<void> replacement_started_;
  base::test::TestFuture<void> invoke_error_future_;
  bool last_is_primary_ = false;
  bool invoke_error_reported_ = false;
  std::optional<chrome::mojom::IndigoInvokeError> last_invoke_error_;
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
    base::DiscardableMemoryAllocator::SetInstance(
        &discardable_memory_allocator_);
    ChromeRenderViewTest::SetUp();

    // It's a bit inconvenient to extract the IndigoAgent instance created in
    // ChromeContentRendererClient, so we just create and bind a new one here.
    new IndigoAgent(GetMainRenderFrame(), &interface_registry_);
  }

  void TearDown() override {
    base::DiscardableMemoryAllocator::SetInstance(nullptr);
    ChromeRenderViewTest::TearDown();
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
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
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

TEST_F(IndigoAgentBrowserTest, InvokeResetInvoke) {
  mojo::AssociatedRemote<chrome::mojom::IndigoAgent> remote = BindIndigoAgent();

  // Inject a script that sets up the indigo agent.
  const std::string kScript = R"(
    window.invoked_count = 0;
    window.reset_count = 0;
    window.indigo.setup({
      invoke: function() {
        window.invoked_count++;
      },
      reset: function() {
        window.reset_count++;
      }
    });
  )";
  const GURL kUrl("https://example.com/test.js");
  const url::Origin kOrigin = url::Origin::Create(kUrl);

  base::test::TestFuture<void> inject_done;
  remote->InjectScript(kScript, kUrl, kOrigin, host_.BindAndPassRemote(),
                       inject_done.GetCallback());
  ASSERT_TRUE(inject_done.Wait());

  base::test::TestFuture<void> done;
  remote->Invoke(done.GetCallback());
  ASSERT_TRUE(done.Wait());
  EXPECT_EQ(1, EvaluateAs<int32_t>("window.invoked_count"));

  done.Clear();
  remote->Reset(done.GetCallback());
  ASSERT_TRUE(done.Wait());
  EXPECT_EQ(1, EvaluateAs<int32_t>("window.reset_count"));

  done.Clear();
  remote->Invoke(done.GetCallback());
  ASSERT_TRUE(done.Wait());
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

TEST_F(IndigoAgentBrowserTest,
       PrimaryImageReplacementFailureTriggersMojoCallback) {
  mojo::AssociatedRemote<chrome::mojom::IndigoAgent> remote = BindIndigoAgent();

  const std::string kScript = R"(
    window.indigo.setup({
      invoke: function() {
        try {
          const div = document.createElement('div');
          document.body.appendChild(div);
          window.indigo.startImageReplacement(div, {disposition: 'primary'});
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

  // Verify exception was thrown in the renderer.
  EXPECT_EQ("Error", EvaluateAs<std::string>("window.exception_name"));
  EXPECT_EQ("Not an HTMLImageElement",
            EvaluateAs<std::string>("window.exception_message"));

  // Verify that the host received the failure notification.
  ASSERT_TRUE(host_.WaitForInvokeError());
  EXPECT_TRUE(host_.invoke_error_reported());
  EXPECT_EQ(
      host_.last_invoke_error(),
      chrome::mojom::IndigoInvokeError::kPrimaryImageReplacementCreationFailed);
}

TEST_F(IndigoAgentBrowserTest, NotifyNoPrimaryImageFoundTriggersMojoCallback) {
  mojo::AssociatedRemote<chrome::mojom::IndigoAgent> remote = BindIndigoAgent();

  const std::string kScript = R"(
    window.indigo.setup({
      invoke: function() {
        window.indigo.notifyNoPrimaryImageFound();
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

  // Verify that the host received the failure notification.
  ASSERT_TRUE(host_.WaitForInvokeError());
  EXPECT_TRUE(host_.invoke_error_reported());
  EXPECT_EQ(host_.last_invoke_error(),
            chrome::mojom::IndigoInvokeError::kNoPrimaryImageFound);
}

TEST_F(IndigoAgentBrowserTest, IsReplacedByUserAgentInIsolatedWorld) {
  mojo::AssociatedRemote<chrome::mojom::IndigoAgent> remote = BindIndigoAgent();

  const std::string kScript = R"(
    window.indigo.setup({
      invoke: function() {
        const img = document.createElement('img');
        img.id = 'test-img';
        document.body.appendChild(img);
        const div = document.createElement('div');
        document.body.appendChild(div);

        window.is_replaced_method = window.indigo.isReplacedByUserAgent(img);
        window.is_replaced_null = window.indigo.isReplacedByUserAgent(null);
        window.is_replaced_div = window.indigo.isReplacedByUserAgent(div);
        window.is_replaced_obj = window.indigo.isReplacedByUserAgent({});
        window.is_replaced_str = window.indigo.isReplacedByUserAgent('test');
        window.is_replaced_num = window.indigo.isReplacedByUserAgent(123);
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

  EXPECT_EQ(false, EvaluateAs<bool>("window.is_replaced_method"));
  EXPECT_EQ(false, EvaluateAs<bool>("window.is_replaced_null"));
  EXPECT_EQ(false, EvaluateAs<bool>("window.is_replaced_div"));
  EXPECT_EQ(false, EvaluateAs<bool>("window.is_replaced_obj"));
  EXPECT_EQ(false, EvaluateAs<bool>("window.is_replaced_str"));
  EXPECT_EQ(false, EvaluateAs<bool>("window.is_replaced_num"));

  // Verify that replacedByUserAgent is NOT exposed in the main world.
  blink::WebLocalFrame* frame = GetMainRenderFrame()->GetWebFrame();
  v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> main_result = frame->ExecuteScriptAndReturnValue(
      blink::WebScriptSource(blink::WebString(
          "document.getElementById('test-img').replacedByUserAgent")));
  EXPECT_TRUE(main_result->IsUndefined());
}

TEST_F(IndigoAgentBrowserTest,
       IsReplacedByUserAgentReturnsTrueAfterReplacement) {
  LoadHTML(
      "<!DOCTYPE html><body><img id='test-img' "
      "src='data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+"
      "ip1sAAAAASUVORK5CYII='></body>");
  mojo::AssociatedRemote<chrome::mojom::IndigoAgent> remote = BindIndigoAgent();

  const std::string kScript = R"(
    window.indigo.setup({
      invoke: function() {
        const img = document.getElementById('test-img');
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

  ASSERT_TRUE(host_.WaitForReplacementStarted());

  // Before replacement is started from the host, verify it is still false.
  EXPECT_EQ(false, EvaluateAs<bool>("window.indigo.isReplacedByUserAgent("
                                    "document.getElementById('test-img'))"));

  // Start replacement on the image from the host and flush the pipe so
  // StartReplacement executes synchronously on the renderer.
  mojo::PendingRemote<blink::mojom::ImageReplacementHost> host_remote;
  std::ignore = host_remote.InitWithNewPipeAndPassReceiver();
  host_.last_replacement_remote()->StartReplacement(std::move(host_remote),
                                                    std::nullopt);
  host_.last_replacement_remote().FlushForTesting();

  // After replacement is active, verify that the method returns true in the
  // isolated world.
  EXPECT_EQ(true, EvaluateAs<bool>("window.indigo.isReplacedByUserAgent("
                                   "document.getElementById('test-img'))"));

  // Verify that it still remains undefined in the main world.
  blink::WebLocalFrame* frame = GetMainRenderFrame()->GetWebFrame();
  v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> main_result = frame->ExecuteScriptAndReturnValue(
      blink::WebScriptSource(blink::WebString(
          "document.getElementById('test-img').replacedByUserAgent")));
  EXPECT_TRUE(main_result->IsUndefined());
}

}  // namespace
}  // namespace indigo
