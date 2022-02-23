// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/cast_extensions_browser_client.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chromecast/browser/cast_network_contexts.h"
#include "chromecast/browser/extensions/cast_extension_host_delegate.h"
#include "chromecast/browser/extensions/cast_extension_system_factory.h"
#include "chromecast/browser/extensions/cast_extension_web_contents_observer.h"
#include "chromecast/browser/extensions/cast_extensions_api_client.h"
#include "chromecast/browser/extensions/cast_extensions_browser_api_provider.h"
#include "chromecast/browser/extensions/cast_kiosk_delegate.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/user_agent.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/runtime/runtime_api_delegate.h"
#include "extensions/browser/core_extensions_browser_api_provider.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_browser_interface_binders.h"
#include "extensions/browser/null_app_sorting.h"
#include "extensions/browser/updater/null_extension_cache.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/common/features/feature_channel.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

CastExtensionsBrowserClient::CastExtensionsBrowserClient(
    BrowserContext* context,
    PrefService* pref_service,
    chromecast::shell::CastNetworkContexts* cast_network_contexts)
    : browser_context_(context),
      cast_network_contexts_(cast_network_contexts),
      pref_service_(pref_service),
      api_client_(new CastExtensionsAPIClient) {
  DCHECK(cast_network_contexts_);
  // Set to UNKNOWN to enable all APIs.
  // TODO(achaulk): figure out what channel to use here.
  SetCurrentChannel(version_info::Channel::UNKNOWN);

  AddAPIProvider(std::make_unique<CoreExtensionsBrowserAPIProvider>());
  AddAPIProvider(std::make_unique<CastExtensionsBrowserAPIProvider>());
}

CastExtensionsBrowserClient::~CastExtensionsBrowserClient() {}

network::mojom::NetworkContext*
CastExtensionsBrowserClient::GetSystemNetworkContext() {
  return cast_network_contexts_->GetSystemContext();
}

bool CastExtensionsBrowserClient::IsShuttingDown() {
  return false;
}

bool CastExtensionsBrowserClient::AreExtensionsDisabled(
    const base::CommandLine& command_line,
    BrowserContext* context) {
  return false;
}

bool CastExtensionsBrowserClient::IsValidContext(BrowserContext* context) {
  return context == browser_context_;
}

bool CastExtensionsBrowserClient::IsSameContext(BrowserContext* first,
                                                BrowserContext* second) {
  return first == second;
}

bool CastExtensionsBrowserClient::HasOffTheRecordContext(
    BrowserContext* context) {
  return false;
}

BrowserContext* CastExtensionsBrowserClient::GetOffTheRecordContext(
    BrowserContext* context) {
  // cast_shell only supports a single context.
  return nullptr;
}

BrowserContext* CastExtensionsBrowserClient::GetOriginalContext(
    BrowserContext* context) {
  return context;
}

bool CastExtensionsBrowserClient::IsGuestSession(
    BrowserContext* context) const {
  return false;
}

bool CastExtensionsBrowserClient::IsExtensionIncognitoEnabled(
    const std::string& extension_id,
    content::BrowserContext* context) const {
  return false;
}

bool CastExtensionsBrowserClient::CanExtensionCrossIncognito(
    const Extension* extension,
    content::BrowserContext* context) const {
  return false;
}

base::FilePath CastExtensionsBrowserClient::GetBundleResourcePath(
    const network::ResourceRequest& request,
    const base::FilePath& extension_resources_path,
    int* resource_id) const {
  return base::FilePath();
}

void CastExtensionsBrowserClient::LoadResourceFromResourceBundle(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    const base::FilePath& resource_relative_path,
    int resource_id,
    scoped_refptr<net::HttpResponseHeaders> headers,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  NOTREACHED() << "Cannot load resource from bundle w/o path";
}

bool CastExtensionsBrowserClient::AllowCrossRendererResourceLoad(
    const network::ResourceRequest& request,
    network::mojom::RequestDestination destination,
    ui::PageTransition page_transition,
    int child_id,
    bool is_incognito,
    const Extension* extension,
    const ExtensionSet& extensions,
    const ProcessMap& process_map) {
  bool allowed = false;
  if (url_request_util::AllowCrossRendererResourceLoad(
          request, destination, page_transition, child_id, is_incognito,
          extension, extensions, process_map, &allowed)) {
    return allowed;
  }

  // Couldn't determine if resource is allowed. Block the load.
  return false;
}

