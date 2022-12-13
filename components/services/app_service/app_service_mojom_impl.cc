// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/app_service_mojom_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/preferred_apps_list.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/clone_traits.h"

namespace {

void Connect(apps::mojom::Publisher* publisher,
             apps::mojom::Subscriber* subscriber) {
  mojo::PendingRemote<apps::mojom::Subscriber> clone;
  subscriber->Clone(clone.InitWithNewPipeAndPassReceiver());
  // TODO: replace nullptr with a ConnectOptions.
  publisher->Connect(std::move(clone), nullptr);
}

}  // namespace

namespace apps {

AppServiceMojomImpl::AppServiceMojomImpl(
    const base::FilePath& profile_dir,
    base::OnceClosure read_completed_for_testing,
    base::OnceClosure write_completed_for_testing) {}

AppServiceMojomImpl::~AppServiceMojomImpl() = default;

void AppServiceMojomImpl::BindReceiver(
    mojo::PendingReceiver<apps::mojom::AppService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AppServiceMojomImpl::FlushMojoCallsForTesting() {
  subscribers_.FlushForTesting();
  receivers_.FlushForTesting();
}

void AppServiceMojomImpl::RegisterPublisher(
    mojo::PendingRemote<apps::mojom::Publisher> publisher_remote,
    apps::mojom::AppType app_type) {
  mojo::Remote<apps::mojom::Publisher> publisher(std::move(publisher_remote));
  // Connect the new publisher with every registered subscriber.
  for (auto& subscriber : subscribers_) {
    ::Connect(publisher.get(), subscriber.get());
  }

  // Check that no previous publisher has registered for the same app_type.
  CHECK(publishers_.find(app_type) == publishers_.end());

  // Add the new publisher to the set.
  publisher.set_disconnect_handler(
      base::BindOnce(&AppServiceMojomImpl::OnPublisherDisconnected,
                     base::Unretained(this), app_type));
  auto result = publishers_.emplace(app_type, std::move(publisher));
  CHECK(result.second);
}

void AppServiceMojomImpl::RegisterSubscriber(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  // Connect the new subscriber with every registered publisher.
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  for (const auto& iter : publishers_) {
    ::Connect(iter.second.get(), subscriber.get());
  }

  // Add the new subscriber to the set.
  subscribers_.Add(std::move(subscriber));
}

void AppServiceMojomImpl::OnPublisherDisconnected(
    apps::mojom::AppType app_type) {
  publishers_.erase(app_type);
}

}  // namespace apps
