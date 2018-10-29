// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_MODULE_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_MODULE_H_

#include <memory>
#include <string>

#include "base/macros.h"

namespace chromeos {

class DiscoverHandler;

class DiscoverModule {
 public:
  DiscoverModule() = default;

  virtual ~DiscoverModule() = default;

  // Returns true if module is completed
  virtual bool IsCompleted() const = 0;

  // Creates and returns WebUI handler for the module.
  virtual std::unique_ptr<DiscoverHandler> CreateWebUIHandler() = 0;

  // Module is also expected to provide static method:
  // static const char* kModuleName;

 private:
  DISALLOW_COPY_AND_ASSIGN(DiscoverModule);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_MODULE_H_
