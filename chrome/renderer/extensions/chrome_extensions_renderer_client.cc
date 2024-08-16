// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_extensions_renderer_client.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "chrome/renderer/extensions/renderer_permissions_policy_delegate.h"
#include "chrome/renderer/extensions/resource_request_policy.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_manager.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/site_for_cookies.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "url/origin.h"

using extensions::Extension;

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GoogleDocsExtensionAvailablity {
  kAvailableRegular = 0,
  kNotAvailableRegular = 1,
  kAvailableIncognito = 2,
  kNotAvailableIncognito = 3,
  kMaxValue = kNotAvailableIncognito
};

}  // namespace

ChromeExtensionsRendererClient::ChromeExtensionsRendererClient() {}

ChromeExtensionsRendererClient::~ChromeExtensionsRendererClient() {}

// static
ChromeExtensionsRendererClient* ChromeExtensionsRendererClient::GetInstance() {
  static base::LazyInstance<ChromeExtensionsRendererClient>::Leaky client =
      LAZY_INSTANCE_INITIALIZER;
  return client.Pointer();
}

bool ChromeExtensionsRendererClient::IsIncognitoProcess() const {
  return ChromeRenderThreadObserver::is_incognito_process();
}

int ChromeExtensionsRendererClient::GetLowestIsolatedWorldId() const {
  return ISOLATED_WORLD_ID_EXTENSIONS;
}

void ChromeExtensionsRendererClient::OnExtensionLoaded(
    const extensions::Extension& extension) {
  resource_request_policy_->OnExtensionLoaded(extension);
}

void ChromeExtensionsRendererClient::OnExtensionUnloaded(
    const extensions::ExtensionId& extension_id) {
  resource_request_policy_->OnExtensionUnloaded(extension_id);
}

void ChromeExtensionsRendererClient::FinishInitialization() {
  permissions_policy_delegate_ =
      std::make_unique<extensions::RendererPermissionsPolicyDelegate>(
          dispatcher());
  resource_request_policy_ =
      std::make_unique<extensions::ResourceRequestPolicy>(dispatcher());
}

void ChromeExtensionsRendererClient::WillSendRequest(
    blink::WebLocalFrame* frame,
    ui::PageTransition transition_type,
    const blink::WebURL& upstream_url,
    const blink::WebURL& target_url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* initiator_origin,
    GURL* new_url) {
  std::string extension_id;
  if (initiator_origin &&
      initiator_origin->scheme() == extensions::kExtensionScheme) {
    extension_id = initiator_origin->host();
  } else {
    if (site_for_cookies.scheme() == extensions::kExtensionScheme) {
      extension_id = site_for_cookies.registrable_domain();
    }
  }

  if (!extension_id.empty()) {
    const extensions::RendererExtensionRegistry* extension_registry =
        extensions::RendererExtensionRegistry::Get();
    const Extension* extension = extension_registry->GetByID(extension_id);
    if (!extension) {
      // If there is no extension installed for the origin, it may be from a
      // recently uninstalled extension.  The tabs of such extensions are
      // automatically closed, but subframes and content scripts may stick
      // around. Fail such requests without killing the process.
      *new_url = GURL(extensions::kExtensionInvalidRequestURL);
    }
  }

  // The rest of this method is only concerned with extensions URLs.
  if (!target_url.ProtocolIs(extensions::kExtensionScheme)) {
    return;
  }

  if (target_url.ProtocolIs(extensions::kExtensionScheme) &&
      !resource_request_policy_->CanRequestResource(
          upstream_url, target_url, frame, transition_type, initiator_origin)) {
    *new_url = GURL(extensions::kExtensionInvalidRequestURL);
  }

  // TODO(crbug.com/41240557): Remove metrics after bug is fixed.
  GURL request_url(target_url);
  if (target_url.ProtocolIs(extensions::kExtensionScheme) &&
      request_url.host_piece() == extension_misc::kDocsOfflineExtensionId) {
    if (!ukm_recorder_) {
      mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
      content::RenderThread::Get()->BindHostReceiver(
          factory.BindNewPipeAndPassReceiver());
      ukm_recorder_ = ukm::MojoUkmRecorder::Create(*factory);
    }

    const ukm::SourceId source_id = frame->GetDocument().GetUkmSourceId();
    ukm::builders::GoogleDocsOfflineExtension(source_id)
        .SetResourceRequested(true)
        .Record(ukm_recorder_.get());

    bool is_available = extensions::RendererExtensionRegistry::Get()->GetByID(
                            extension_misc::kDocsOfflineExtensionId) != nullptr;
    bool is_incognito = IsIncognitoProcess();
    GoogleDocsExtensionAvailablity vote;
    if (is_incognito) {
      vote = is_available
                 ? GoogleDocsExtensionAvailablity::kAvailableIncognito
                 : GoogleDocsExtensionAvailablity::kNotAvailableIncognito;
    } else {
      vote = is_available
                 ? GoogleDocsExtensionAvailablity::kAvailableRegular
                 : GoogleDocsExtensionAvailablity::kNotAvailableRegular;
    }
    base::UmaHistogramEnumeration(
        "Extensions.GoogleDocOffline.AvailabilityOnResourceRequest", vote);
  }
}

// static
void ChromeExtensionsRendererClient::DidBlockMimeHandlerViewForDisallowedPlugin(
    const blink::WebElement& plugin_element) {
  extensions::MimeHandlerViewContainerManager::Get(
      content::RenderFrame::FromWebFrame(
          plugin_element.GetDocument().GetFrame()),
      true /* create_if_does_not_exist */)
      ->DidBlockMimeHandlerViewForDisallowedPlugin(plugin_element);
}

// static
bool ChromeExtensionsRendererClient::MaybeCreateMimeHandlerView(
    const blink::WebElement& plugin_element,
    const GURL& resource_url,
    const std::string& mime_type,
    const content::WebPluginInfo& plugin_info) {
  return extensions::MimeHandlerViewContainerManager::Get(
             content::RenderFrame::FromWebFrame(
                 plugin_element.GetDocument().GetFrame()),
             true /* create_if_does_not_exist */)
      ->CreateFrameContainer(plugin_element, resource_url, mime_type,
                             plugin_info);
}
