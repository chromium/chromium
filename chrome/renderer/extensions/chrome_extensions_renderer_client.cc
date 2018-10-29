// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_extensions_renderer_client.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "chrome/renderer/extensions/chrome_extensions_dispatcher_delegate.h"
#include "chrome/renderer/extensions/extension_process_policy.h"
#include "chrome/renderer/extensions/renderer_permissions_policy_delegate.h"
#include "chrome/renderer/extensions/resource_request_policy.h"
#include "chrome/renderer/media/cast_ipc_dispatcher.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/mime_handler_view_mode.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extensions_render_frame_observer.h"
#include "extensions/renderer/guest_view/extensions_guest_view_container.h"
#include "extensions/renderer/guest_view/extensions_guest_view_container_dispatcher.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_frame_container.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "url/origin.h"

using extensions::Extension;

namespace {

bool IsStandaloneExtensionProcess() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      extensions::switches::kExtensionProcess);
}

void IsGuestViewApiAvailableToScriptContext(
    bool* api_is_available,
    extensions::ScriptContext* context) {
  if (context->GetAvailability("guestViewInternal").is_available()) {
    *api_is_available = true;
  }
}

// Returns true if the frame is navigating to an URL either into or out of an
// extension app's extent.
bool CrossesExtensionExtents(blink::WebLocalFrame* frame,
                             const GURL& new_url,
                             bool is_extension_url,
                             bool is_initial_navigation) {
  DCHECK(!frame->Parent());
  GURL old_url(frame->GetDocument().Url());

  extensions::RendererExtensionRegistry* extension_registry =
      extensions::RendererExtensionRegistry::Get();

  // If old_url is still empty and this is an initial navigation, then this is
  // a window.open operation.  We should look at the opener URL.  Note that the
  // opener is a local frame in this case.
  if (is_initial_navigation && old_url.is_empty() && frame->Opener()) {
    blink::WebLocalFrame* opener_frame = frame->Opener()->ToWebLocalFrame();

    // We want to compare against the URL that determines the type of
    // process.  Use the URL of the opener's local frame root, which will
    // correctly handle any site isolation modes (e.g. --site-per-process).
    blink::WebLocalFrame* local_root = opener_frame->LocalRoot();
    old_url = local_root->GetDocument().Url();

    // If we're about to open a normal web page from a same-origin opener stuck
    // in an extension process (other than the Chrome Web Store), we want to
    // keep it in process to allow the opener to script it.
    blink::WebDocument opener_document = opener_frame->GetDocument();
    blink::WebSecurityOrigin opener_origin =
        opener_document.GetSecurityOrigin();
    bool opener_is_extension_url =
        !opener_origin.IsUnique() && extension_registry->GetExtensionOrAppByURL(
                                         opener_document.Url()) != nullptr;
    const Extension* opener_top_extension =
        extension_registry->GetExtensionOrAppByURL(old_url);
    bool opener_is_web_store =
        opener_top_extension &&
        opener_top_extension->id() == extensions::kWebStoreAppId;
    if (!is_extension_url && !opener_is_extension_url && !opener_is_web_store &&
        IsStandaloneExtensionProcess() &&
        opener_origin.CanRequest(blink::WebURL(new_url)))
      return false;
  }

  // Only consider keeping non-app URLs in an app process if this window
  // has an opener (in which case it might be an OAuth popup that tries to
  // script an iframe within the app).
  bool should_consider_workaround = !!frame->Opener();

  return extensions::CrossesExtensionProcessBoundary(
      *extension_registry->GetMainThreadExtensionSet(), old_url, new_url,
      should_consider_workaround);
}

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

