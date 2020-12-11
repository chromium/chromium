// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_RENDER_VIEW_TEST_H_
#define CHROME_TEST_BASE_CHROME_RENDER_VIEW_TEST_H_

#include <memory>
#include <string>

#include "chrome/renderer/chrome_mock_render_thread.h"
#include "content/public/test/render_view_test.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

class ChromeContentRendererClient;

namespace autofill {
class AutofillAgent;
class TestPasswordAutofillAgent;
class PasswordGenerationAgent;
class AutofillAssistantAgent;
}  // namespace autofill

class ChromeRenderViewTest : public content::RenderViewTest {
 public:
  ChromeRenderViewTest();
  ~ChromeRenderViewTest() override;

 protected:
  // testing::Test
  void SetUp() override;
  void TearDown() override;
  content::ContentClient* CreateContentClient() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;

  // Called from SetUp(). Override to register mojo interfaces.
  virtual void RegisterMainFrameRemoteInterfaces();

  // Initializes commonly needed global state and renderer client parts.
  // Use when overriding CreateContentRendererClient.
  void InitChromeContentRendererClient(ChromeContentRendererClient* client);

  void WaitForAutofillDidAssociateFormControl();

  autofill::TestPasswordAutofillAgent* password_autofill_agent_ = nullptr;
  autofill::PasswordGenerationAgent* password_generation_ = nullptr;
  autofill::AutofillAssistantAgent* autofill_assistant_agent_ = nullptr;
  autofill::AutofillAgent* autofill_agent_ = nullptr;

  std::unique_ptr<service_manager::BinderRegistry> registry_;
  blink::AssociatedInterfaceRegistry associated_interfaces_;

  // Naked pointer as ownership is with content::RenderViewTest::render_thread_.
  ChromeMockRenderThread* chrome_render_thread_ = nullptr;
};

#endif  // CHROME_TEST_BASE_CHROME_RENDER_VIEW_TEST_H_
