// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_client.h"

void SigninClient::PreSignOut(
    base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
    signin_metrics::ProfileSignout signout_source_metric) {
  // Allow sign out to continue.
  std::move(on_signout_decision_reached).Run(SignoutDecision::ALLOW_SIGNOUT);
}

void SigninClient::PreGaiaLogout(base::OnceClosure callback) {
  std::move(callback).Run();
}

bool SigninClient::IsNonEnterpriseUser(const std::string& username) {
  return false;
}
