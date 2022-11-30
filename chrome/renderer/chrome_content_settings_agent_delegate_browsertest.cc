// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_settings_agent_delegate.h"

#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

class ChromeContentSettingsAgentDelegateBrowserTest
    : public ChromeRenderViewTest {
 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    // Set up a fake url loader factory to ensure that script loader can create
    // a WebURLLoader.
    CreateFakeWebURLLoaderFactory();

    // Unbind the ContentSettingsAgent interface that would be registered by
    // the ChromeContentSettingsAgent created when the render frame is created.
    GetMainRenderFrame()->GetAssociatedInterfaceRegistry()->RemoveInterface(
        content_settings::mojom::ContentSettingsAgent::Name_);
  }
};

TEST_F(ChromeContentSettingsAgentDelegateBrowserTest,
       PluginsTemporarilyAllowed) {
  // Load some HTML.
  LoadHTML("<html>Foo</html>");

  std::string foo_plugin = "foo";
  std::string bar_plugin = "bar";

  auto* delegate =
      ChromeContentSettingsAgentDelegate::Get(GetMainRenderFrame());
  EXPECT_FALSE(delegate->IsPluginTemporarilyAllowed(foo_plugin));

  // Temporarily allow the "foo" plugin.
  delegate->AllowPluginTemporarily(foo_plugin);
  EXPECT_TRUE(delegate->IsPluginTemporarilyAllowed(foo_plugin));
  EXPECT_FALSE(delegate->IsPluginTemporarilyAllowed(bar_plugin));

  // Simulate same document navigation.
  OnSameDocumentNavigation(GetMainFrame(), true);
  EXPECT_TRUE(delegate->IsPluginTemporarilyAllowed(foo_plugin));
  EXPECT_FALSE(delegate->IsPluginTemporarilyAllowed(bar_plugin));

  // Navigate to a different page.
  LoadHTML("<html>Bar</html>");
  EXPECT_FALSE(delegate->IsPluginTemporarilyAllowed(foo_plugin));
  EXPECT_FALSE(delegate->IsPluginTemporarilyAllowed(bar_plugin));

  // Temporarily allow all plugins.
  delegate->AllowPluginTemporarily(std::string());
  EXPECT_TRUE(delegate->IsPluginTemporarilyAllowed(foo_plugin));
  EXPECT_TRUE(delegate->IsPluginTemporarilyAllowed(bar_plugin));
}
