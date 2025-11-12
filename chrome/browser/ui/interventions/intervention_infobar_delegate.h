// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTERVENTIONS_INTERVENTION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_INTERVENTIONS_INTERVENTION_INFOBAR_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/infobars/core/infobar_delegate.h"

class InterventionDelegate;

// An specialized implementation of InfoBarDelegate used by intervention
// infobars.
class InterventionInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  // |identifier| will be used to uniquely identify the infobar.
  //
  // |intervention_delegate| should outlive this object. It will notified that
  // the intervention is accepted when the user dismisses the infobar.
  InterventionInfoBarDelegate(
      infobars::InfoBarDelegate::InfoBarIdentifier identifier,
      InterventionDelegate* intervention_delegate);

  InterventionInfoBarDelegate(const InterventionInfoBarDelegate&) = delete;
  InterventionInfoBarDelegate& operator=(const InterventionInfoBarDelegate&) =
      delete;

  ~InterventionInfoBarDelegate() override;

  // infobars::InfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;
  void InfoBarDismissed() override;

 private:
  const infobars::InfoBarDelegate::InfoBarIdentifier identifier_;

  // Weak pointer, the delegate is guaranteed to outlive this object.
  raw_ptr<InterventionDelegate> intervention_delegate_;
};

#endif  // CHROME_BROWSER_UI_INTERVENTIONS_INTERVENTION_INFOBAR_DELEGATE_H_
