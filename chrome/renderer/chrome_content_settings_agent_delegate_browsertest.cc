// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_settings_agent_delegate.h"

#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_frame.h"
#include "pdf/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

#if BUILDFLAG(ENABLE_PDF)
#include "third_party/blink/public/web/web_local_frame.h"
#endif  // BUILDFLAG(ENABLE_PDF)

class ChromeContentSettingsAgentDelegateBrowserTest
    : public ChromeRenderViewTest {
 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    // Set up a fake url loader factory to ensure that script loader can create
    // a URLLoader.
    CreateFakeURLLoaderFactory();

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

#if BUILDFLAG(ENABLE_PDF)
// Test that the PDF extension frame is allowlisted for storage access.
TEST_F(ChromeContentSettingsAgentDelegateBrowserTest,
       FrameAllowlistedForStorageAccessPdfExtensionOrigin) {
  // Load HTML with an iframe navigating to the PDF extension URL. Normally,
  // an iframe navigating to the PDF extension URL fails. For testing purposes,
  // it is needed to create a child with the PDF extension origin.
  LoadHTML(
      "<html><iframe "
      "src='chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html'></"
      "iframe></html>");

  auto* delegate =
      ChromeContentSettingsAgentDelegate::Get(GetMainRenderFrame());

  blink::WebFrame* child_frame = GetMainFrame()->FirstChild();
  ASSERT_TRUE(child_frame);

  // The PDF extension frame should be allowlisted for storage access.
  EXPECT_TRUE(delegate->IsFrameAllowlistedForStorageAccess(child_frame));
}
#endif  // BUILDFLAG(ENABLE_PDF)

// Test that a child frame with an origin not allowlisted for storage access
// cannot access it.
TEST_F(ChromeContentSettingsAgentDelegateBrowserTest,
       FrameAllowlistedForStorageAccessFail) {
  // Load HTML with an iframe navigating to a URL without an allowlisted origin.
  LoadHTML("<html><iframe src='https://www.example.com'></iframe></html>");

  auto* delegate =
      ChromeContentSettingsAgentDelegate::Get(GetMainRenderFrame());

  blink::WebFrame* child_frame = GetMainFrame()->FirstChild();
  ASSERT_TRUE(child_frame);

  EXPECT_FALSE(delegate->IsFrameAllowlistedForStorageAccess(child_frame));
}
