// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_infobar_delegate.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace send_tab_to_self {

// static
std::unique_ptr<SendTabToSelfInfoBarDelegate>
SendTabToSelfInfoBarDelegate::Create(content::WebContents* web_contents,
                                     const SendTabToSelfEntry* entry) {
  return base::WrapUnique(
      new SendTabToSelfInfoBarDelegate(web_contents, entry));
}

SendTabToSelfInfoBarDelegate::~SendTabToSelfInfoBarDelegate() = default;

std::u16string SendTabToSelfInfoBarDelegate::GetInfobarMessage() const {
  // TODO(crbug.com/40619532): Define real string.
  NOTIMPLEMENTED();
  return u"Open";
}

void SendTabToSelfInfoBarDelegate::OpenTab() {
  content::OpenURLParams open_url_params(
      entry_->GetURL(), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PageTransition::PAGE_TRANSITION_LINK,
      false /* is_renderer_initiated */);
  web_contents_->OpenURL(open_url_params, /*navigation_handle_callback=*/{});

  // TODO(crbug.com/40619532): Update the model to reflect that an infobar is
  // shown.
}

void SendTabToSelfInfoBarDelegate::InfoBarDismissed() {
  NOTIMPLEMENTED();
}

infobars::InfoBarDelegate::InfoBarIdentifier
SendTabToSelfInfoBarDelegate::GetIdentifier() const {
  return SEND_TAB_TO_SELF_INFOBAR_DELEGATE;
}

SendTabToSelfInfoBarDelegate::SendTabToSelfInfoBarDelegate(
    content::WebContents* web_contents,
    const SendTabToSelfEntry* entry) {
  web_contents_ = web_contents;
  entry_ = entry;
}

}  // namespace send_tab_to_self
