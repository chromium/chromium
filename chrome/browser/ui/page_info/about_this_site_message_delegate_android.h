// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_ABOUT_THIS_SITE_MESSAGE_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_UI_PAGE_INFO_ABOUT_THIS_SITE_MESSAGE_DELEGATE_ANDROID_H_

#include <memory>
#include "base/callback.h"
#include "components/messages/android/message_enums.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"

namespace content {
class WebContents;
}  // namespace content

namespace messages {
class MessageWrapper;
}  // namespace messages

// This class creates a message for AboutThisSiteController
class AboutThisSiteMessageDelegateAndroid {
 public:
  AboutThisSiteMessageDelegateAndroid(
      const AboutThisSiteMessageDelegateAndroid&) = delete;
  AboutThisSiteMessageDelegateAndroid& operator=(
      const AboutThisSiteMessageDelegateAndroid&) = delete;

  // Creates an AboutThisSite message.
  static void Create(content::WebContents* web_contents,
                     page_info::proto::BannerInfo banner_info,
                     base::OnceClosure on_dismissed,
                     base::OnceClosure on_url_opened);

 private:
  explicit AboutThisSiteMessageDelegateAndroid(
      content::WebContents* web_contents,
      page_info::proto::BannerInfo banner_info,
      base::OnceClosure on_dismissed,
      base::OnceClosure on_url_opened);
  ~AboutThisSiteMessageDelegateAndroid();

  void OnDismiss(messages::DismissReason reason);
  void OpenUrl();

  std::unique_ptr<messages::MessageWrapper> message_;
  page_info::proto::BannerInfo banner_info_;
  base::OnceClosure on_dismissed_;
  base::OnceClosure on_url_opened_;
  content::WebContents* web_contents_;
};

#endif  // CHROME_BROWSER_UI_PAGE_INFO_ABOUT_THIS_SITE_MESSAGE_DELEGATE_ANDROID_H_