PrefService* CastExtensionsBrowserClient::GetPrefServiceForContext(
    BrowserContext* context) {
  return pref_service_;
}

void CastExtensionsBrowserClient::GetEarlyExtensionPrefsObservers(
    content::BrowserContext* context,
    std::vector<EarlyExtensionPrefsObserver*>* observers) const {}

ProcessManagerDelegate* CastExtensionsBrowserClient::GetProcessManagerDelegate()
    const {
  return nullptr;
}

std::unique_ptr<ExtensionHostDelegate>
CastExtensionsBrowserClient::CreateExtensionHostDelegate() {
  return base::WrapUnique(new CastExtensionHostDelegate);
}

bool CastExtensionsBrowserClient::DidVersionUpdate(BrowserContext* context) {
  return false;
}

void CastExtensionsBrowserClient::PermitExternalProtocolHandler() {}

bool CastExtensionsBrowserClient::IsInDemoMode() {
  return false;
}

bool CastExtensionsBrowserClient::IsScreensaverInDemoMode(
    const std::string& app_id) {
  return false;
}

bool CastExtensionsBrowserClient::IsRunningInForcedAppMode() {
  return false;
}

bool CastExtensionsBrowserClient::IsAppModeForcedForApp(const ExtensionId& id) {
  return false;
}

bool CastExtensionsBrowserClient::IsLoggedInAsPublicAccount() {
  return false;
}

ExtensionSystemProvider*
CastExtensionsBrowserClient::GetExtensionSystemFactory() {
  return CastExtensionSystemFactory::GetInstance();
}

void CastExtensionsBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) const {
  PopulateExtensionFrameBinders(binder_map, render_frame_host, extension);
}

std::unique_ptr<RuntimeAPIDelegate>
CastExtensionsBrowserClient::CreateRuntimeAPIDelegate(
    content::BrowserContext* context) const {
  return nullptr;
}

const ComponentExtensionResourceManager*
CastExtensionsBrowserClient::GetComponentExtensionResourceManager() {
  return nullptr;
}

void CastExtensionsBrowserClient::BroadcastEventToRenderers(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> args,
    bool dispatch_to_off_the_record_profiles) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&CastExtensionsBrowserClient::BroadcastEventToRenderers,
                       base::Unretained(this), histogram_value, event_name,
                       std::move(args), dispatch_to_off_the_record_profiles));
    return;
  }
  // Currently ignoring the dispatch_to_off_the_record_profiles attribute
  // as it is not necessary at the time
  std::unique_ptr<Event> event(new Event(
      histogram_value, event_name, std::move(*args).TakeListDeprecated()));
  EventRouter::Get(browser_context_)->BroadcastEvent(std::move(event));
}

ExtensionCache* CastExtensionsBrowserClient::GetExtensionCache() {
  return nullptr;
}

bool CastExtensionsBrowserClient::IsBackgroundUpdateAllowed() {
  return true;
}

bool CastExtensionsBrowserClient::IsMinBrowserVersionSupported(
    const std::string& min_version) {
  return true;
}

void CastExtensionsBrowserClient::SetAPIClientForTest(
    ExtensionsAPIClient* api_client) {
  api_client_.reset(api_client);
}

ExtensionWebContentsObserver*
CastExtensionsBrowserClient::GetExtensionWebContentsObserver(
    content::WebContents* web_contents) {
  return CastExtensionWebContentsObserver::FromWebContents(web_contents);
}

KioskDelegate* CastExtensionsBrowserClient::GetKioskDelegate() {
  if (!kiosk_delegate_)
    kiosk_delegate_.reset(new CastKioskDelegate());
  return kiosk_delegate_.get();
}

bool CastExtensionsBrowserClient::IsLockScreenContext(
    content::BrowserContext* context) {
  return false;
}

std::string CastExtensionsBrowserClient::GetApplicationLocale() {
  // TODO(b/70902491): Use system locale.
  return "en-US";
}

std::string CastExtensionsBrowserClient::GetUserAgent() const {
  return content::BuildUserAgentFromProduct(
      version_info::GetProductNameAndVersionForUserAgent());
}

}  // namespace extensions
