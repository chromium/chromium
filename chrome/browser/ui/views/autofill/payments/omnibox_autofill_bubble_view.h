// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_BUBBLE_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class ScrollView;
class View;
}  // namespace views

namespace autofill {

class OmniboxAutofillBubbleController;
struct Suggestion;

class OmniboxAutofillBubbleView : public AutofillLocationBarBubble {
  METADATA_HEADER(OmniboxAutofillBubbleView, AutofillLocationBarBubble)

 public:
  OmniboxAutofillBubbleView(views::BubbleAnchor anchor_view,
                            content::WebContents* web_contents,
                            OmniboxAutofillBubbleController* controller);
  OmniboxAutofillBubbleView(const OmniboxAutofillBubbleView&) = delete;
  OmniboxAutofillBubbleView& operator=(const OmniboxAutofillBubbleView&) =
      delete;
  ~OmniboxAutofillBubbleView() override;

  void Show(DisplayReason reason);

  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;
  void AddedToWidget() override;

 protected:
  // LocationBarBubbleDelegateView:
  void Init() override;

 private:
  // Calculates the maximum available vertical space for the bubble (shown below
  // the omnibox chip), bounded by both the screen display work area and the
  // browser window.
  int GetMaxBubbleHeight() const;

  // Calculates the maximum available vertical space for the suggestion list
  // scroll view.
  int GetMaxScrollViewHeight() const;

  void OnSuggestionAccepted(const Suggestion& suggestion, size_t row_index);
  void OnSuggestionSelected(const Suggestion& suggestion);
  void OnSuggestionDeselected();

  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
  base::WeakPtr<OmniboxAutofillBubbleController> controller_;
  base::WeakPtrFactory<OmniboxAutofillBubbleView> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_BUBBLE_VIEW_H_
