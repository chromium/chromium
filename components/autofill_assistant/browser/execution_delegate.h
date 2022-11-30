// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_EXECUTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_EXECUTION_DELEGATE_H_

#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/event_handler.h"
#include "components/autofill_assistant/browser/public/rectf.h"
#include "components/autofill_assistant/browser/public/runtime_manager_impl.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/viewport_mode.h"

namespace autofill_assistant {
class ControllerObserver;

// Execution delegate is used to access the current session's state and to
// notify the Controller.
class ExecutionDelegate {
 public:
  // Colors of the overlay. Empty string to use the default.
  struct OverlayColors {
    // Overlay background color.
    std::string background;

    // Color of the border around the highlighted portions of the overlay.
    std::string highlight_border;
  };

  virtual ~ExecutionDelegate() = default;

  // Returns the current state of the controller.
  virtual AutofillAssistantState GetState() const = 0;

  // If the controller is waiting for user data, this field contains a non-null
  // object describing the currently selected data.
  virtual UserData* GetUserData() = 0;

  // Reports a fatal error to Autofill Assistant, which should then stop.
  virtual void OnFatalError(const std::string& error_message,
                            Metrics::DropOutReason reason) = 0;

  // Adds the rectangles that correspond to the current touchable area to
  // the given vector.
  //
  // At the end of this call, |rectangles| contains one element per
  // configured rectangles, though these can correspond to empty rectangles.
  // Coordinates absolute CSS coordinates.
  //
  // Note that the vector is not cleared before rectangles are added.
  virtual void GetTouchableArea(std::vector<RectF>* rectangles) const = 0;
  virtual void GetRestrictedArea(std::vector<RectF>* rectangles) const = 0;

  // Returns the current size of the visual viewport. May be empty if
  // unknown.
  //
  // The rectangle is expressed in absolute CSS coordinates.
  virtual void GetVisualViewport(RectF* viewport) const = 0;

  // Returns whether the viewport should be resized.
  virtual ViewportMode GetViewportMode() = 0;

  // Gets whether the tab associated with this controller is currently selected.
  virtual bool IsTabSelected() = 0;

  // Sets whether the tab associated with this controller is currently selected.
  virtual void SetTabSelected(bool selected) = 0;

  // Fills in the overlay colors.
  virtual void GetOverlayColors(OverlayColors* colors) const = 0;

  // Gets the current Client Settings
  virtual const ClientSettings& GetClientSettings() const = 0;

  // Gets the trgger context
  virtual const TriggerContext* GetTriggerContext() = 0;

  // Gets the current URL
  virtual const GURL& GetCurrentURL() = 0;

  // Sets whether a UI is shown.
  virtual void SetUiShown(bool shown) = 0;

  // Returns the user model.
  virtual UserModel* GetUserModel() = 0;

  // Whether the overlay should be determined based on AA state or always
  // hidden.
  virtual bool ShouldShowOverlay() const = 0;

  // Whether the keyboard should currently be suppressed.
  virtual bool ShouldSuppressKeyboard() const = 0;

  // Set the keyboard suppression for all frames for the current WebContent's
  // main page.
  virtual void SuppressKeyboard(bool suppress) = 0;

  // Notifies the execution delegate that it should shut down.
  virtual void ShutdownIfNecessary() = 0;

  // Notifies the execution delegate about a change to the UserData.
  virtual void NotifyUserDataChange(UserDataFieldChange field_change) = 0;

  // Register an observer. Observers get told about changes to the
  // controller.
  virtual void AddObserver(ControllerObserver* observer) = 0;

  // Remove a previously registered observer.
  virtual void RemoveObserver(const ControllerObserver* observer) = 0;

  // Returns true if the controller is in a state where UI is necessary.
  virtual bool NeedsUI() const = 0;

 protected:
  ExecutionDelegate() = default;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_EXECUTION_DELEGATE_H_
