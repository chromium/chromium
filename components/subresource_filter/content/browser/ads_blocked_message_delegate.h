// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ADS_BLOCKED_MESSAGE_DELEGATE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ADS_BLOCKED_MESSAGE_DELEGATE_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "components/subresource_filter/android/ads_blocked_dialog.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace subresource_filter {

// An ads blocked delegate responsible for showing message bubbles.
// Created when the user proceeds through Safe Browsing warning
// interstitials to a site with deceptive embedded content, and matches the
// lifetime of WebContents afterwards.
//
// The message also appears when the site is known to show intrusive ads.
class AdsBlockedMessageDelegate
    : public content::WebContentsUserData<AdsBlockedMessageDelegate>,
      public content::WebContentsObserver {
 public:
  using AdsBlockedDialogFactory =
      base::RepeatingCallback<std::unique_ptr<AdsBlockedDialogBase>(
          content::WebContents*,
          base::OnceClosure,
          base::OnceClosure,
          base::OnceClosure)>;

  ~AdsBlockedMessageDelegate() override;

  // content::WebContentsObserver implementation.
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;

  void ShowMessage();
  void DismissMessage(messages::DismissReason dismiss_reason);

  void DismissMessageForTesting(messages::DismissReason dismiss_reason);

  messages::MessageWrapper* message_for_testing() { return message_.get(); }
  bool reprompt_required_flag_for_testing() { return reprompt_required_; }

 private:
  friend class content::WebContentsUserData<AdsBlockedMessageDelegate>;

  AdsBlockedMessageDelegate(content::WebContents* web_contents);
  AdsBlockedMessageDelegate(content::WebContents* web_contents,
                            AdsBlockedDialogFactory ads_blocked_dialog_factory);

  // Invoked when the user clicks on "OK" as an acknowledgement of the
  // ads blocked message. This action simply dismisses the message.
  void HandleMessageOkClicked();

  // Invoked when the user clicks on the secondary button (settings icon)
  // to get more details and manage site ads settings. This action
  // dismisses the message and opens the ads blocked dialog.
  void HandleMessageManageClicked();

  // Invoked when the message is dismissed, whether by the user, explicitly
  // in the code or automatically.
  void HandleMessageDismissed(messages::DismissReason dismiss_reason);

  // Invoked when the user clicks on the ads blocked dialog button to
  // update site ads settings.
  void HandleDialogAllowAdsClicked();

  // Invoked when the user clicks on the ads blocked dialog button to
  // learn more about blocked ads on sites.
  void HandleDialogLearnMoreClicked();

  // Invoked when the dialog is dismissed, whether by user action,
  // explicitly in the code or automatically.
  void HandleDialogDismissed();

  void ShowDialog(bool should_post_dialog);

  std::unique_ptr<messages::MessageWrapper> message_;

  AdsBlockedDialogFactory ads_blocked_dialog_factory_;
  std::unique_ptr<AdsBlockedDialogBase> ads_blocked_dialog_;

  // Whether we should re-show the dialog to users when users return to the tab.
  bool reprompt_required_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ADS_BLOCKED_MESSAGE_DELEGATE_H_
