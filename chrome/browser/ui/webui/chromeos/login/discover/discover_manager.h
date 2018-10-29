// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_MANAGER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/macros.h"

namespace chromeos {

class DiscoverHandler;
class DiscoverModule;

class DiscoverManager {
 public:
  using ModulesMap =
      std::unordered_map<std::string, std::unique_ptr<DiscoverModule>>;

  DiscoverManager();
  ~DiscoverManager();

  // Returns object instance from platform_parts.
  static DiscoverManager* Get();

  // Returns true if there are no modules to be displayed.
  bool IsCompleted() const;

  // Returns vector of WebUI message handlers for visible modules.
  std::vector<std::unique_ptr<DiscoverHandler>> CreateWebUIHandlers() const;

  template <typename T>
  T* GetModule() {
    return static_cast<T*>(GetModuleByName(T::kModuleName));
  }

  const ModulesMap& get_modules() const { return modules_; }

 private:
  // Creates all needed modules.
  void CreateModules();

  // Returns module by name.
  DiscoverModule* GetModuleByName(const std::string& module_name) const;

  ModulesMap modules_;

  DISALLOW_COPY_AND_ASSIGN(DiscoverManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_MANAGER_H_
