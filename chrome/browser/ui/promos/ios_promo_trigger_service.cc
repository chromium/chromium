// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/promos/ios_promo_trigger_service.h"

IOSPromoTriggerService::IOSPromoTriggerService() = default;
IOSPromoTriggerService::~IOSPromoTriggerService() = default;

void IOSPromoTriggerService::NotifyPromoShouldBeShown(IOSPromoType promo_type) {
  callback_list_.Notify(promo_type);
}

base::CallbackListSubscription IOSPromoTriggerService::RegisterPromoCallback(
    PromoCallback callback) {
  return callback_list_.Add(std::move(callback));
}
