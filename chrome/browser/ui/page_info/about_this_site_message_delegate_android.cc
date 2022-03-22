// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/about_this_site_message_delegate_android.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "components/page_info/core/about_this_site_service.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "content/public/browser/web_contents.h"

// static
void AboutThisSiteMessageDelegateAndroid::Create(
    content::WebContents* web_contents,
    page_info::proto::BannerInfo banner_info,
    base::OnceClosure on_dismissed,
    base::OnceClosure on_url_opened) {
  // This object lives until the Message is dismissed.
  new AboutThisSiteMessageDelegateAndroid(web_contents, std::move(banner_info),
                                          std::move(on_dismissed),
                                          std::move(on_url_opened));
}

AboutThisSiteMessageDelegateAndroid::AboutThisSiteMessageDelegateAndroid(
    content::WebContents* web_contents,
    page_info::proto::BannerInfo banner_info,
    base::OnceClosure on_dismissed,
    base::OnceClosure on_url_opened)
    : banner_info_(std::move(banner_info)),
      on_dismissed_(std::move(on_dismissed)),
      on_url_opened_(std::move(on_url_opened)),
      web_contents_(web_contents) {
  DCHECK(on_dismissed_);
  DCHECK(on_url_opened_);

  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::ABOUT_THIS_SITE,
      base::BindOnce(&AboutThisSiteMessageDelegateAndroid::OpenUrl,
                     base::Unretained(this)),
      base::BindOnce(&AboutThisSiteMessageDelegateAndroid::OnDismiss,
                     base::Unretained(this)));
  if (banner_info_.has_title()) {
    message_->SetTitle(base::UTF8ToUTF16(banner_info_.title()));
    message_->SetDescription(base::UTF8ToUTF16(banner_info_.label()));
  } else {
    message_->SetTitle(base::UTF8ToUTF16(banner_info_.label()));
  }

  message_->SetPrimaryButtonText(base::UTF8ToUTF16(banner_info_.url().label()));
  message_->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_PAGEINFO_BAD));

  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents, messages::MessageScopeType::NAVIGATION,
      messages::MessagePriority::kNormal);
}

AboutThisSiteMessageDelegateAndroid::~AboutThisSiteMessageDelegateAndroid() =
    default;

void AboutThisSiteMessageDelegateAndroid::OnDismiss(
    messages::DismissReason reason) {
  if (reason == messages::DismissReason::GESTURE)
    std::move(on_dismissed_).Run();
  delete this;
}

void AboutThisSiteMessageDelegateAndroid::OpenUrl() {
  std::move(on_url_opened_).Run();
  web_contents_->OpenURL({GURL(banner_info_.url().url()), content::Referrer(),
                          WindowOpenDisposition::NEW_FOREGROUND_TAB,
                          ui::PAGE_TRANSITION_LINK, false});
}
