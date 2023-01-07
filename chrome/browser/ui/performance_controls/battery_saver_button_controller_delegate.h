// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUTTON_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUTTON_CONTROLLER_DELEGATE_H_

// This delegate class is to control the battery saver button.
class BatterySaverButtonControllerDelegate {
 public:
  virtual void Show() = 0;
  virtual void Hide() = 0;

 protected:
  virtual ~BatterySaverButtonControllerDelegate() = default;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUTTON_CONTROLLER_DELEGATE_H_
