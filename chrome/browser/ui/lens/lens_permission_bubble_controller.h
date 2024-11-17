// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_PERMISSION_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_PERMISSION_BUBBLE_CONTROLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/prefs/pref_change_registrar.h"

namespace content {
class WebContents;
}

namespace ui {
class DialogModel;
class Event;
}  // namespace ui

namespace views {
class Widget;
}

class BrowserWindowInterface;
class PrefService;

namespace lens {

static constexpr char kLensPermissionDialogName[] = "LensPermissionDialog";

// Manages the Lens Permission Bubble instance for the associated browser.
class LensPermissionBubbleController {
 public:
  LensPermissionBubbleController(
      BrowserWindowInterface* browser_window_interface,
      PrefService* pref_service,
      LensOverlayInvocationSource invocation_source);
  LensPermissionBubbleController(const LensPermissionBubbleController&) =
      delete;
  LensPermissionBubbleController& operator=(
      const LensPermissionBubbleController&) = delete;
  ~LensPermissionBubbleController();

  const views::Widget* dialog_widget_for_testing() {
    return dialog_widget_.get();
  }

  // Shows a tab-modal dialog. `callback` is called when the permission is
  // granted, whether by user directly accepting this dialog or indirectly via
  // pref change.
  using RequestPermissionCallback = base::RepeatingClosure;
  void RequestPermission(content::WebContents* web_contents,
                         RequestPermissionCallback callback);

  // Returns whether there is an associated open dialog widget.
  bool HasOpenDialogWidget();

 private:
  std::unique_ptr<ui::DialogModel> CreateLensPermissionDialogModel();
  void OnHelpCenterLinkClicked(const ui::Event& event);
  void OnPermissionDialogAccept();
  void OnPermissionDialogCancel();
  void OnPermissionDialogClose();
  void OnPermissionPreferenceUpdated(RequestPermissionCallback callback);

  // Invocation source for the lens overlay.
  LensOverlayInvocationSource invocation_source_;
  // The associated browser.
  raw_ptr<BrowserWindowInterface> browser_window_interface_ = nullptr;
  // The pref service associated with the current profile.
  raw_ptr<PrefService> pref_service_ = nullptr;
  // Registrar for pref change notifications.
  PrefChangeRegistrar pref_observer_;
  // Pointer to the widget that contains the current open dialog, if any.
  raw_ptr<views::Widget> dialog_widget_ = nullptr;

  base::WeakPtrFactory<LensPermissionBubbleController> weak_ptr_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_PERMISSION_BUBBLE_CONTROLLER_H_
