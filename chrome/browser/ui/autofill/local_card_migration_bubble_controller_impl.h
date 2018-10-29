// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_LOCAL_CARD_MIGRATION_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_LOCAL_CARD_MIGRATION_BUBBLE_CONTROLLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/elapsed_timer.h"
#include "components/autofill/core/browser/ui/local_card_migration_bubble_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

// Implementation of per-tab class to control the local card migration bubble
// and Omnibox icon.
class LocalCardMigrationBubbleControllerImpl
    : public LocalCardMigrationBubbleController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<
          LocalCardMigrationBubbleControllerImpl> {
 public:
  ~LocalCardMigrationBubbleControllerImpl() override;

  // Shows the prompt that offers local credit card migration.
  // |local_card_migration_bubble_closure| is run upon acceptance.
  void ShowBubble(base::OnceClosure local_card_migration_bubble_closure);

  // Remove the |local_card_migration_bubble_| and hide the bubble.
  void HideBubble();

  // Invoked when local card migration icon is clicked.
  void ReshowBubble();

  // Returns true if Omnibox save credit card icon should be visible.
  bool IsIconVisible() const;

  // Returns nullptr if no bubble is currently shown.
  LocalCardMigrationBubble* local_card_migration_bubble_view() const;

  // LocalCardBubbleController:
  void OnConfirmButtonClicked() override;
  void OnCancelButtonClicked() override;
  void OnBubbleClosed() override;

 protected:
  explicit LocalCardMigrationBubbleControllerImpl(
      content::WebContents* web_contents);

  // Returns the time elapsed since |timer_| was initialized.
  // Exists for testing.
  virtual base::TimeDelta Elapsed() const;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

 private:
  friend class content::WebContentsUserData<
      LocalCardMigrationBubbleControllerImpl>;

  void ShowBubbleImplementation();

  // Update the visibility and toggled state of the Omnibox save card icon.
  void UpdateIcon();

  // Weak reference. Will be nullptr if no bubble is currently shown.
  LocalCardMigrationBubble* local_card_migration_bubble_ = nullptr;

  // Callback to run if user presses Save button in the offer-to-migrate bubble.
  base::OnceClosure local_card_migration_bubble_closure_;

  // Timer used to track the amount of time on this page.
  std::unique_ptr<base::ElapsedTimer> timer_;

  // Boolean to determine if bubble is called from ReshowBubble().
  bool is_reshow_ = false;

  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationBubbleControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_LOCAL_CARD_MIGRATION_BUBBLE_CONTROLLER_IMPL_H_
