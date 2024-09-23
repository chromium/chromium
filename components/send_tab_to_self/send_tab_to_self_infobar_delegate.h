// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_INFOBAR_DELEGATE_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_INFOBAR_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/infobars/core/infobar_delegate.h"

namespace content {
class WebContents;
}

namespace send_tab_to_self {

class SendTabToSelfEntry;

// Delegate containing logic about what to display and how to behave
// in the SendTabToSelf infobar. Used on Android.
// TODO(crbug.com/40627861): Rename this class to be Android specific.
class SendTabToSelfInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  static std::unique_ptr<SendTabToSelfInfoBarDelegate> Create(
      content::WebContents* web_contents,
      const SendTabToSelfEntry* entry);

  SendTabToSelfInfoBarDelegate(const SendTabToSelfInfoBarDelegate&) = delete;
  SendTabToSelfInfoBarDelegate& operator=(const SendTabToSelfInfoBarDelegate&) =
      delete;

  ~SendTabToSelfInfoBarDelegate() override;

  // Returns the message to be shown in the infobar.
  std::u16string GetInfobarMessage() const;

  // Opens a tab to the url of the shared |entry_|.
  void OpenTab();

  // InfoBarDelegate:
  void InfoBarDismissed() override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;

 private:
  explicit SendTabToSelfInfoBarDelegate(content::WebContents* web_contents,
                                        const SendTabToSelfEntry* entry);

  // The web_content the infobar is attached to. Must outlive this class.
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  // The entry that was share to this device. Must outlive this instance.
  raw_ptr<const SendTabToSelfEntry> entry_ = nullptr;
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_INFOBAR_DELEGATE_H_
