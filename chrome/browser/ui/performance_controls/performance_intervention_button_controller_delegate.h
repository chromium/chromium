// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_CONTROLLER_DELEGATE_H_

// This delegate class is to control the performance intervention toolbar
// button.
class PerformanceInterventionButtonControllerDelegate {
 public:
  virtual ~PerformanceInterventionButtonControllerDelegate() = default;

  // Show the performance intervention toolbar button.
  virtual void Show() = 0;

  // Hides the performance intervention toolbar button.
  virtual void Hide() = 0;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_CONTROLLER_DELEGATE_H_
