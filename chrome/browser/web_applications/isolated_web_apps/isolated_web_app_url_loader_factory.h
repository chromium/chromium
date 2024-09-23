// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_URL_LOADER_FACTORY_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "url/origin.h"

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

class IwaSourceProxy;
class IwaSourceWithMode;
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
  //
  // If `app_origin` is present, the factory will only handle requests that are
  // same-origin to it.
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> CreateForFrame(
      content::BrowserContext* browser_context,
      std::optional<url::Origin> app_origin,
      content::FrameTreeNodeId frame_tree_node_id);

  // The same as `CreateForFrame`, but doesn't have access to a FrameTreeNode
  // to log errors to.
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create(
      content::BrowserContext* browser_context,
      std::optional<url::Origin> app_origin);

  IsolatedWebAppURLLoaderFactory(const IsolatedWebAppURLLoaderFactory&) =
      delete;
  IsolatedWebAppURLLoaderFactory& operator=(
      const IsolatedWebAppURLLoaderFactory&) = delete;

 private:
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> CreateInternal(
      content::BrowserContext* browser_context,
      std::optional<url::Origin> app_origin,
      std::optional<content::FrameTreeNodeId> frame_tree_node_id);

  IsolatedWebAppURLLoaderFactory(
      Profile* profile,
      std::optional<url::Origin> app_origin,
      std::optional<content::FrameTreeNodeId> frame_tree_node_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);

  void HandleSignedBundle(
      const base::FilePath& path,
      bool dev_mode,
      const web_package::SignedWebBundleId& web_bundle_id,
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client);

  void HandleProxy(
      const IsolatedWebAppUrlInfo& url_info,
      const IwaSourceProxy& proxy,
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

  void HandleRequest(
      const IsolatedWebAppUrlInfo& url_info,
      const IwaSourceWithMode& source,
      bool is_pending_install,
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  bool CanRequestUrl(const GURL& url) const;

  // It is safe to store a pointer to a `Profile` here, since `this` is freed
  // via `profile_observation_` when the `Profile` is destroyed.
  const raw_ptr<Profile> profile_;
  const std::optional<url::Origin> app_origin_;
  const std::optional<content::FrameTreeNodeId> frame_tree_node_id_;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
  base::WeakPtrFactory<IsolatedWebAppURLLoaderFactory> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_URL_LOADER_FACTORY_H_
