// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LACROS_FLOAT_CONTROLLER_LACROS_H_
#define CHROME_BROWSER_UI_LACROS_FLOAT_CONTROLLER_LACROS_H_

#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"

// Lacros implementation of `chromeos::FloatControllerBase`.
class FloatControllerLacros : public chromeos::FloatControllerBase {
 public:
  FloatControllerLacros();
  FloatControllerLacros(const FloatControllerLacros&) = delete;
  FloatControllerLacros& operator=(const FloatControllerLacros&) = delete;
  ~FloatControllerLacros() override;

  // chromeos::FloatControllerBase:
  void SetFloat(aura::Window* window,
                chromeos::FloatStartLocation float_start_location) override;
  void UnsetFloat(aura::Window* window) override;
};

#endif  // CHROME_BROWSER_UI_LACROS_FLOAT_CONTROLLER_LACROS_H_
