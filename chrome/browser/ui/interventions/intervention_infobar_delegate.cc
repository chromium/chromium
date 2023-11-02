// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/interventions/intervention_infobar_delegate.h"

#include "chrome/browser/ui/interventions/intervention_delegate.h"

InterventionInfoBarDelegate::InterventionInfoBarDelegate(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier,
    InterventionDelegate* intervention_delegate)
    : identifier_(identifier), intervention_delegate_(intervention_delegate) {
  DCHECK(intervention_delegate_);
}

InterventionInfoBarDelegate::~InterventionInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
InterventionInfoBarDelegate::GetIdentifier() const {
  return identifier_;
}

bool InterventionInfoBarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  return delegate->GetIdentifier() == GetIdentifier();
}

void InterventionInfoBarDelegate::InfoBarDismissed() {
  // As the infobar allows the user to undo the intervention, dismissing it
  // implies accepting the intervention.
  intervention_delegate_->AcceptIntervention();
}
