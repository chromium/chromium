// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_loader_factory.h"

#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolation_data.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/web_bundle_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace web_app {

namespace {

void CompleteWith404(
    mojo::Remote<network::mojom::URLLoaderClient> loader_client) {
  auto generated_response = web_package::mojom::BundleResponse::New();
  generated_response->response_code = net::HTTP_NOT_FOUND;
  // Setting the Content-Type header makes Chrome return a nicer error page
  // that shows the actual error code ("HTTP ERROR 404") instead of just
  // "ERR_INVALID_RESPONSE".
  generated_response->response_headers["Content-Type"] =
      "text/html;charset=utf-8";
  auto response_head = web_package::CreateResourceResponse(generated_response);

  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  mojo::ScopedDataPipeProducerHandle producer_handle;

  auto result = mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle);
  if (result != MOJO_RESULT_OK) {
    loader_client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    return;
  }
  producer_handle.reset();  // The response is empty.

  loader_client->OnReceiveResponse(std::move(response_head),
                                   std::move(consumer_handle), absl::nullopt);

  loader_client->OnComplete(network::URLLoaderCompletionStatus(net::OK));
}

void LogErrorMessageToConsole(int frame_tree_node_id,
                              const std::string& error_message) {
  // TODO(crbug.com/1334594): The console message will vanish from the console
  // if the user does not have the `Preserve Log` option enabled, since it is
  // triggered before the navigation commits. We should try to use a similar
  // approach as in crrev.com/c/3397976, but `FrameTreeNode` is not part of
  // content/public.

  // Find the `RenderFrameHost` associated with the `FrameTreeNode`
  // corresponding to the `frame_tree_node_id`, and then log the message.
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents) {
    return;
  }
  web_contents->ForEachRenderFrameHostWithAction(
      [frame_tree_node_id,
       &error_message](content::RenderFrameHost* render_frame_host) {
        if (render_frame_host->GetFrameTreeNodeId() == frame_tree_node_id) {
          render_frame_host->AddMessageToConsole(
              blink::mojom::ConsoleMessageLevel::kError, error_message);
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
}

base::expected<std::reference_wrapper<const WebApp>, std::string>
FindIsolatedWebApp(Profile* profile, const IsolatedWebAppUrlInfo& url_info) {
  // TODO(b/242738845): Defer navigation in IsolatedAppThrottle until
  // WebAppProvider is ready to ensure we never fail this DCHECK.
  auto* web_app_provider = WebAppProvider::GetForWebApps(profile);
  DCHECK(web_app_provider->is_registry_ready());
  const WebAppRegistrar& registrar = web_app_provider->registrar();
  const WebApp* iwa = registrar.GetAppById(url_info.app_id());

  if (iwa == nullptr || !iwa->is_locally_installed()) {
    return base::unexpected(base::StrCat(
        {"Isolated Web App not installed: ", url_info.origin().Serialize()}));
  }

  if (!iwa->isolation_data().has_value()) {
    return base::unexpected(base::StrCat(
        {"App is not an Isolated Web App: ", url_info.origin().Serialize()}));
  }

  return *iwa;
}

}  // namespace

IsolatedWebAppURLLoaderFactory::IsolatedWebAppURLLoaderFactory(
    int frame_tree_node_id,
    Profile* profile,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver)
    : network::SelfDeletingURLLoaderFactory(std::move(factory_receiver)),
      frame_tree_node_id_(frame_tree_node_id),
      profile_(profile) {}

IsolatedWebAppURLLoaderFactory::~IsolatedWebAppURLLoaderFactory() = default;

void IsolatedWebAppURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(resource_request.url.SchemeIs(chrome::kIsolatedAppScheme));
  DCHECK(resource_request.url.IsStandard());

  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(resource_request.url);
  if (!url_info.has_value()) {
    LogErrorAndFail(url_info.error(), std::move(loader_client));
    return;
  }

  base::expected<std::reference_wrapper<const WebApp>, std::string> iwa =
      FindIsolatedWebApp(profile_, *url_info);
  if (!iwa.has_value()) {
    LogErrorAndFail(iwa.error(), std::move(loader_client));
    return;
  }

  IsolationData isolation_data = iwa->get().isolation_data().value();
  if (absl::holds_alternative<IsolationData::DevModeProxy>(
          isolation_data.content)) {
    CompleteWith404(mojo::Remote<network::mojom::URLLoaderClient>(
        std::move(loader_client)));
    return;
  }

  LogErrorAndFail("IsolatedWebAppURLLoaderFactory not implemented",
                  std::move(loader_client));
}

void IsolatedWebAppURLLoaderFactory::LogErrorAndFail(
    const std::string& error_message,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  LogErrorMessageToConsole(frame_tree_node_id_, error_message);

  mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
      ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
IsolatedWebAppURLLoaderFactory::Create(
    int frame_tree_node_id,
    content::BrowserContext* browser_context) {
  DCHECK(browser_context);
  DCHECK(!browser_context->ShutdownStarted());

  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;

  // The IsolatedWebAppURLLoaderFactory will delete itself when there are no
  // more receivers - see the
  // network::SelfDeletingURLLoaderFactory::OnDisconnect method.
  new IsolatedWebAppURLLoaderFactory(
      frame_tree_node_id, Profile::FromBrowserContext(browser_context),
      pending_remote.InitWithNewPipeAndPassReceiver());

  return pending_remote;
}

}  // namespace web_app
