// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_DELEGATE_H_

#include "base/i18n/rtl.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class RectF;
}  // namespace gfx

namespace autofill {

// Base class for Controllers of Autofill-style popups. This interface is
// used by the relevant views to communicate with the controller.
class AutofillPopupViewDelegate {
 public:
  // Called when the popup should be hidden. Controller will be deleted after
  // the view has been hidden and destroyed. The reason can be used to decide
  // whether to defer that.
  virtual void Hide(SuggestionHidingReason reason) = 0;

  // Called when the popup view was destroyed.
  virtual void ViewDestroyed() = 0;

  // The view that the form field element sits in.
  virtual gfx::NativeView container_view() const = 0;

  // The web contents the form field element sits in.
  virtual content::WebContents* GetWebContents() const = 0;

  // The bounds of the form field element or caret (screen coordinates).
  virtual const gfx::RectF& element_bounds() const = 0;

  // Whether the element to anchor the popup on is a field/text area, the
  // caret or the keyboard accessory.
  virtual PopupAnchorType anchor_type() const = 0;

  // Returns the text direction of the focused field at the time of creating
  // this popup. This does not govern whether the popup UI is RTL (that is
  // determined by the browser language), but it may have an impact on how the
  // bubble and its arrow are placed on Desktop.
  virtual base::i18n::TextDirection GetElementTextDirection() const = 0;

 protected:
  virtual ~AutofillPopupViewDelegate() = default;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_DELEGATE_H_
