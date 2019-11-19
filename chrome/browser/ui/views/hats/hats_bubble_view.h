// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HATS_HATS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_HATS_HATS_BUBBLE_VIEW_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/views/close_bubble_on_tab_activation_helper.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class AppMenuButton;
class Browser;

// The type of the callback invoked when a user consents, cancels, or dismisses
// the consent banner bubble.
// |accept| indicates whether the user consents to take the survey.
using HatsConsentCallback = base::OnceCallback<void(bool accept)>;

// This bubble view is displayed when a Happiness tracking survey is triggered.
// It displays a WebUI that hosts the survey.
class HatsBubbleView : public views::BubbleDialogDelegateView {
 public:
  // Histogram enum on how users interact with the bubble.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class BubbleUsageCounts {
    kAccepted = 0,
    kDeclined = 1,
    kUIDismissed = 2,
    kIgnored = 3,
    kMaxValue = kIgnored,
  };

  // Returns a pointer to the Hats Bubble being shown. For testing only.
  static views::BubbleDialogDelegateView* GetHatsBubble();

  // Shows the bubble when the survey content identified by |site_id| is ready.
  static void ShowOnContentReady(Browser* browser, const std::string& site_id);

  // Shows the bubble now with supplied callback |consent_callback|.
  static void Show(Browser* browser, HatsConsentCallback consent_callback);

 protected:
  // views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;

  // views::DialogDelegate:
  bool Cancel() override;
  bool Accept() override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  HatsBubbleView(Browser* browser,
                 AppMenuButton* anchor_button,
                 gfx::NativeView parent_view,
                 HatsConsentCallback consent_callback);
  ~HatsBubbleView() override;

  static HatsBubbleView* instance_;
  CloseBubbleOnTabActivationHelper close_bubble_helper_;
  HatsConsentCallback consent_callback_;

  DISALLOW_COPY_AND_ASSIGN(HatsBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HATS_HATS_BUBBLE_VIEW_H_
