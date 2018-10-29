// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_router_ui_helper.h"

#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

TEST(MediaRouterUIHelperTest, GetExtensionNameExtensionPresent) {
  std::string id = "extensionid";
  GURL url = GURL("chrome-extension://" + id);
  std::unique_ptr<extensions::ExtensionRegistry> registry =
      std::make_unique<extensions::ExtensionRegistry>(nullptr);
  scoped_refptr<const extensions::Extension> app =
      extensions::ExtensionBuilder(
          "test app name", extensions::ExtensionBuilder::Type::PLATFORM_APP)
          .SetID(id)
          .Build();

  ASSERT_TRUE(registry->AddEnabled(app));
  EXPECT_EQ("test app name", GetExtensionName(url, registry.get()));
}

TEST(MediaRouterUIHelperTest, GetExtensionNameEmptyWhenNotInstalled) {
  std::string id = "extensionid";
  GURL url = GURL("chrome-extension://" + id);
  std::unique_ptr<extensions::ExtensionRegistry> registry =
      std::make_unique<extensions::ExtensionRegistry>(nullptr);

  EXPECT_EQ("", GetExtensionName(url, registry.get()));
}

TEST(MediaRouterUIHelperTest, GetExtensionNameEmptyWhenNotExtensionURL) {
  GURL url = GURL("https://www.google.com");
  std::unique_ptr<extensions::ExtensionRegistry> registry =
      std::make_unique<extensions::ExtensionRegistry>(nullptr);

  EXPECT_EQ("", GetExtensionName(url, registry.get()));
}

}  // namespace media_router
