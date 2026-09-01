// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/config/ash_web_ui_config_manager.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/strings/strcat.h"
#include "content/public/browser/webui_config.h"
#include "content/public/browser/webui_config_map.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace ash {

namespace {

AshWebUIConfigManager* g_instance = nullptr;

GURL GetURLFromWebUIConfig(content::WebUIConfig& config) {
  return GURL(base::StrCat(
      {config.scheme(), url::kStandardSchemeSeparator, config.host()}));
}

}  // namespace

// static
AshWebUIConfigManager* AshWebUIConfigManager::GetInstance() {
  return g_instance;
}

AshWebUIConfigManager::AshWebUIConfigManager() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

AshWebUIConfigManager::~AshWebUIConfigManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(g_instance, this);
  Unregister();
  g_instance = nullptr;
}

void AshWebUIConfigManager::AddWebUIConfig(
    std::unique_ptr<content::WebUIConfig> config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(config);
  registered_urls_to_unregister_.push_back(GetURLFromWebUIConfig(*config));
  content::WebUIConfigMap::GetInstance().AddWebUIConfig(std::move(config));
}

void AshWebUIConfigManager::AddUntrustedWebUIConfig(
    std::unique_ptr<content::WebUIConfig> config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(config);
  registered_urls_to_unregister_.push_back(GetURLFromWebUIConfig(*config));
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::move(config));
}

void AshWebUIConfigManager::Unregister() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& url : base::Reversed(registered_urls_to_unregister_)) {
    CHECK(content::WebUIConfigMap::GetInstance().RemoveConfig(url));
  }
  registered_urls_to_unregister_.clear();
}

}  // namespace ash
