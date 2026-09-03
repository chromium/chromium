// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CONFIG_ASH_WEB_UI_CONFIG_MANAGER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CONFIG_ASH_WEB_UI_CONFIG_MANAGER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "url/gurl.h"

class ApplicationLocaleStorage;

namespace content {
class WebUIConfig;
}  // namespace content

namespace ash {

// AshWebUIConfigManager manages the registration and shutdown unregistration
// of Ash-specific trusted (chrome://) and untrusted (chrome-untrusted://)
// WebUIConfigs with content::WebUIConfigMap. On destruction, it unregisters all
// tracked WebUIConfigs in reverse order (LIFO) from `content::WebUIConfigMap`
// to ensure clean shutdown before services/resources are torn down.
class AshWebUIConfigManager {
 public:
  // Returns the singleton instance pointer or nullptr (e.g., in unit tests).
  static AshWebUIConfigManager* GetInstance();

  // `application_locale_storage` must not be null and must outlive `this`.
  explicit AshWebUIConfigManager(
      const ApplicationLocaleStorage* application_locale_storage);
  AshWebUIConfigManager(const AshWebUIConfigManager&) = delete;
  AshWebUIConfigManager& operator=(const AshWebUIConfigManager&) = delete;
  ~AshWebUIConfigManager();

  // Registers all trusted Ash WebUIConfigs with content::WebUIConfigMap.
  void RegisterWebUIConfigs();

  // Registers all untrusted Ash WebUIConfigs with content::WebUIConfigMap.
  void RegisterUntrustedWebUIConfigs();

 private:
  friend class AshWebUIConfigManagerTest;

  // Tracks and registers a trusted WebUIConfig with content::WebUIConfigMap.
  void AddWebUIConfig(std::unique_ptr<content::WebUIConfig> config);

  // Tracks and registers an untrusted WebUIConfig with content::WebUIConfigMap.
  void AddUntrustedWebUIConfig(std::unique_ptr<content::WebUIConfig> config);

  // Unregisters all tracked WebUI configs from content::WebUIConfigMap in
  // reverse order.
  void Unregister();

  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;

  std::vector<GURL> registered_urls_to_unregister_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CONFIG_ASH_WEB_UI_CONFIG_MANAGER_H_
