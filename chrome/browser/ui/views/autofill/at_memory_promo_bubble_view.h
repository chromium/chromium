// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AT_MEMORY_PROMO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AT_MEMORY_PROMO_BUBBLE_VIEW_H_

#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/help_bubble/custom_help_bubble.h"
#include "components/user_education/common/user_education_context.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}

namespace autofill {

class AtMemoryPromoBubbleView : public AutofillLocationBarBubble,
                                public user_education::CustomHelpBubbleUi {
  METADATA_HEADER(AtMemoryPromoBubbleView, AutofillLocationBarBubble)

 public:
  static std::unique_ptr<AtMemoryPromoBubbleView> Create(
      const scoped_refptr<user_education::UserEducationContext>& context,
      user_education::FeaturePromoSpecification::BuildHelpBubbleParams params);

  AtMemoryPromoBubbleView(views::BubbleAnchor anchor_view,
                          content::WebContents* web_contents);
  AtMemoryPromoBubbleView(const AtMemoryPromoBubbleView&) = delete;
  AtMemoryPromoBubbleView& operator=(const AtMemoryPromoBubbleView&) = delete;
  ~AtMemoryPromoBubbleView() override;

  // LocationBarBubbleDelegateView:
  std::u16string GetWindowTitle() const override;
  void AddedToWidget() override;

  // AutofillBubbleBase:
  void Hide() override;

 private:
  void OnLearnMoreClicked();
  void OnGotItClicked();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AT_MEMORY_PROMO_BUBBLE_VIEW_H_
