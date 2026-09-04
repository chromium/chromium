// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_CONTROLLER_H_

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_view.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;

namespace send_tab_to_self {

class SendTabToSelfEntry;
class SendTabToSelfBubbleView;

class SendTabToSelfToolbarBubbleController : public views::WidgetObserver {
 public:
  explicit SendTabToSelfToolbarBubbleController(BrowserWindowInterface* bwi);
  ~SendTabToSelfToolbarBubbleController() override;

  DECLARE_USER_DATA(SendTabToSelfToolbarBubbleController);

  static SendTabToSelfToolbarBubbleController* From(
      BrowserWindowInterface* bwi);

  void ShowBubble(const SendTabToSelfEntry& entry, views::BubbleAnchor anchor);
  void HideBubble();

  void OnWidgetDestroyed(views::Widget* widget) override;

  bool IsBubbleShowing() const;

  SendTabToSelfToolbarBubbleView* bubble() {
    return static_cast<SendTabToSelfToolbarBubbleView*>(bubble_tracker_.view());
  }

 private:
  friend class ui::ScopedUnownedUserData<SendTabToSelfToolbarBubbleController>;

  void ShowPendingBubble(const SendTabToSelfEntry& entry);
  void OnBrowserDidClose(BrowserWindowInterface* browser);

  views::ViewTracker bubble_tracker_;
  base::circular_deque<SendTabToSelfEntry> pending_entries_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::CallbackListSubscription browser_did_close_subscription_;
  const raw_ref<BrowserWindowInterface> bwi_;
  ::ui::ScopedUnownedUserData<SendTabToSelfToolbarBubbleController>
      scoped_unowned_user_data_;
  base::WeakPtrFactory<SendTabToSelfToolbarBubbleController> weak_ptr_factory_{
      this};
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_CONTROLLER_H_
