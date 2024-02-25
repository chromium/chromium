// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_FIELD_PROMO_VIEW_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_FIELD_PROMO_VIEW_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
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

class AutofillFieldPromoViewImpl : public AutofillFieldPromoView,
                                   public views::View {
  METADATA_HEADER(AutofillFieldPromoViewImpl, views::View)

 public:
  AutofillFieldPromoViewImpl(const AutofillFieldPromoViewImpl&) = delete;
  AutofillFieldPromoViewImpl& operator=(const AutofillFieldPromoViewImpl&) =
      delete;
  ~AutofillFieldPromoViewImpl() override;

  bool OverlapsWithPictureInPictureWindow() const override;

  void Close() override;

  base::WeakPtr<AutofillFieldPromoView> GetWeakPtr() override;

 private:
  friend class AutofillFieldPromoView;

  AutofillFieldPromoViewImpl(
      content::WebContents* web_contents,
      const gfx::RectF& element_bounds,
      const ui::ElementIdentifier& promo_element_identifier);

  // Places the view at the bottom of the DOM element.
  void SetViewBounds(const gfx::RectF& element_bounds);

  raw_ptr<content::WebContents> web_contents_;
  base::WeakPtrFactory<AutofillFieldPromoView> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_FIELD_PROMO_VIEW_IMPL_H_
