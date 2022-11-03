// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_DELEGATE_H_

#include <stddef.h>

#include "components/autofill/core/browser/ui/popup_types.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
class Rect;
class RectF;
}  // namespace gfx

namespace content {
class WebContents;
}  // namespace content
namespace autofill {

// Base class for Controllers of Autofill-style popups. This interface is
// used by the relevant views to communicate with the controller.
class AutofillPopupViewDelegate {
 public:
  // Called when the popup should be hidden. Controller will be deleted after
  // the view has been hidden and destroyed. The reason can be used to decide
  // whether to defer that.
  virtual void Hide(PopupHidingReason reason) = 0;

  // Called when the popup view was destroyed.
  virtual void ViewDestroyed() = 0;

  // The user cleared the current selection, e.g. by moving the mouse cursor
  // out of the popup bounds.
  virtual void SelectionCleared() = 0;

  // The view that the form field element sits in.
  virtual gfx::NativeView container_view() const = 0;

  // The web contents the form field element sits in.
  virtual content::WebContents* GetWebContents() const = 0;

  // The bounds of the form field element (screen coordinates).
  virtual const gfx::RectF& element_bounds() const = 0;

  // If the current popup should be displayed in RTL mode.
  virtual bool IsRTL() const = 0;

 protected:
  virtual ~AutofillPopupViewDelegate() = default;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_DELEGATE_H_
