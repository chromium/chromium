// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_loader_factory.h"

#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace web_app {

namespace {

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

}  // namespace

IsolatedWebAppURLLoaderFactory::IsolatedWebAppURLLoaderFactory(
    int frame_tree_node_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver)
    : network::SelfDeletingURLLoaderFactory(std::move(factory_receiver)),
      frame_tree_node_id_(frame_tree_node_id) {}

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
      frame_tree_node_id, pending_remote.InitWithNewPipeAndPassReceiver());

  return pending_remote;
}

}  // namespace web_app
