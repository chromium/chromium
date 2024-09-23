// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_manager.h"

#include "base/strings/escape.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/custom_handlers/protocol_handler.h"

using custom_handlers::ProtocolHandler;

namespace web_app {

class WebAppProtocolHandlerManagerTest : public WebAppTest {
 protected:
  void SetUp() override {
    WebAppTest::SetUp();

    provider_ = FakeWebAppProvider::Get(profile());
    provider_->SetOsIntegrationManager(
        std::make_unique<FakeOsIntegrationManager>(
            profile(),
            /*file_handler_manager=*/nullptr,
            std::make_unique<WebAppProtocolHandlerManager>(profile())));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  WebAppProtocolHandlerManager& protocol_handler_manager() {
    return provider()
        .os_integration_manager()
        .protocol_handler_manager_for_testing();
  }

  WebAppProvider& provider() { return *provider_; }

  WebAppRegistrar& app_registrar() { return provider().registrar_unsafe(); }

  webapps::AppId CreateWebAppWithProtocolHandlers(
      const GURL& start_url,
      std::vector<apps::ProtocolHandlerInfo> protocol_handler_infos,
      base::flat_set<std::string> allowed_launch_protocols = {},
      base::flat_set<std::string> disallowed_launch_protocols = {}) {
    auto web_app = test::CreateWebApp(start_url);
    const webapps::AppId app_id = web_app->app_id();
    web_app->SetProtocolHandlers(protocol_handler_infos);
    web_app->SetAllowedLaunchProtocols(allowed_launch_protocols);
    web_app->SetDisallowedLaunchProtocols(disallowed_launch_protocols);
    {
      ScopedRegistryUpdate update =
          provider().sync_bridge_unsafe().BeginUpdate();
      update->CreateApp(std::move(web_app));
    }
    return app_id;
  }

  apps::ProtocolHandlerInfo CreateProtocolHandlerInfo(
      const std::string& protocol,
      const GURL& url) {
    apps::ProtocolHandlerInfo protocol_handler_info;
    protocol_handler_info.protocol = protocol;
    protocol_handler_info.url = url;
    return protocol_handler_info;
  }

  std::vector<apps::ProtocolHandlerInfo> CreateDefaultProtocolHandlerInfos() {
    return {CreateProtocolHandlerInfo("web+test",
                                      GURL("http://example.com/test=%s")),
            CreateProtocolHandlerInfo("web+test2",
                                      GURL("http://example.com/test2=%s"))};
  }

 private:
  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_ = nullptr;
};

TEST_F(WebAppProtocolHandlerManagerTest, GetAppProtocolHandlerInfos) {
  std::vector<apps::ProtocolHandlerInfo> protocol_handler_infos =
      CreateDefaultProtocolHandlerInfos();

  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();
  web_app->SetProtocolHandlers(protocol_handler_infos);

  ASSERT_EQ(
      protocol_handler_manager().GetAppProtocolHandlerInfos(app_id).size(), 0U);

  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(web_app));
  }

  std::vector<apps::ProtocolHandlerInfo> handler_infos =
      protocol_handler_manager().GetAppProtocolHandlerInfos(app_id);
  ASSERT_EQ(handler_infos.size(), protocol_handler_infos.size());
  for (size_t i = 0; i < handler_infos.size(); i++) {
    ASSERT_EQ(handler_infos[i], protocol_handler_infos[i]);
  }
}

TEST_F(WebAppProtocolHandlerManagerTest,
       GetAppProtocolHandlerInfosDisallowedProtocols) {
  std::vector<apps::ProtocolHandlerInfo> protocol_handler_infos =
      CreateDefaultProtocolHandlerInfos();
  const webapps::AppId app_id = CreateWebAppWithProtocolHandlers(
      GURL("https://example.com/path"), protocol_handler_infos, {},
      {"web+test"});
  std::vector<apps::ProtocolHandlerInfo> handler_infos =
      protocol_handler_manager().GetAppProtocolHandlerInfos(app_id);
  ASSERT_EQ(handler_infos.size(), 1U);
  ASSERT_EQ(handler_infos[0], protocol_handler_infos[1]);
}

