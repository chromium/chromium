// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/renderer_permissions_policy_delegate.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/renderer/process_state.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/mock_render_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extensions_renderer_api_provider.h"
#include "extensions/renderer/test_extensions_renderer_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class RendererPermissionsPolicyDelegateTest : public testing::Test {
 public:
  RendererPermissionsPolicyDelegateTest() = default;

  void SetUp() override {
    testing::Test::SetUp();
    render_thread_ = std::make_unique<content::MockRenderThread>();
    renderer_client_ = std::make_unique<TestExtensionsRendererClient>();
    ExtensionsRendererClient::Set(renderer_client_.get());
    extension_dispatcher_ = std::make_unique<Dispatcher>(
        std::vector<std::unique_ptr<const ExtensionsRendererAPIProvider>>());
    policy_delegate_ = std::make_unique<RendererPermissionsPolicyDelegate>(
        extension_dispatcher_.get());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<content::MockRenderThread> render_thread_;
  std::unique_ptr<ExtensionsRendererClient> renderer_client_;
  std::unique_ptr<Dispatcher> extension_dispatcher_;
  std::unique_ptr<RendererPermissionsPolicyDelegate> policy_delegate_;
};

scoped_refptr<const Extension> CreateTestExtension(const std::string& id) {
  return ExtensionBuilder()
      .SetManifest(
          base::Value::Dict()
              .Set("name", "Extension with ID " + id)
              .Set("version", "1.0")
              .Set("manifest_version", 2)
              .Set("permissions", base::Value::List().Append("<all_urls>")))
      .SetID(id)
      .Build();
}

}  // namespace

// Tests that CanAccessPage returns false for the any process
// which hosts the webstore.
TEST_F(RendererPermissionsPolicyDelegateTest, CannotScriptWebstore) {
  GURL kAnyUrl("http://example.com/");
  scoped_refptr<const Extension> extension(CreateTestExtension("a"));
  std::string error;
  if (base::FeatureList::IsEnabled(features::kInstantUsesSpareRenderer)) {
    // If the feature is enabled, we use is_instant_process flag to check.
    // We need to set the process state to bypass the CHECK as the test code
    // does not trigger `process_state::SetIsInstantProcess()`;
    process_state::SetIsInstantProcess(false);
  }
  EXPECT_TRUE(extension->permissions_data()->CanAccessPage(kAnyUrl, -1, &error))
      << error;

  // Pretend we are in the webstore process. We should not be able to execute
  // script.
  scoped_refptr<const Extension> webstore_extension(
      CreateTestExtension(extensions::kWebStoreAppId));
  RendererExtensionRegistry::Get()->Insert(webstore_extension.get());
  extension_dispatcher_->ActivateExtension(extensions::kWebStoreAppId);
  EXPECT_FALSE(
      extension->permissions_data()->CanAccessPage(kAnyUrl, -1, &error))
      << error;
}

// Tests that CanAccessPage returns false for the instant process.
TEST_F(RendererPermissionsPolicyDelegateTest, CannotScriptInInstantProcess) {
  GURL kAnyUrl("http://example.com/");
  scoped_refptr<const Extension> extension(CreateTestExtension("a"));
  std::string error;
  if (base::FeatureList::IsEnabled(features::kInstantUsesSpareRenderer)) {
    // Pretend we are in the instant process by overriding the instant process
    // param. We should not be able to execute script.
    process_state::SetIsInstantProcess(true);
    EXPECT_FALSE(
        extension->permissions_data()->CanAccessPage(kAnyUrl, -1, &error))
        << error;
    process_state::SetIsInstantProcess(false);
  } else {
    // If the feature is not enabled, we still replying on the command line
    // switch `kInstantProcess`. Pretend we are in the instant process by
    // adding the command line switches. We should not be able to execute
    // script.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kInstantProcess);
    EXPECT_FALSE(
        extension->permissions_data()->CanAccessPage(kAnyUrl, -1, &error))
        << error;
    command_line->RemoveSwitch(switches::kInstantProcess);
  }
}

}  // namespace extensions
