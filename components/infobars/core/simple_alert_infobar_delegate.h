// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_SIMPLE_ALERT_INFOBAR_DELEGATE_H_
#define COMPONENTS_INFOBARS_CORE_SIMPLE_ALERT_INFOBAR_DELEGATE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace gfx {
struct VectorIcon;
}

class SimpleAlertInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  SimpleAlertInfoBarDelegate(
      infobars::InfoBarDelegate::InfoBarIdentifier infobar_identifier,
      const gfx::VectorIcon* vector_icon,
      const std::u16string& message,
      bool auto_expire,
      bool should_animate,
      bool closeable = true);

  SimpleAlertInfoBarDelegate(const SimpleAlertInfoBarDelegate&) = delete;
  SimpleAlertInfoBarDelegate& operator=(const SimpleAlertInfoBarDelegate&) =
      delete;

  ~SimpleAlertInfoBarDelegate() override;

 private:
  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  bool ShouldAnimate() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  bool IsCloseable() const override;

  infobars::InfoBarDelegate::InfoBarIdentifier infobar_identifier_;
  raw_ptr<const gfx::VectorIcon> vector_icon_;
  std::u16string message_;
  bool auto_expire_;  // Should it expire automatically on navigation?
  bool should_animate_;
  bool closeable_;
};

#endif  // COMPONENTS_INFOBARS_CORE_SIMPLE_ALERT_INFOBAR_DELEGATE_H_
