// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_PWA_CONFIRMATION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_PWA_CONFIRMATION_BUBBLE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/prefs/pref_service.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/widget/widget.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Checkbox;
}

namespace feature_engagement {
class Tracker;
}

class PageActionIconView;

// PWAConfirmationBubbleView provides a bubble dialog for accepting or rejecting
// the installation of a PWA (Progressive Web App) anchored off the PWA install
// icon in the omnibox.
class PWAConfirmationBubbleView : public LocationBarBubbleDelegateView {
 public:
  static bool IsShowing();
  static PWAConfirmationBubbleView* GetBubble();

  PWAConfirmationBubbleView(views::View* anchor_view,
                            content::WebContents* web_contents,
                            PageActionIconView* highlight_icon_button,
                            std::unique_ptr<WebAppInstallInfo> web_app_info,
                            chrome::AppInstallationAcceptanceCallback callback,
                            chrome::PwaInProductHelpState iph_state,
                            PrefService* prefs,
                            feature_engagement::Tracker* tracker);

  PWAConfirmationBubbleView(const PWAConfirmationBubbleView&) = delete;
  PWAConfirmationBubbleView& operator=(const PWAConfirmationBubbleView&) =
      delete;

  ~PWAConfirmationBubbleView() override;

  // LocationBarBubbleDelegateView:
  bool OnCloseRequested(views::Widget::ClosedReason close_reason) override;
  views::View* GetInitiallyFocusedView() override;
  void WindowClosing() override;
  bool Accept() override;

  static base::AutoReset<bool> SetDontCloseOnDeactivateForTesting();

 protected:
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;
 private:
  raw_ptr<PageActionIconView> highlight_icon_button_ = nullptr;
  std::unique_ptr<WebAppInstallInfo> web_app_info_;
  chrome::AppInstallationAcceptanceCallback callback_;

  // Checkbox to launch window with tab strip.
  raw_ptr<views::Checkbox> tabbed_window_checkbox_ = nullptr;

  chrome::PwaInProductHelpState iph_state_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<feature_engagement::Tracker> tracker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_PWA_CONFIRMATION_BUBBLE_VIEW_H_
