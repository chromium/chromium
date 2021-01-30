// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_

#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace autofill {

class AutofillBubbleBase;

// Interface that exposes controller functionality to all autofill bubbles.
class AutofillBubbleControllerBase : public content::WebContentsObserver {
 public:
  explicit AutofillBubbleControllerBase(content::WebContents* web_contents);
  ~AutofillBubbleControllerBase() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

 protected:
  // Called in DidFinishNavigation() if the navigation may result in an action
  // in the bubble. Isn't called when the navigation happens too quickly or is a
  // navigation to the same document. Check DidFinishNavigation() for details.
  // Returns true if this navigation is relevant from the point of view of the
  // specific controller (e.g. whether the bubble is available or not).
  // Subclasses usually override this method to do custom work upon
  // navigation (e.g. reporting metrics).
  virtual bool HandleDidFinishRelevantNavigation() = 0;

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

  void SetShownTimestampToNow();

 private:
  // Remove the |bubble_view_| and hide the bubble.
  void HideBubble();

  // Weak reference. Will be nullptr if no bubble is currently shown.
  AutofillBubbleBase* bubble_view_ = nullptr;

  // The time at which the bubble was shown. If it has been visible for less
  // time than some reasonable limit, don't close the bubble upon navigation.
  base::Time bubble_shown_timestamp_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_
