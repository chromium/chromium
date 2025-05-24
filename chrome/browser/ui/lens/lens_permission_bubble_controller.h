// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_PERMISSION_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_PERMISSION_BUBBLE_CONTROLLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/views/widget/widget.h"

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

class PrefService;

namespace lens {

inline constexpr char kLensPermissionDialogName[] = "LensPermissionDialog";

// Manages the Lens Permission Bubble instance for the associated browser.
class LensPermissionBubbleController {
 public:
  LensPermissionBubbleController(tabs::TabInterface& tab_interface,
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
  std::unique_ptr<ui::DialogModel> CreateLensPermissionDialogModel(
      RequestPermissionCallback callback);
  void OnHelpCenterLinkClicked(const ui::Event& event);
  void OnPermissionDialogAccept(RequestPermissionCallback callback);
  // Callback that closes permission dialogs open on non-active tabs if the
  // active tab accepts the permission.
  void OnPermissionPreferenceUpdated();
  void TabWillDetach(tabs::TabInterface* tab,
                     tabs::TabInterface::DetachReason reason);

  // Creates and shows the dialog widget.
  std::unique_ptr<views::Widget> ShowDialogWidget(
      RequestPermissionCallback callback,
      content::WebContents* web_contents);
  void CloseDialogWidget(views::Widget::ClosedReason reason);

  // Invocation source for the lens overlay.
  LensOverlayInvocationSource invocation_source_;
  // The associated tab.
  const raw_ref<tabs::TabInterface> tab_interface_;
  // The pref service associated with the current profile.
  raw_ptr<PrefService> pref_service_ = nullptr;
  // Registrar for pref change notifications.
  PrefChangeRegistrar pref_observer_;
  // Pointer to the widget that contains the current open dialog, if any.
  std::unique_ptr<views::Widget> dialog_widget_;

  base::CallbackListSubscription tab_will_detach_subscription_;

  base::WeakPtrFactory<LensPermissionBubbleController> weak_ptr_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_PERMISSION_BUBBLE_CONTROLLER_H_
