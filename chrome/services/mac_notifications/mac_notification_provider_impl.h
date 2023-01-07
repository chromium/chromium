// Copyright 2021 The Chromium Authors
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

namespace mac_notifications {

class MacNotificationProviderImpl : public mojom::MacNotificationProvider {
 public:
  // Constructs a MacNotificationProviderImpl which will be bound to some
  // externally owned Receiver like |mojo::MakeSelfOwnedReceiver()|.
  MacNotificationProviderImpl();

  // Constructs a MacNotificationProviderImpl bound to |receiver|.
  explicit MacNotificationProviderImpl(
      mojo::PendingReceiver<mojom::MacNotificationProvider> binding);

  MacNotificationProviderImpl(const MacNotificationProviderImpl&) = delete;
  MacNotificationProviderImpl& operator=(const MacNotificationProviderImpl&) =
      delete;
  ~MacNotificationProviderImpl() override;

  // mojom::MacNotificationProvider:
  void BindNotificationService(
      mojo::PendingReceiver<mojom::MacNotificationService> service,
      mojo::PendingRemote<mojom::MacNotificationActionHandler> handler)
      override;

 private:
  mojo::Receiver<mojom::MacNotificationProvider> binding_{this};
  std::unique_ptr<mojom::MacNotificationService> service_;
};

}  // namespace mac_notifications

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_PROVIDER_IMPL_H_
