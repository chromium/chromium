// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_SMS_ANDROID_SMS_INFOBAR_DELEGATE_H_
#define COMPONENTS_BROWSER_UI_SMS_ANDROID_SMS_INFOBAR_DELEGATE_H_

#include "base/functional/callback.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/origin.h"

namespace sms {

// This class configures an infobar shown when an SMS is received and the user
// is asked for confirmation that it should be shared with the site (WebOTP).
class SmsInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  using OriginList = std::vector<url::Origin>;

  SmsInfoBarDelegate(const OriginList& origin_list,
                     const std::string& one_time_code,
                     base::OnceClosure on_confirm,
                     base::OnceClosure on_cancel);

  SmsInfoBarDelegate(const SmsInfoBarDelegate&) = delete;
  SmsInfoBarDelegate& operator=(const SmsInfoBarDelegate&) = delete;

  ~SmsInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  void InfoBarDismissed() override;

  std::u16string GetTitle() const;

 private:
  const OriginList origin_list_;
  const std::string one_time_code_;
  base::OnceClosure on_confirm_;
  base::OnceClosure on_cancel_;
};

}  // namespace sms

#endif  // COMPONENTS_BROWSER_UI_SMS_ANDROID_SMS_INFOBAR_DELEGATE_H_
