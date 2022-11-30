// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace autofill {

class AutofillBubbleBase;

// Enum for the current showing state of the bubble.
enum class BubbleState {
  // The bubble and the omnibox icon should be hidden.
  kHidden = 0,
  // Only the omnibox icon should be visible.
  kShowingIcon = 1,
  // The bubble and the omnibox icon should be both visible.
  kShowingIconAndBubble = 2,
};

// Interface that exposes controller functionality to all autofill bubbles.
class AutofillBubbleControllerBase : public content::WebContentsObserver {
 public:
  explicit AutofillBubbleControllerBase(content::WebContents* web_contents);
  ~AutofillBubbleControllerBase() override;

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

 protected:
  virtual PageActionIconType GetPageActionIconType() = 0;

  // Subclasses should implement this method to actually show the bubble and
  // potentially log metrics.
  virtual void DoShowBubble() = 0;

  void Show();

  void UpdatePageActionIcon();

  AutofillBubbleBase* bubble_view() const { return bubble_view_; }
  void set_bubble_view(AutofillBubbleBase* bubble_view) {
    bubble_view_ = bubble_view;
  }

  // Remove the |bubble_view_| and hide the bubble.
  void HideBubble();

 private:
  // Weak reference. Will be nullptr if no bubble is currently shown.
  raw_ptr<AutofillBubbleBase> bubble_view_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_
