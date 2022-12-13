// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PUBLISHER_BASE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PUBLISHER_BASE_H_

#include <string>

#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace apps {

// A publisher parent class (in the App Service sense) for all app publishers.
// This class has NOTIMPLEMENTED() implementations of mandatory methods from the
// apps::mojom::Publisher class to simplify the process of adding a new
// publisher.
//
// See components/services/app_service/README.md.
class PublisherBase : public apps::mojom::Publisher {
 public:
  PublisherBase();
  ~PublisherBase() override;

  PublisherBase(const PublisherBase&) = delete;
  PublisherBase& operator=(const PublisherBase&) = delete;

  static apps::mojom::AppPtr MakeApp(apps::mojom::AppType app_type,
                                     std::string app_id,
                                     apps::mojom::Readiness readiness,
                                     const std::string& name,
                                     apps::mojom::InstallReason install_reason);

  void FlushMojoCallsForTesting();

 protected:
  void Initialize(const mojo::Remote<apps::mojom::AppService>& app_service,
                  apps::mojom::AppType app_type);

  // Publish |app| to all subscribers in |subscribers|. Should be called
  // whenever the app represented by |app| undergoes some state change to inform
  // subscribers of the change.
  void Publish(apps::mojom::AppPtr app,
               const mojo::RemoteSet<apps::mojom::Subscriber>& subscribers);

  mojo::Receiver<apps::mojom::Publisher>& receiver() { return receiver_; }

 private:
  mojo::Receiver<apps::mojom::Publisher> receiver_{this};
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PUBLISHER_BASE_H_
