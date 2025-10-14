// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROMOS_IOS_PROMO_TRIGGER_SERVICE_H_
#define CHROME_BROWSER_UI_PROMOS_IOS_PROMO_TRIGGER_SERVICE_H_

#include "base/callback_list.h"
#include "chrome/browser/promos/promos_types.h"
#include "components/keyed_service/core/keyed_service.h"

// Service that acts as a communication bridge between different UI components
// to trigger iOS promos.
// TODO(crbug.com/446944658): This service is a temporary solution for
// triggering promos. The long-term plan is to migrate the presentation logic
// to the Browser User Education system. Once that is complete, this class can
// be removed.
class IOSPromoTriggerService : public KeyedService {
 public:
  using PromoCallback = base::RepeatingCallback<void(IOSPromoType)>;

  IOSPromoTriggerService();
  ~IOSPromoTriggerService() override;

  IOSPromoTriggerService(const IOSPromoTriggerService&) = delete;
  IOSPromoTriggerService& operator=(const IOSPromoTriggerService&) = delete;

  // Notifies observers that a promo should be shown.
  virtual void NotifyPromoShouldBeShown(IOSPromoType promo_type);

  // Registers a callback to be notified when a promo should be shown.
  [[nodiscard]] base::CallbackListSubscription RegisterPromoCallback(
      PromoCallback callback);

 private:
  base::RepeatingCallbackList<void(IOSPromoType)> callback_list_;
};

#endif  // CHROME_BROWSER_UI_PROMOS_IOS_PROMO_TRIGGER_SERVICE_H_
