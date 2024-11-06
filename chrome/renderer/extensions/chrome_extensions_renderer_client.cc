// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_extensions_renderer_client.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/renderer/extensions/chrome_resource_request_policy_delegate.h"
#include "chrome/renderer/extensions/renderer_permissions_policy_delegate.h"
#include "chrome/renderer/process_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/renderer/dispatcher.h"
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

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_manager.h"
#endif

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

ChromeExtensionsRendererClient::ChromeExtensionsRendererClient() {
  ExtensionsRendererClient::Set(this);
}

ChromeExtensionsRendererClient::~ChromeExtensionsRendererClient() {
  ExtensionsRendererClient::Set(nullptr);
}

// static
void ChromeExtensionsRendererClient::Create() {
  static base::LazyInstance<ChromeExtensionsRendererClient>::Leaky client =
      LAZY_INSTANCE_INITIALIZER;
  client.Pointer();
}

bool ChromeExtensionsRendererClient::IsIncognitoProcess() const {
  return ::IsIncognitoProcess();
}

int ChromeExtensionsRendererClient::GetLowestIsolatedWorldId() const {
  return ISOLATED_WORLD_ID_EXTENSIONS;
}

void ChromeExtensionsRendererClient::FinishInitialization() {
  permissions_policy_delegate_ =
      std::make_unique<extensions::RendererPermissionsPolicyDelegate>(
          dispatcher());
}

std::unique_ptr<extensions::ResourceRequestPolicy::Delegate>
ChromeExtensionsRendererClient::CreateResourceRequestPolicyDelegate() {
  return std::make_unique<extensions::ChromeResourceRequestPolicyDelegate>();
}

void ChromeExtensionsRendererClient::RecordMetricsForURLRequest(
    blink::WebLocalFrame* frame,
    const blink::WebURL& target_url) {
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
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  extensions::MimeHandlerViewContainerManager::Get(
      content::RenderFrame::FromWebFrame(
          plugin_element.GetDocument().GetFrame()),
      true /* create_if_does_not_exist */)
      ->DidBlockMimeHandlerViewForDisallowedPlugin(plugin_element);
#endif
}

// static
bool ChromeExtensionsRendererClient::MaybeCreateMimeHandlerView(
    const blink::WebElement& plugin_element,
    const GURL& resource_url,
    const std::string& mime_type,
    const content::WebPluginInfo& plugin_info) {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  return extensions::MimeHandlerViewContainerManager::Get(
             content::RenderFrame::FromWebFrame(
                 plugin_element.GetDocument().GetFrame()),
             true /* create_if_does_not_exist */)
      ->CreateFrameContainer(plugin_element, resource_url, mime_type,
                             plugin_info);
#else
  return false;
#endif
}
