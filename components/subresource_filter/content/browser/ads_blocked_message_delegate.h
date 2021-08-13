// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ADS_BLOCKED_MESSAGE_DELEGATE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ADS_BLOCKED_MESSAGE_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "components/infobars/android/infobar_android.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
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
    : public content::WebContentsUserData<AdsBlockedMessageDelegate> {
 public:
  void ShowMessage();

  ~AdsBlockedMessageDelegate() override;

  messages::MessageWrapper* message_for_testing() { return message_.get(); }

 private:
  friend class content::WebContentsUserData<AdsBlockedMessageDelegate>;

  explicit AdsBlockedMessageDelegate(content::WebContents* web_contents);
  void HandleClick();
  void HandleDismissCallback(messages::DismissReason dismiss_reason);

  content::WebContents* web_contents_ = nullptr;
  std::unique_ptr<messages::MessageWrapper> message_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ADS_BLOCKED_INFOBAR_DELEGATE_H_