void ChromeExtensionsRendererClient::RenderThreadStarted() {
  content::RenderThread* thread = content::RenderThread::Get();
  // ChromeRenderViewTest::SetUp() creates its own ExtensionDispatcher and
  // injects it using SetExtensionDispatcher(). Don't overwrite it.
  if (!extension_dispatcher_) {
    extension_dispatcher_ = std::make_unique<extensions::Dispatcher>(
        std::make_unique<ChromeExtensionsDispatcherDelegate>());
  }
  extension_dispatcher_->OnRenderThreadStarted(thread);
  permissions_policy_delegate_.reset(
      new extensions::RendererPermissionsPolicyDelegate(
          extension_dispatcher_.get()));
  resource_request_policy_.reset(
      new extensions::ResourceRequestPolicy(extension_dispatcher_.get()));
  guest_view_container_dispatcher_.reset(
      new extensions::ExtensionsGuestViewContainerDispatcher());

  thread->AddObserver(extension_dispatcher_.get());
  thread->AddObserver(guest_view_container_dispatcher_.get());
  thread->AddFilter(new CastIPCDispatcher(thread->GetIOTaskRunner()));
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
  extension_dispatcher_->script_context_set().ForEach(
      render_frame, base::Bind(&IsGuestViewApiAvailableToScriptContext,
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
    case extensions::Feature::SERVICE_WORKER_CONTEXT:
    case extensions::Feature::LOCK_SCREEN_EXTENSION_CONTEXT:
      return false;
    case extensions::Feature::BLESSED_EXTENSION_CONTEXT:
    case extensions::Feature::CONTENT_SCRIPT_CONTEXT:
      return true;
    case extensions::Feature::BLESSED_WEB_PAGE_CONTEXT:
      return !current_context->web_frame()->Parent();
    default:
      NOTREACHED();
      return false;
  }
}

void ChromeExtensionsRendererClient::WillSendRequest(
    blink::WebLocalFrame* frame,
    ui::PageTransition transition_type,
    const blink::WebURL& url,
    const url::Origin* initiator_origin,
    GURL* new_url,
    bool* attach_same_site_cookies) {
  if (initiator_origin &&
      initiator_origin->scheme() == extensions::kExtensionScheme) {
    const extensions::RendererExtensionRegistry* extension_registry =
        extensions::RendererExtensionRegistry::Get();
    const Extension* extension =
        extension_registry->GetByID(initiator_origin->host());
    if (extension) {
      int tab_id = extensions::ExtensionFrameHelper::Get(
                       content::RenderFrame::FromWebFrame(frame))
                       ->tab_id();
      GURL request_url(url);
      if (extension->permissions_data()->GetPageAccess(request_url, tab_id,
                                                       nullptr) ==
              extensions::PermissionsData::PageAccess::kAllowed ||
          extension->permissions_data()->GetContentScriptAccess(
              request_url, tab_id, nullptr) ==
              extensions::PermissionsData::PageAccess::kAllowed) {
        *attach_same_site_cookies = true;
      }
    }
  }

  if (url.ProtocolIs(extensions::kExtensionScheme) &&
      !resource_request_policy_->CanRequestResource(GURL(url), frame,
                                                    transition_type)) {
    *new_url = GURL(chrome::kExtensionInvalidRequestURL);
  }
}

void ChromeExtensionsRendererClient::SetExtensionDispatcherForTest(
    std::unique_ptr<extensions::Dispatcher> extension_dispatcher) {
  extension_dispatcher_ = std::move(extension_dispatcher);
  permissions_policy_delegate_.reset(
      new extensions::RendererPermissionsPolicyDelegate(
          extension_dispatcher_.get()));
}

extensions::Dispatcher*
ChromeExtensionsRendererClient::GetExtensionDispatcherForTest() {
  return extension_dispatcher();
}

// static
bool ChromeExtensionsRendererClient::ShouldFork(blink::WebLocalFrame* frame,
                                                const GURL& url,
                                                bool is_initial_navigation,
                                                bool is_server_redirect) {
  const extensions::RendererExtensionRegistry* extension_registry =
      extensions::RendererExtensionRegistry::Get();

  // Determine if the new URL is an extension (excluding bookmark apps).
  const Extension* new_url_extension = extensions::GetNonBookmarkAppExtension(
      *extension_registry->GetMainThreadExtensionSet(), url);
  bool is_extension_url = !!new_url_extension;

  // If the navigation would cross an app extent boundary, we also need
  // to defer to the browser to ensure process isolation.  This is not necessary
  // for server redirects, which will be transferred to a new process by the
  // browser process when they are ready to commit.  It is necessary for client
  // redirects, which won't be transferred in the same way.
  if (!is_server_redirect &&
      CrossesExtensionExtents(frame, url, is_extension_url,
                              is_initial_navigation)) {
    const Extension* extension =
        extension_registry->GetExtensionOrAppByURL(url);
    if (extension && extension->is_app()) {
      extensions::RecordAppLaunchType(
          extension_misc::APP_LAUNCH_CONTENT_NAVIGATION, extension->GetType());
    }
    return true;
  }

  return false;
}

// static
content::BrowserPluginDelegate*
ChromeExtensionsRendererClient::CreateBrowserPluginDelegate(
    content::RenderFrame* render_frame,
    const content::WebPluginInfo& info,
    const std::string& mime_type,
    const GURL& original_url) {
  if (mime_type == content::kBrowserPluginMimeType)
    return new extensions::ExtensionsGuestViewContainer(render_frame);
  return new extensions::MimeHandlerViewContainer(render_frame, info, mime_type,
                                                  original_url);
}

// static
bool ChromeExtensionsRendererClient::MaybeCreateMimeHandlerView(
    const blink::WebElement& plugin_element,
    const GURL& resource_url,
    const std::string& mime_type,
    const content::WebPluginInfo& plugin_info,
    int32_t element_instance_id) {
  CHECK(content::MimeHandlerViewMode::UsesCrossProcessFrame());
  return extensions::MimeHandlerViewFrameContainer::Create(
      plugin_element, resource_url, mime_type, plugin_info,
      element_instance_id);
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
