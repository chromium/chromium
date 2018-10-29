// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_MODULES_DISCOVER_MODULE_PIN_SETUP_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_MODULES_DISCOVER_MODULE_PIN_SETUP_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_module.h"

namespace chromeos {

class DiscoverModulePinSetup : public DiscoverModule {
 public:
  // Module name.
  static const char kModuleName[];

  DiscoverModulePinSetup();
  ~DiscoverModulePinSetup() override;

  // Returns primary user password, or empty string if not known.
  // Password is kept for newly created user only, and is returned only once.
  // (Empty string will be returned for subsequent calls.)
  std::string ConsumePrimaryUserPassword();
  void SetPrimaryUserPassword(const std::string& password);

  // DiscoverModule:
  bool IsCompleted() const override;
  std::unique_ptr<DiscoverHandler> CreateWebUIHandler() override;

 private:
  std::string primary_user_password_;

  base::WeakPtrFactory<DiscoverModulePinSetup> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(DiscoverModulePinSetup);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_MODULES_DISCOVER_MODULE_PIN_SETUP_H_
