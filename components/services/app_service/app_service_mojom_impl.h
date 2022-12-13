// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_APP_SERVICE_MOJOM_IMPL_H_
#define COMPONENTS_SERVICES_APP_SERVICE_APP_SERVICE_MOJOM_IMPL_H_

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "components/services/app_service/public/cpp/preferred_apps_impl.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace apps {

// The implementation of the apps::mojom::AppService Mojo interface.
//
// See components/services/app_service/README.md.
class AppServiceMojomImpl : public apps::mojom::AppService {
 public:
  AppServiceMojomImpl(
      const base::FilePath& profile_dir,
      base::OnceClosure read_completed_for_testing = base::OnceClosure(),
      base::OnceClosure write_completed_for_testing = base::OnceClosure());

  AppServiceMojomImpl(const AppServiceMojomImpl&) = delete;
  AppServiceMojomImpl& operator=(const AppServiceMojomImpl&) = delete;

  ~AppServiceMojomImpl() override;

  void BindReceiver(mojo::PendingReceiver<apps::mojom::AppService> receiver);

  void FlushMojoCallsForTesting();

  // apps::mojom::AppService overrides.
  void RegisterPublisher(
      mojo::PendingRemote<apps::mojom::Publisher> publisher_remote,
      apps::mojom::AppType app_type) override;
  void RegisterSubscriber(
      mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
      apps::mojom::ConnectOptionsPtr opts) override;

 private:
  void OnPublisherDisconnected(apps::mojom::AppType app_type);

  // publishers_ is a std::map, not a mojo::RemoteSet, since we want to
  // be able to find *the* publisher for a given apps::mojom::AppType.
  std::map<apps::mojom::AppType, mojo::Remote<apps::mojom::Publisher>>
      publishers_;
  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  // Must come after the publisher and subscriber maps to ensure it is
  // destroyed first, closing the connection to avoid dangling callbacks.
  mojo::ReceiverSet<apps::mojom::AppService> receivers_;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_APP_SERVICE_MOJOM_IMPL_H_
