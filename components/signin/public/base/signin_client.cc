// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_client.h"

void SigninClient::PreSignOut(
    base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
    signin_metrics::ProfileSignout signout_source_metric) {
  // Allow sign out to continue.
  std::move(on_signout_decision_reached).Run(SignoutDecision::ALLOW);
}

bool SigninClient::IsClearPrimaryAccountAllowed() const {
  return true;
}
