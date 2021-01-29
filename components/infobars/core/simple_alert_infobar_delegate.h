// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_SIMPLE_ALERT_INFOBAR_DELEGATE_H_
#define COMPONENTS_INFOBARS_CORE_SIMPLE_ALERT_INFOBAR_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace gfx {
struct VectorIcon;
}

namespace infobars {
class InfoBarManager;
}

class SimpleAlertInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a simple alert infobar and delegate and adds the infobar to
  // |infobar_manager|. If |vector_icon| is not null, it will be shown.
  // |infobar_identifier| names what class triggered the infobar for metrics.
  static void Create(
      infobars::InfoBarManager* infobar_manager,
      infobars::InfoBarDelegate::InfoBarIdentifier infobar_identifier,
      const gfx::VectorIcon* vector_icon,
      const base::string16& message,
      bool auto_expire = true,
      bool should_animate = true);

 private:
  SimpleAlertInfoBarDelegate(
      infobars::InfoBarDelegate::InfoBarIdentifier infobar_identifier,
      const gfx::VectorIcon* vector_icon,
      const base::string16& message,
      bool auto_expire,
      bool should_animate);
  ~SimpleAlertInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  bool ShouldAnimate() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;

  infobars::InfoBarDelegate::InfoBarIdentifier infobar_identifier_;
  const gfx::VectorIcon* vector_icon_;
  base::string16 message_;
  bool auto_expire_;  // Should it expire automatically on navigation?
  bool should_animate_;

  DISALLOW_COPY_AND_ASSIGN(SimpleAlertInfoBarDelegate);
};

#endif  // COMPONENTS_INFOBARS_CORE_SIMPLE_ALERT_INFOBAR_DELEGATE_H_
