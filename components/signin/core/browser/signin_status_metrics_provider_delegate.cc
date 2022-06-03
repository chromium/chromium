// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_status_metrics_provider_delegate.h"

#include "base/check.h"
#include "components/signin/core/browser/signin_status_metrics_provider.h"

AccountsStatus::AccountsStatus()
    : num_accounts(0), num_opened_accounts(0), num_signed_in_accounts(0) {}

SigninStatusMetricsProviderDelegate::SigninStatusMetricsProviderDelegate()
    : owner_(nullptr) {}

SigninStatusMetricsProviderDelegate::~SigninStatusMetricsProviderDelegate() {}

void SigninStatusMetricsProviderDelegate::SetOwner(
    SigninStatusMetricsProvider* owner) {
  DCHECK(owner);
  DCHECK(!owner_);
  owner_ = owner;
}
