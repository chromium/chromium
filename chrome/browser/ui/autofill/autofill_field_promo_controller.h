// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_FIELD_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_FIELD_PROMO_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_view.h"
#include "chrome/browser/ui/autofill/autofill_popup_hide_helper.h"
#include "components/user_education/common/feature_promo_result.h"
#include "ui/base/interaction/element_identifier.h"

namespace autofill {

// This class is the controller for the `AutofillFieldPromoView`. Its main
// function is to control the view's lifetime.
//
// This controller is used in order to display IPHs attached directly to the
// DOM. Normally, this is not possible. IPHs can only be attached to views (for
// example, an IPH can easily be attached to the autofill popup).
//
// One controller can display only one IPH. To display a new IPH, a new
// controller instance is needed.
//
// The controller receives (in the constructor) details about which IPH should
// be displayed. The `Show()` method additionally receives the bounds of the DOM
// element. The IPH will be displayed under this DOM element.
//
// On show, the controller creates an `AutofillFieldPromoView`, an invisible
// view placed at the bottom of where the DOM element is displayed.
//
// The role of the invisible view is to serve as an anchor to the IPH (as IPHs
// can only be attached to views).
//
// An IPH is hidden automatically when its anchor view gets hidden/destroyed.
// For example, an IPH attached to the autofill popup is hidden when the
// autofill popup is hidden (on tab change, on scroll, etc.). In this case, the
// AutofillFieldPromoController takes care of hiding the anchor view.
//
// Responsibilities when creating a new `AutofillFieldPromoController` instance:
//
// 1. An `AutofillFieldPromoController` already exists as a member variable of
// `ChromeAutofillClient`. Creating another instance of the controller should
// also happen in `ChromeAutofillClient` and follow the already existing
// implementation.
//
// 2. The IPH displayed by this controller has to be hidden in various
// scenarios, which are explained below. The hide calls to the new controller
// should happen in the same spot as the hide calls of the existing controller.
//
// 3. Hiding becomes trickier when some hiding events cannot be captured by
// the `AutofillPopupHideHelper`. Two such events are interesting for the IPH:
//
// 3.a. Scrolling. The IPH should be hidden in the same place where the autofill
// popup is hidden (currently in `BrowserAutofillManager::OnHidePopupImpl()`).
// This method is called from the renderer not only on scroll, but hiding the
// IPH in extra scenarios is not a disadvantage.
//
// 3.b. Overlapping with password generation prompt. The IPH should be hidden in
// the same place where the autofill popup is hidden (currently in
// `password_manager_util.cc::UserTriggeredManualGenerationFromContextMenu()`).
//
// 4. The IPH has to be hidden right before the autofill popup is shown
// (currently in `ChromeAutofillClient::ShowAutofillSuggestions()`), because the
// autofill popup cannot be shown if it overlaps with another view. Hiding the
// IPH is an asynchronous task, that's why immediately after the IPH is hidden,
// the task of showing the autofill popup is posted to the task thread.
//
// 5. (Optional) For more information, see go/mullet-m3-iph "Closing the bubble
// section".
class AutofillFieldPromoController {
 public:
  virtual ~AutofillFieldPromoController() = default;

  // Displays the `AutofillFieldPromoView` (which is an invisible view), and
  // anchors the IPH onto the view. The IPH may or may not be shown depending on
  // configuration.
  virtual void Show(const gfx::RectF& bounds) = 0;
  // Hides and destroys the view.
  virtual void Hide() = 0;
  // Whether the call to Show was successful. This value is reset when the
  // promo is hidden.
  virtual bool IsMaybeShowing() const = 0;
  // Returns the feature promo associated with a `AutofillFieldPromoController`
  // instance.
  virtual const base::Feature& GetFeaturePromo() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_FIELD_PROMO_CONTROLLER_H_
