// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/cast_extensions_browser_client.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chromecast/browser/extensions/cast_extension_host_delegate.h"
#include "chromecast/browser/extensions/cast_extension_system_factory.h"
#include "chromecast/browser/extensions/cast_extension_web_contents_observer.h"
#include "chromecast/browser/extensions/cast_extensions_api_client.h"
#include "chromecast/browser/extensions/cast_extensions_browser_api_provider.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_request_info.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/runtime/runtime_api_delegate.h"
#include "extensions/browser/core_extensions_browser_api_provider.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/mojo/interface_registration.h"
#include "extensions/browser/null_app_sorting.h"
#include "extensions/browser/updater/null_extension_cache.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/common/features/feature_channel.h"

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

CastExtensionsBrowserClient::CastExtensionsBrowserClient(
    BrowserContext* context,
    PrefService* pref_service)
    : browser_context_(context),
      pref_service_(pref_service),
      api_client_(new CastExtensionsAPIClient) {
  // Set to UNKNOWN to enable all APIs.
  // TODO(achaulk): figure out what channel to use here.
  SetCurrentChannel(version_info::Channel::UNKNOWN);

  AddAPIProvider(std::make_unique<CoreExtensionsBrowserAPIProvider>());
  AddAPIProvider(std::make_unique<CastExtensionsBrowserAPIProvider>());
}

CastExtensionsBrowserClient::~CastExtensionsBrowserClient() {}

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

net::URLRequestJob*
CastExtensionsBrowserClient::MaybeCreateResourceBundleRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const base::FilePath& directory_path,
    const std::string& content_security_policy,
    bool send_cors_header) {
  return nullptr;
}

base::FilePath CastExtensionsBrowserClient::GetBundleResourcePath(
    const network::ResourceRequest& request,
    const base::FilePath& extension_resources_path,
    int* resource_id) const {
  return base::FilePath();
}

void CastExtensionsBrowserClient::LoadResourceFromResourceBundle(
    const network::ResourceRequest& request,
    network::mojom::URLLoaderRequest loader,
    const base::FilePath& resource_relative_path,
    int resource_id,
    const std::string& content_security_policy,
    network::mojom::URLLoaderClientPtr client,
    bool send_cors_header) {
  NOTREACHED() << "Cannot load resource from bundle w/o path";
}

bool CastExtensionsBrowserClient::AllowCrossRendererResourceLoad(
    const GURL& url,
    content::ResourceType resource_type,
    ui::PageTransition page_transition,
    int child_id,
    bool is_incognito,
    const Extension* extension,
    const ExtensionSet& extensions,
    const ProcessMap& process_map) {
  bool allowed = false;
  if (url_request_util::AllowCrossRendererResourceLoad(
          url, resource_type, page_transition, child_id, is_incognito,
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
    std::vector<ExtensionPrefsObserver*>* observers) const {}

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

void CastExtensionsBrowserClient::RegisterExtensionInterfaces(
    service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>*
        registry,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) const {
  RegisterInterfacesForExtension(registry, render_frame_host, extension);
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
    std::unique_ptr<base::ListValue> args) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&CastExtensionsBrowserClient::BroadcastEventToRenderers,
                       base::Unretained(this), histogram_value, event_name,
                       std::move(args)));
    return;
  }

  std::unique_ptr<Event> event(
      new Event(histogram_value, event_name, std::move(args)));
  EventRouter::Get(browser_context_)->BroadcastEvent(std::move(event));
}

net::NetLog* CastExtensionsBrowserClient::GetNetLog() {
  return nullptr;
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

ExtensionNavigationUIData*
CastExtensionsBrowserClient::GetExtensionNavigationUIData(
    net::URLRequest* request) {
  return nullptr;
}

KioskDelegate* CastExtensionsBrowserClient::GetKioskDelegate() {
  return nullptr;
}

bool CastExtensionsBrowserClient::IsLockScreenContext(
    content::BrowserContext* context) {
  return false;
}

std::string CastExtensionsBrowserClient::GetApplicationLocale() {
  // TODO(b/70902491): Use system locale.
  return "en-US";
}

}  // namespace extensions