TEST_F(WebAppProtocolHandlerManagerTest, TranslateProtocolUrl) {
  const webapps::AppId app_id = CreateWebAppWithProtocolHandlers(
      GURL("https://example.com/path"), CreateDefaultProtocolHandlerInfos());

  std::optional<GURL> translated_url =
      protocol_handler_manager().TranslateProtocolUrl(app_id,
                                                      GURL("web+test://test"));
  ASSERT_TRUE(translated_url.has_value());
  ASSERT_EQ(translated_url.value(),
            GURL("http://example.com/test=web%2Btest%3A%2F%2Ftest"));

  ASSERT_FALSE(protocol_handler_manager()
                   .TranslateProtocolUrl(app_id, GURL("web+test3://test"))
                   .has_value());
}

TEST_F(WebAppProtocolHandlerManagerTest, GetAppProtocolHandlers) {
  std::vector<apps::ProtocolHandlerInfo> protocol_handler_infos =
      CreateDefaultProtocolHandlerInfos();

  const webapps::AppId app_id = CreateWebAppWithProtocolHandlers(
      GURL("https://example.com/path"), protocol_handler_infos);

  std::vector<ProtocolHandler> handlers =
      protocol_handler_manager().GetAppProtocolHandlers(app_id);
  ASSERT_EQ(handlers.size(), protocol_handler_infos.size());
  for (size_t i = 0; i < handlers.size(); i++) {
    ASSERT_EQ(handlers[i], ProtocolHandler::CreateWebAppProtocolHandler(
                               protocol_handler_infos[i].protocol,
                               protocol_handler_infos[i].url, app_id));
  }
}

TEST_F(WebAppProtocolHandlerManagerTest, GetAllowedHandlersForProtocol) {
  std::vector<apps::ProtocolHandlerInfo> protocol_handler_infos = {
      CreateProtocolHandlerInfo("web+test",
                                GURL("http://example2.com/test=%s"))};

  const webapps::AppId app_id1 = CreateWebAppWithProtocolHandlers(
      GURL("https://example.com/path"), CreateDefaultProtocolHandlerInfos(),
      {"web+test"});
  const webapps::AppId app_id2 = CreateWebAppWithProtocolHandlers(
      GURL("https://example2.com/path"), protocol_handler_infos, {"web+test"});
  std::vector<ProtocolHandler> handlers =
      protocol_handler_manager().GetAllowedHandlersForProtocol("web+test");
  ASSERT_EQ(handlers.size(), 2U);
  for (const ProtocolHandler& handler : handlers) {
    ASSERT_EQ(handler.protocol(), "web+test");
    if (handler.web_app_id() == app_id1) {
      ASSERT_EQ(handler.url(), GURL("http://example.com/test=%s"));
    } else {
      ASSERT_EQ(handler.web_app_id(), app_id2);
      ASSERT_EQ(handler.url(), GURL("http://example2.com/test=%s"));
    }
  }
}

TEST_F(WebAppProtocolHandlerManagerTest, GetDisallowedHandlersForProtocol) {
  std::vector<apps::ProtocolHandlerInfo> protocol_handler_infos = {
      CreateProtocolHandlerInfo("web+test",
                                GURL("http://example2.com/test=%s"))};

  const webapps::AppId app_id1 = CreateWebAppWithProtocolHandlers(
      GURL("https://example.com/path"), CreateDefaultProtocolHandlerInfos(), {},
      {"web+test"});
  const webapps::AppId app_id2 = CreateWebAppWithProtocolHandlers(
      GURL("https://example2.com/path"), protocol_handler_infos, {},
      {"web+test"});
  std::vector<ProtocolHandler> handlers =
      protocol_handler_manager().GetDisallowedHandlersForProtocol("web+test");
  ASSERT_EQ(handlers.size(), 2U);
  for (const ProtocolHandler& handler : handlers) {
    ASSERT_EQ(handler.protocol(), "web+test");
    if (handler.web_app_id() == app_id1) {
      ASSERT_EQ(handler.url(), GURL("http://example.com/test=%s"));
    } else {
      ASSERT_EQ(handler.web_app_id(), app_id2);
      ASSERT_EQ(handler.url(), GURL("http://example2.com/test=%s"));
    }
  }
}

}  // namespace web_app
