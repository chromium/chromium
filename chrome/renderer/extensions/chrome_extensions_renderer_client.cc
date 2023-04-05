// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_extensions_renderer_client.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/features.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/optional_util.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "chrome/renderer/extensions/chrome_extensions_dispatcher_delegate.h"
#include "chrome/renderer/extensions/renderer_permissions_policy_delegate.h"
#include "chrome/renderer/extensions/resource_request_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extension_web_view_helper.h"
#include "extensions/renderer/extensions_render_frame_observer.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_manager.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/script_context.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/site_for_cookies.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "url/origin.h"

using extensions::Extension;

namespace {

void IsGuestViewApiAvailableToScriptContext(
    bool* api_is_available,
    extensions::ScriptContext* context) {
  if (context->GetAvailability("guestViewInternal").is_available()) {
    *api_is_available = true;
  }
}

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

extensions::Dispatcher* ChromeExtensionsRendererClient::GetDispatcher() {
  return extension_dispatcher_.get();
}

void ChromeExtensionsRendererClient::OnExtensionLoaded(
    const extensions::Extension& extension) {
  resource_request_policy_->OnExtensionLoaded(extension);
}

void ChromeExtensionsRendererClient::OnExtensionUnloaded(
    const extensions::ExtensionId& extension_id) {
  resource_request_policy_->OnExtensionUnloaded(extension_id);
}

bool ChromeExtensionsRendererClient::ExtensionAPIEnabledForServiceWorkerScript(
    const GURL& scope,
    const GURL& script_url) const {
  if (!script_url.SchemeIs(extensions::kExtensionScheme))
    return false;

  const Extension* extension =
      extensions::RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(
          script_url);

  if (!extension ||
      !extensions::BackgroundInfo::IsServiceWorkerBased(extension))
    return false;

  if (scope != extension->url())
    return false;

  const std::string& sw_script =
      extensions::BackgroundInfo::GetBackgroundServiceWorkerScript(extension);

  return extension->GetResourceURL(sw_script) == script_url;
}

void ChromeExtensionsRendererClient::RenderThreadStarted() {
  content::RenderThread* thread = content::RenderThread::Get();
  // ChromeRenderViewTest::SetUp() creates its own ExtensionDispatcher and
  // injects it using SetExtensionDispatcher(). Don't overwrite it.
  if (!extension_dispatcher_) {
    extension_dispatcher_ = std::make_unique<extensions::Dispatcher>(
        std::make_unique<ChromeExtensionsDispatcherDelegate>());
  }
  extension_dispatcher_->OnRenderThreadStarted(thread);
  permissions_policy_delegate_ =
      std::make_unique<extensions::RendererPermissionsPolicyDelegate>(

          extension_dispatcher_.get());
  resource_request_policy_ =
      std::make_unique<extensions::ResourceRequestPolicy>(
          extension_dispatcher_.get());

  thread->AddObserver(extension_dispatcher_.get());
}

void ChromeExtensionsRendererClient::WebViewCreated(
    blink::WebView* web_view,
    const url::Origin* outermost_origin) {
  new extensions::ExtensionWebViewHelper(web_view, outermost_origin);
}

void ChromeExtensionsRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame,
    service_manager::BinderRegistry* registry) {
  new extensions::ExtensionsRenderFrameObserver(render_frame, registry);
  new extensions::ExtensionFrameHelper(render_frame,
                                       extension_dispatcher_.get());
  extension_dispatcher_->OnRenderFrameCreated(render_frame);
}

bool ChromeExtensionsRendererClient::OverrideCreatePlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {
  if (params.mime_type.Utf8() != content::kBrowserPluginMimeType)
    return true;

  bool guest_view_api_available = false;
  extension_dispatcher_->script_context_set_iterator()->ForEach(
      render_frame, base::BindRepeating(&IsGuestViewApiAvailableToScriptContext,
                                        &guest_view_api_available));
  return !guest_view_api_available;
}

bool ChromeExtensionsRendererClient::AllowPopup() {
  extensions::ScriptContext* current_context =
      extension_dispatcher_->script_context_set().GetCurrent();
  if (!current_context || !current_context->extension())
    return false;

  // See http://crbug.com/117446 for the subtlety of this check.
  switch (current_context->context_type()) {
    case extensions::Feature::UNSPECIFIED_CONTEXT:
    case extensions::Feature::WEB_PAGE_CONTEXT:
    case extensions::Feature::UNBLESSED_EXTENSION_CONTEXT:
    case extensions::Feature::WEBUI_CONTEXT:
    case extensions::Feature::WEBUI_UNTRUSTED_CONTEXT:
    case extensions::Feature::OFFSCREEN_EXTENSION_CONTEXT:
    case extensions::Feature::USER_SCRIPT_CONTEXT:
    case extensions::Feature::LOCK_SCREEN_EXTENSION_CONTEXT:
      return false;
    case extensions::Feature::BLESSED_EXTENSION_CONTEXT:
      return !current_context->IsForServiceWorker();
    case extensions::Feature::CONTENT_SCRIPT_CONTEXT:
      return true;
    case extensions::Feature::BLESSED_WEB_PAGE_CONTEXT:
      return current_context->web_frame()->IsOutermostMainFrame();
  }
}

