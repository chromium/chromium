// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_FIELD_PROMO_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_FIELD_PROMO_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "ui/views/view.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class RectF;
}  // namespace gfx

namespace ui {
class ElementIdentifier;
}  // namespace ui

namespace autofill {

// This is an invisible view which is placed at the bottom of an DOM element.
// It serves as an anchor for IPH's which need to be attached directly to an
// DOM element.
//
// This class should not be instantiated by itself. Please use an
// `AutofillFieldPromoController`.
class AutofillFieldPromoView {
 public:
  // Creates and displays the view, and also maybe displays the IPH (the IPH is
  // not always displayed, depending on its configuration).
  static base::WeakPtr<AutofillFieldPromoView> CreateAndShow(
      content::WebContents* web_contents,
      const gfx::RectF& element_bounds,
      const ui::ElementIdentifier& promo_element_identifier);

  virtual ~AutofillFieldPromoView() = default;

  virtual bool OverlapsWithPictureInPictureWindow() const = 0;

  // Destroys the view. As a consequence, the IPH is also hidden.
  virtual void Close() = 0;

  virtual base::WeakPtr<AutofillFieldPromoView> GetWeakPtr() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_FIELD_PROMO_VIEW_H_
