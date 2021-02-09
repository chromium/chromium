// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_PROVIDER_IMPL_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_PROVIDER_IMPL_H_

#include <memory>

#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class MacNotificationProviderImpl
    : public notifications::mojom::MacNotificationProvider {
 public:
  explicit MacNotificationProviderImpl(
      mojo::PendingReceiver<notifications::mojom::MacNotificationProvider>
          binding);
  MacNotificationProviderImpl(const MacNotificationProviderImpl&) = delete;
  MacNotificationProviderImpl& operator=(const MacNotificationProviderImpl&) =
      delete;
  ~MacNotificationProviderImpl() override;

  // notifications::mojom::MacNotificationProvider:
  void BindNotificationService(
      mojo::PendingReceiver<notifications::mojom::MacNotificationService>
          service,
      mojo::PendingRemote<notifications::mojom::MacNotificationActionHandler>
          handler) override;

 private:
  mojo::Receiver<notifications::mojom::MacNotificationProvider> binding_;
  std::unique_ptr<notifications::mojom::MacNotificationService> service_;
};

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_PROVIDER_IMPL_H_