blink::ProtocolHandlerSecurityLevel
ChromeExtensionsRendererClient::GetProtocolHandlerSecurityLevel() {
  // WARNING: This must match the logic of
  // Browser::GetProtocolHandlerSecurityLevel().
  extensions::ScriptContext* current_context =
      extension_dispatcher_->script_context_set().GetCurrent();
  if (!current_context || !current_context->extension())
    return blink::ProtocolHandlerSecurityLevel::kStrict;

  switch (current_context->context_type()) {
    case extensions::Feature::BLESSED_WEB_PAGE_CONTEXT:
    case extensions::Feature::CONTENT_SCRIPT_CONTEXT:
    case extensions::Feature::LOCK_SCREEN_EXTENSION_CONTEXT:
    case extensions::Feature::OFFSCREEN_EXTENSION_CONTEXT:
    case extensions::Feature::UNBLESSED_EXTENSION_CONTEXT:
    case extensions::Feature::UNSPECIFIED_CONTEXT:
    case extensions::Feature::USER_SCRIPT_CONTEXT:
    case extensions::Feature::WEBUI_CONTEXT:
    case extensions::Feature::WEBUI_UNTRUSTED_CONTEXT:
    case extensions::Feature::WEB_PAGE_CONTEXT:
      return blink::ProtocolHandlerSecurityLevel::kStrict;
    case extensions::Feature::BLESSED_EXTENSION_CONTEXT:
      return blink::ProtocolHandlerSecurityLevel::kExtensionFeatures;
  }
}

void ChromeExtensionsRendererClient::WillSendRequest(
    blink::WebLocalFrame* frame,
    ui::PageTransition transition_type,
    const blink::WebURL& url,
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
      *new_url = GURL(chrome::kExtensionInvalidRequestURL);
    }
  }

  // The rest of this method is only concerned with extensions URLs.
  if (base::FeatureList::IsEnabled(base::features::kOptimizeDataUrls) &&
      !url.ProtocolIs(extensions::kExtensionScheme)) {
    return;
  }

  if (url.ProtocolIs(extensions::kExtensionScheme) &&
      !resource_request_policy_->CanRequestResource(
          GURL(url), frame, transition_type,
          base::OptionalFromPtr(initiator_origin))) {
    *new_url = GURL(chrome::kExtensionInvalidRequestURL);
  }

  // TODO(https://crbug.com/588766): Remove metrics after bug is fixed.
  GURL request_url(url);
  if (url.ProtocolIs(extensions::kExtensionScheme) &&
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

void ChromeExtensionsRendererClient::SetExtensionDispatcherForTest(
    std::unique_ptr<extensions::Dispatcher> extension_dispatcher) {
  extension_dispatcher_ = std::move(extension_dispatcher);
  permissions_policy_delegate_ =
      std::make_unique<extensions::RendererPermissionsPolicyDelegate>(

          extension_dispatcher_.get());
}

extensions::Dispatcher*
ChromeExtensionsRendererClient::GetExtensionDispatcherForTest() {
  return extension_dispatcher();
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

v8::Local<v8::Object> ChromeExtensionsRendererClient::GetScriptableObject(
    const blink::WebElement& plugin_element,
    v8::Isolate* isolate) {
  // If there is a MimeHandlerView that can provide the scriptable object then
  // MaybeCreateMimeHandlerView must have been called before and a container
  // manager should exist.
  auto* container_manager = extensions::MimeHandlerViewContainerManager::Get(
      content::RenderFrame::FromWebFrame(
          plugin_element.GetDocument().GetFrame()),
      false /* create_if_does_not_exist */);
  if (container_manager)
    return container_manager->GetScriptableObject(plugin_element, isolate);
  return v8::Local<v8::Object>();
}

// static
blink::WebFrame* ChromeExtensionsRendererClient::FindFrame(
    blink::WebLocalFrame* relative_to_frame,
    const std::string& name) {
  content::RenderFrame* result = extensions::ExtensionFrameHelper::FindFrame(
      content::RenderFrame::FromWebFrame(relative_to_frame), name);
  return result ? result->GetWebFrame() : nullptr;
}

void ChromeExtensionsRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  extension_dispatcher_->RunScriptsAtDocumentStart(render_frame);
}

void ChromeExtensionsRendererClient::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {
  extension_dispatcher_->RunScriptsAtDocumentEnd(render_frame);
}

void ChromeExtensionsRendererClient::RunScriptsAtDocumentIdle(
    content::RenderFrame* render_frame) {
  extension_dispatcher_->RunScriptsAtDocumentIdle(render_frame);
}
