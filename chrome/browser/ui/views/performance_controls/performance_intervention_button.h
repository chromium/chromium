// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller_delegate.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class PerformanceInterventionButtonController;

class PerformanceInterventionButton
    : public ToolbarButton,
      public PerformanceInterventionButtonControllerDelegate {
  METADATA_HEADER(PerformanceInterventionButton, ToolbarButton)

 public:
  PerformanceInterventionButton();
  ~PerformanceInterventionButton() override;

  PerformanceInterventionButton(const PerformanceInterventionButton&) = delete;
  PerformanceInterventionButton& operator=(
      const PerformanceInterventionButton&) = delete;

  // PerformanceInterventionButtonControllerDelegate:
  void Show() override;
  void Hide() override;

  // views::View:
  void OnThemeChanged() override;

 private:
  std::unique_ptr<PerformanceInterventionButtonController> controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_H_
