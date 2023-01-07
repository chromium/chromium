// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_SPLASH_SCREEN_H_
#define CHROME_UPDATER_SPLASH_SCREEN_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"

namespace updater {

// Displays a splash screen during install.
class SplashScreen {
 public:
  using Maker = base::RepeatingCallback<std::unique_ptr<SplashScreen>(
      const std::string& app_name)>;
  virtual ~SplashScreen() = default;

  virtual void Show() = 0;
  virtual void Dismiss(base::OnceClosure callback) = 0;
};

}  // namespace updater

#endif  // CHROME_UPDATER_SPLASH_SCREEN_H_
