// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/config/ash_web_ui_config_manager.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "content/public/browser/webui_config.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

namespace {

class DestructionTrackingWebUIConfig : public content::WebUIConfig {
 public:
  DestructionTrackingWebUIConfig(std::string_view scheme,
                                 std::string_view host,
                                 std::vector<std::string>* destruction_order)
      : content::WebUIConfig(scheme, host),
        destruction_order_(*destruction_order),
        host_str_(host) {}

  ~DestructionTrackingWebUIConfig() override {
    destruction_order_->push_back(host_str_);
  }

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override {
    return nullptr;
  }

 private:
  const raw_ref<std::vector<std::string>> destruction_order_;
  const std::string host_str_;
};

}  // namespace

class AshWebUIConfigManagerTest : public testing::Test {
 public:
  AshWebUIConfigManagerTest() = default;
  ~AshWebUIConfigManagerTest() override = default;

 protected:
  void AddWebUIConfig(std::unique_ptr<content::WebUIConfig> config) {
    ash_webui_config_manager_.AddWebUIConfig(std::move(config));
  }

  void AddUntrustedWebUIConfig(std::unique_ptr<content::WebUIConfig> config) {
    ash_webui_config_manager_.AddUntrustedWebUIConfig(std::move(config));
  }

  void Unregister() { ash_webui_config_manager_.Unregister(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  AshWebUIConfigManager ash_webui_config_manager_;
};

TEST_F(AshWebUIConfigManagerTest, SingletonLifecycle) {
  EXPECT_TRUE(AshWebUIConfigManager::GetInstance());
}

TEST_F(AshWebUIConfigManagerTest, AddAndUnregisterConfigsInReverseOrder) {
  auto& map = content::WebUIConfigMap::GetInstance();

  const GURL url1("chrome://test-first");
  const GURL url2("chrome://test-second");
  const GURL url3("chrome-untrusted://test-third");

  std::vector<std::string> destruction_order;

  AddWebUIConfig(std::make_unique<DestructionTrackingWebUIConfig>(
      "chrome", "test-first", &destruction_order));
  AddWebUIConfig(std::make_unique<DestructionTrackingWebUIConfig>(
      "chrome", "test-second", &destruction_order));
  AddUntrustedWebUIConfig(std::make_unique<DestructionTrackingWebUIConfig>(
      "chrome-untrusted", "test-third", &destruction_order));

  EXPECT_TRUE(map.GetConfig(nullptr, url1));
  EXPECT_TRUE(map.GetConfig(nullptr, url2));
  EXPECT_TRUE(map.GetConfig(nullptr, url3));

  // Calling Unregister on AshWebUIConfigManager should unregister all tracked
  // configs in reverse (LIFO) order.
  Unregister();

  EXPECT_FALSE(map.GetConfig(nullptr, url1));
  EXPECT_FALSE(map.GetConfig(nullptr, url2));
  EXPECT_FALSE(map.GetConfig(nullptr, url3));

  EXPECT_EQ(destruction_order, (std::vector<std::string>{
                                   "test-third", "test-second", "test-first"}));
}

}  // namespace ash
