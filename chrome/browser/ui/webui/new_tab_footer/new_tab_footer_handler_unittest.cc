// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_handler.h"

#include <memory>

#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class NewTabFooterHandlerExtensionTest
    : public extensions::ExtensionServiceTestBase {
 public:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    InitializeEmptyExtensionService();
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    handler_ = std::make_unique<NewTabFooterHandler>(
        mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandler>(),
        mojo::PendingRemote<new_tab_footer::mojom::NewTabFooterDocument>(),
        web_contents_.get());
  }

  NewTabFooterHandler& handler() { return *handler_; }

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<NewTabFooterHandler> handler_;
};

TEST_F(NewTabFooterHandlerExtensionTest,
       GetExtensionAttributionWithoutExtension) {
  new_tab_footer::mojom::ExtensionAttributionPtr extension_attribution;

  base::MockCallback<NewTabFooterHandler::GetNtpExtensionAttributionCallback>
      callback;
  EXPECT_CALL(callback, Run).WillOnce(MoveArg<0>(&extension_attribution));
  handler().GetNtpExtensionAttribution(callback.Get());
  EXPECT_FALSE(extension_attribution);
}

TEST_F(NewTabFooterHandlerExtensionTest, GetExtensionAttributionWithExtension) {
  // Load NTP extension.
  extensions::TestExtensionDir extension_dir;
  constexpr char kManifest[] = R"(
      {
        "chrome_url_overrides": {
            "newtab": "ext.html"
        },
        "name": "Extension-overridden NTP",
        "manifest_version": 3,
        "version": "0.1"
      })";
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("ext.html"),
                          "<body>Extension-overridden NTP</body>");
  extensions::ChromeTestExtensionLoader extension_loader(profile());
  scoped_refptr<const extensions::Extension> extension =
      extension_loader.LoadExtension(extension_dir.Pack());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registrar()->IsExtensionEnabled(extension->id()));
  // Force activation of the URL override because the usual observer for
  // extension load isn't created in the unit test.
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      profile(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));

  new_tab_footer::mojom::ExtensionAttributionPtr extension_attribution;
  base::MockCallback<NewTabFooterHandler::GetNtpExtensionAttributionCallback>
      callback;
  EXPECT_CALL(callback, Run).WillOnce(MoveArg<0>(&extension_attribution));
  handler().GetNtpExtensionAttribution(callback.Get());
  ASSERT_TRUE(extension_attribution);
  EXPECT_EQ(extension_attribution->name, extension->name());
  EXPECT_EQ(extension_attribution->url,
            net::AppendOrReplaceQueryParameter(
                GURL(chrome::kChromeUIExtensionsURL), "id", extension->id()));
}
