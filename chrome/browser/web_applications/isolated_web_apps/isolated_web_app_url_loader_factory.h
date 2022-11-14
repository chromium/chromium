// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_URL_LOADER_FACTORY_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/web_applications/isolation_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"

namespace content {
class BrowserContext;
}

namespace net {
struct MutableNetworkTrafficAnnotationTag;
}

namespace network {
struct ResourceRequest;
}

namespace web_package {
class SignedWebBundleId;
}

namespace web_app {

class IsolatedWebAppUrlInfo;

// A URLLoaderFactory used for the isolated-app:// scheme.
class IsolatedWebAppURLLoaderFactory
    : public network::SelfDeletingURLLoaderFactory,
      public ProfileObserver {
 public:
  // Returns mojo::PendingRemote to a newly constructed
  // IsolatedWebAppURLLoaderFactory. The factory is self-owned - it will delete
  // itself once there are no more receivers (including the receiver associated
  // with the returned mojo::PendingRemote and the receivers bound by the Clone
  // method).
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create(
      int frame_tree_node_id,
      content::BrowserContext* browser_context);

  // The same as `Create`, but doesn't have access to the frame tree.
  static mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateForServiceWorker(content::BrowserContext* browser_context);

  IsolatedWebAppURLLoaderFactory(const IsolatedWebAppURLLoaderFactory&) =
      delete;
  IsolatedWebAppURLLoaderFactory& operator=(
      const IsolatedWebAppURLLoaderFactory&) = delete;

 private:
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> CreateInternal(
      absl::optional<int> frame_tree_node_id,
      content::BrowserContext* browser_context);

  IsolatedWebAppURLLoaderFactory(
      absl::optional<int> frame_tree_node_id,
      Profile* profile,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);

  void HandleSignedBundle(
      const base::FilePath& path,
      const web_package::SignedWebBundleId& web_bundle_id,
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client);

  void HandleDevModeProxy(
      const IsolatedWebAppUrlInfo& url_info,
      const IsolationData::DevModeProxy& dev_mode_proxy,
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

  void LogErrorAndFail(
      const std::string& error_message,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  // network::mojom::URLLoaderFactory:
  ~IsolatedWebAppURLLoaderFactory() override;
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  const absl::optional<int> frame_tree_node_id_;
  // It is safe to store a pointer to a `Profile` here, since `this` is freed
  // via `profile_observation_` when the `Profile` is destroyed.
  const raw_ptr<Profile> profile_;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_URL_LOADER_FACTORY_H_
