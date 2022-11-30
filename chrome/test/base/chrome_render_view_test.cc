// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_render_view_test.h"

#include <memory>

#include "base/debug/leak_annotations.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/autofill_assistant_agent.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/content/renderer/test_password_autofill_agent.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_script_controller.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_view.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/renderer/extensions/chrome_extensions_dispatcher_delegate.h"
#include "chrome/renderer/extensions/chrome_extensions_renderer_client.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/extension.h"
#include "extensions/renderer/dispatcher.h"
#endif

using autofill::AutofillAgent;
using autofill::AutofillAssistantAgent;
using autofill::PasswordAutofillAgent;
using autofill::PasswordGenerationAgent;
using blink::WebFrame;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebScriptController;
using blink::WebScriptSource;
using blink::WebString;
using blink::WebURLRequest;
using content::RenderFrame;
using testing::_;
using testing::NiceMock;
using testing::Return;

namespace {

// An autofill agent that treats all typing as user gesture.
class MockAutofillAgent : public AutofillAgent {
 public:
  MockAutofillAgent(RenderFrame* render_frame,
                    PasswordAutofillAgent* password_autofill_agent,
                    PasswordGenerationAgent* password_generation_agent,
                    AutofillAssistantAgent* autofill_assistant_agent,
                    blink::AssociatedInterfaceRegistry* registry)
      : AutofillAgent(render_frame,
                      password_autofill_agent,
                      password_generation_agent,
                      autofill_assistant_agent,
                      registry) {}

  MockAutofillAgent(const MockAutofillAgent&) = delete;
  MockAutofillAgent& operator=(const MockAutofillAgent&) = delete;
  ~MockAutofillAgent() override = default;

  void WaitForAutofillDidAssociateFormControl() {
    DCHECK(run_loop_ == nullptr);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

 private:
  void DidAssociateFormControlsDynamically() override {
    AutofillAgent::DidAssociateFormControlsDynamically();
    if (run_loop_)
      run_loop_->Quit();
  }

  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

ChromeRenderViewTest::ChromeRenderViewTest() = default;
ChromeRenderViewTest::~ChromeRenderViewTest() = default;

void ChromeRenderViewTest::SetUp() {
  ChromeUnitTestSuite::InitializeProviders();
  ChromeUnitTestSuite::InitializeResourceBundle();

  registry_ = std::make_unique<service_manager::BinderRegistry>();

  // TODO(crbug/862989): Before this SetUp, the test agents defined at the end
  // of this method should be injected into the creation of RenderViewImpl.
  // In the current state, regular agents are created before the test agents.
  content::RenderViewTest::SetUp();

  RegisterMainFrameRemoteInterfaces();

  // RenderFrame doesn't expose its Agent objects, because it has no need to
  // store them directly (they're stored as RenderFrameObserver*).  So just
  // create another set.
  password_autofill_agent_ = new autofill::TestPasswordAutofillAgent(
      GetMainRenderFrame(), &associated_interfaces_);
  password_generation_ = new autofill::PasswordGenerationAgent(
      GetMainRenderFrame(), password_autofill_agent_, &associated_interfaces_);
  autofill_assistant_agent_ =
      new autofill::AutofillAssistantAgent(GetMainRenderFrame());
  autofill_agent_ = new NiceMock<MockAutofillAgent>(
      GetMainRenderFrame(), password_autofill_agent_, password_generation_,
      autofill_assistant_agent_, &associated_interfaces_);
}

void ChromeRenderViewTest::TearDown() {
  base::RunLoop().RunUntilIdle();

#if defined(LEAK_SANITIZER)
  // Do this before shutting down V8 in RenderViewTest::TearDown().
  // http://crbug.com/328552
  __lsan_do_leak_check();
#endif
  content::RenderViewTest::TearDown();
  registry_.reset();
}

content::ContentClient* ChromeRenderViewTest::CreateContentClient() {
  return new ChromeContentClient();
}

content::ContentBrowserClient*
ChromeRenderViewTest::CreateContentBrowserClient() {
  return new ChromeContentBrowserClient();
}

content::ContentRendererClient*
ChromeRenderViewTest::CreateContentRendererClient() {
  ChromeContentRendererClient* client = new ChromeContentRendererClient();
  InitChromeContentRendererClient(client);
  return client;
}

void ChromeRenderViewTest::RegisterMainFrameRemoteInterfaces() {}

void ChromeRenderViewTest::InitChromeContentRendererClient(
    ChromeContentRendererClient* client) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeExtensionsRendererClient* ext_client =
      ChromeExtensionsRendererClient::GetInstance();
  ext_client->SetExtensionDispatcherForTest(
      std::make_unique<extensions::Dispatcher>(
          std::make_unique<ChromeExtensionsDispatcherDelegate>()));
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
  client->InitSpellCheck();
#endif
}

void ChromeRenderViewTest::WaitForAutofillDidAssociateFormControl() {
  static_cast<MockAutofillAgent*>(autofill_agent_)
      ->WaitForAutofillDidAssociateFormControl();
}
