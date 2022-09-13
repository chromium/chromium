// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_EMPTY_CONTROLLER_OBSERVER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_EMPTY_CONTROLLER_OBSERVER_H_

#include "components/autofill_assistant/browser/controller_observer.h"

namespace autofill_assistant {

// Convenience implementation of |ControllerObserver| which provides an empty
// implementation to all the methods, used by classes which want to inherit from
// |ControllerObserver| but only need to override a subset of the methods.
class EmptyControllerObserver : public ControllerObserver {
 public:
  void OnStateChanged(AutofillAssistantState new_state) override {}
  void OnKeyboardSuppressionStateChanged(
      bool should_suppress_keyboard) override {}
  void CloseCustomTab() override {}
  void OnError(const std::string& error_message,
               Metrics::DropOutReason reason) override {}
  void OnUserDataChanged(const UserData& user_data,
                         UserDataFieldChange field_change) override {}
  void OnTouchableAreaChanged(
      const RectF& visual_viewport,
      const std::vector<RectF>& touchable_areas,
      const std::vector<RectF>& restricted_areas) override {}
  void OnViewportModeChanged(ViewportMode mode) override {}
  void OnOverlayColorsChanged(
      const ExecutionDelegate::OverlayColors& colors) override {}
  void OnClientSettingsChanged(const ClientSettings& settings) override {}
  void OnShouldShowOverlayChanged(bool should_show) override {}
  void OnExecuteScript(const std::string& start_message) override {}
  void OnStart(const TriggerContext& trigger_context) override {}
  void OnStop() override {}
  void OnResetState() override {}
  void OnUiShownChanged(bool shown) override {}
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_EMPTY_CONTROLLER_OBSERVER_H_
