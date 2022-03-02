// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "components/custom_handlers/protocol_handler.h"
#include "net/base/escape.h"

using custom_handlers::ProtocolHandler;

namespace web_app {

class ProtocolHandlerManagerTest : public WebAppTest {
 protected:
  void SetUp() override {
    WebAppTest::SetUp();

    fake_registry_controller_ =
        std::make_unique<FakeWebAppRegistryController>();
    fake_registry_controller_->SetUp(profile());
    protocol_handler_manager_ =
        std::make_unique<FakeWebAppProtocolHandlerManager>(profile());

    protocol_handler_manager_->SetSubsystems(&app_registrar());

    controller().Init();
  }

  FakeWebAppProtocolHandlerManager& protocol_handler_manager() {
    return *protocol_handler_manager_;
  }

  FakeWebAppRegistryController& controller() {
    return *fake_registry_controller_;
  }

  WebAppRegistrar& app_registrar() { return controller().registrar(); }

 private:
  std::unique_ptr<FakeWebAppRegistryController> fake_registry_controller_;
  std::unique_ptr<FakeWebAppProtocolHandlerManager> protocol_handler_manager_;
};

TEST_F(ProtocolHandlerManagerTest, TestGetHandlersFor) {
  auto web_app = test::CreateWebApp();
  const AppId app_id = web_app->app_id();

  controller().RegisterApp(std::move(web_app));

  apps::ProtocolHandlerInfo protocol_handler1;
  const std::string protocol1 = "web+test";
  protocol_handler1.protocol = protocol1;
  protocol_handler1.url = GURL("http://example.com/test=%s");
  protocol_handler_manager().RegisterProtocolHandler(app_id, protocol_handler1);
  ProtocolHandler handler1 = ProtocolHandler::CreateWebAppProtocolHandler(
      protocol_handler1.protocol, GURL(protocol_handler1.url), app_id);

  apps::ProtocolHandlerInfo protocol_handler2;
  const std::string protocol2 = "web+test";
  protocol_handler2.protocol = protocol2;
  protocol_handler2.url = GURL("http://example.net/test=%s");
  protocol_handler_manager().RegisterProtocolHandler(app_id, protocol_handler2);
  ProtocolHandler handler2 = ProtocolHandler::CreateWebAppProtocolHandler(
      protocol_handler2.protocol, GURL(protocol_handler2.url), app_id);

  std::vector<ProtocolHandler> handlers =
      protocol_handler_manager().GetHandlersFor("web+test");

  ASSERT_EQ(static_cast<size_t>(2), handlers.size());

  ASSERT_EQ(handler1, handlers[0]);
  ASSERT_EQ(handler2, handlers[1]);
}

}  // namespace web_app
