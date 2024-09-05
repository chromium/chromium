// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_contents_impl.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_navigation_ui_data.h"
#include "chromecast/browser/cast_permission_user_data.h"
#include "chromecast/browser/cast_session_id_map.h"
#include "chromecast/browser/devtools/remote_debugging_server.h"
#include "chromecast/common/mojom/activity_url_filter.mojom.h"
#include "chromecast/common/mojom/queryable_data_store.mojom.h"
#include "chromecast/common/queryable_data.h"
#include "chromecast/net/connectivity_checker.h"
#include "components/cast/message_port/blink_message_port_adapter.h"
#include "components/cast/message_port/cast/message_port_cast.h"
#include "components/media_control/browser/media_blocker.h"
#include "components/media_control/mojom/media_playback_options.mojom.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/bindings_policy.h"
#include "media/mojo/buildflags.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace chromecast {

namespace {

// IDs start at 1, since 0 is reserved for the root content window.
size_t next_tab_id = 1;

// Next id for id()
size_t next_id = 0;

// Remove the given CastWebContents pointer from the global instance vector.
void RemoveCastWebContents(CastWebContents* instance) {
  auto& all_cast_web_contents = CastWebContents::GetAll();
  auto it = base::ranges::find(all_cast_web_contents, instance);
  if (it != all_cast_web_contents.end()) {
    all_cast_web_contents.erase(it);
  }
}

content::mojom::RendererType ToContentRendererType(mojom::RendererType type) {
  switch (type) {
    case mojom::RendererType::DEFAULT_RENDERER:
      return content::mojom::RendererType::DEFAULT_RENDERER;
    case mojom::RendererType::MOJO_RENDERER:
      return content::mojom::RendererType::MOJO_RENDERER;
    case mojom::RendererType::REMOTING_RENDERER:
      return content::mojom::RendererType::REMOTING_RENDERER;
  }
}

}  // namespace

// static
std::vector<CastWebContents*>& CastWebContents::GetAll() {
  static base::NoDestructor<std::vector<CastWebContents*>> instance;
  return *instance;
}

// static
CastWebContents* CastWebContents::FromWebContents(
    content::WebContents* web_contents) {
  auto& all_cast_web_contents = CastWebContents::GetAll();
  auto it = base::ranges::find(all_cast_web_contents, web_contents,
                               &CastWebContents::web_contents);
  if (it == all_cast_web_contents.end()) {
    return nullptr;
  }
  return *it;
}

void CastWebContentsImpl::RenderProcessReady(content::RenderProcessHost* host) {
  DCHECK(host->IsReady());
  const base::Process& process = host->GetProcess();
  for (auto& observer : observers_) {
    observer->OnRenderProcessReady(process.Pid());
  }
}

void CastWebContentsImpl::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  RemoveRenderProcessHostObserver();
}

void CastWebContentsImpl::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  RemoveRenderProcessHostObserver();
}

void CastWebContentsImpl::RemoveRenderProcessHostObserver() {
  if (main_process_host_) {
    main_process_host_->RemoveObserver(this);
  }
  main_process_host_ = nullptr;
}

CastWebContentsImpl::CastWebContentsImpl(content::WebContents* web_contents,
                                         mojom::CastWebViewParamsPtr params)
    : CastWebContentsImpl(web_contents,
                          std::move(params),
                          nullptr /* parent */) {}

CastWebContentsImpl::CastWebContentsImpl(content::WebContents* web_contents,
                                         mojom::CastWebViewParamsPtr params,
                                         CastWebContents* parent)
    : web_contents_(web_contents),
      params_(std::move(params)),
      page_state_(PageState::IDLE),
      last_state_(PageState::IDLE),
      remote_debugging_server_(
          shell::CastBrowserProcess::GetInstance()->remote_debugging_server()),
      media_blocker_(params_->use_media_blocker
                         ? std::make_unique<CastMediaBlocker>(web_contents_)
                         : nullptr),
      main_process_host_(nullptr),
      parent_cast_web_contents_(parent),
      tab_id_(params_->is_root_window ? 0 : next_tab_id++),
      id_(next_id++),
      main_frame_loaded_(false),
      closing_(false),
      stopped_(false),
      stop_notified_(false),
      notifying_(false),
      last_error_(net::OK),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      weak_factory_(this) {
  DCHECK(web_contents_);
  DCHECK(web_contents_->GetController().IsInitialNavigation());
  DCHECK(!web_contents_->IsLoading());
  DCHECK(web_contents_->GetPrimaryMainFrame());

  main_process_host_ = web_contents_->GetPrimaryMainFrame()->GetProcess();
  DCHECK(main_process_host_);
  main_process_host_->AddObserver(this);

  CastWebContents::GetAll().push_back(this);
  content::WebContentsObserver::Observe(web_contents_);

  if (params_->enabled_for_dev) {
    LOG(INFO) << "Enabling dev console for CastWebContentsImpl";
    remote_debugging_server_->EnableWebContentsForDebugging(web_contents_);
  }

  // TODO(yucliu): Change the flag name to kDisableCmaRenderer in a latter diff.
  if (GetSwitchValueBoolean(switches::kDisableMojoRenderer, false) &&
      params_->renderer_type == mojom::RendererType::MOJO_RENDERER) {
    params_->renderer_type = mojom::RendererType::DEFAULT_RENDERER;
  } else if (GetSwitchValueBoolean(switches::kForceMojoRenderer, false)) {
#if BUILDFLAG(ENABLE_MOJO_RENDERER) && BUILDFLAG(ENABLE_CAST_RENDERER)
    LOG(INFO) << "Enabling mojo renderer";
#else
    LOG(ERROR)
        << "The switch " << switches::kForceMojoRenderer
        << " was used, but either the mojo renderer or cast renderer is "
           "disabled via GN args. Check the values of enable_cast_renderer and "
           "mojo_media_services in your GN args";
#endif  // BUILDFLAG(ENABLE_MOJO_RENDERER) && BUILDFLAG(ENABLE_CAST_RENDERER)
    params_->renderer_type = mojom::RendererType::MOJO_RENDERER;
  }

  web_contents_->SetPageBaseBackgroundColor(chromecast::GetSwitchValueColor(
      switches::kCastAppBackgroundColor, SK_ColorBLACK));

  if (params_->enable_webui_bindings_permission) {
    web_contents_->GetPrimaryMainFrame()->AllowBindings(
        content::kWebUIBindingsPolicySet);
  }
}

CastWebContentsImpl::~CastWebContentsImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!notifying_) << "Do not destroy CastWebContents during observer "
                         "notification!";
  RemoveRenderProcessHostObserver();
  DisableDebugging();
  observers_.Clear();
  for (auto& observer : sync_observers_) {
    observer.ResetCastWebContents();
  }
  RemoveCastWebContents(this);
}

int CastWebContentsImpl::tab_id() const {
  return tab_id_;
}

int CastWebContentsImpl::id() const {
  return id_;
}

content::WebContents* CastWebContentsImpl::web_contents() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return web_contents_;
}

PageState CastWebContentsImpl::page_state() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return page_state_;
}

const media_control::MediaBlocker* CastWebContentsImpl::media_blocker() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return media_blocker_.get();
}

void CastWebContentsImpl::AddRendererFeatures(base::Value::Dict features) {
  renderer_features_ = std::move(features);
}

void CastWebContentsImpl::SetInterfacesForRenderer(
    mojo::PendingRemote<mojom::RemoteInterfaces> remote_interfaces) {
  remote_interfaces_.SetProvider(std::move(remote_interfaces));
}

void CastWebContentsImpl::LoadUrl(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (api_bindings_ && !bindings_received_) {
    LOG(INFO) << "Will load URL: " << url.possibly_invalid_spec()
              << " once bindings has been received.";
    pending_load_url_ = url;
    return;
  }

  if (!web_contents_) {
    LOG(ERROR) << "Cannot load URL for deleted WebContents";
    return;
  }
  if (closing_) {
    LOG(ERROR) << "Cannot load URL for WebContents while closing";
    return;
  }
  OnPageLoading();
  LOG(INFO) << "Load url: " << url.possibly_invalid_spec();
  web_contents_->GetController().LoadURL(url, content::Referrer(),
                                         ui::PAGE_TRANSITION_TYPED, "");
  UpdatePageState();
  DCHECK_EQ(PageState::LOADING, page_state_);
  NotifyPageState();
}

void CastWebContentsImpl::ClosePage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!web_contents_ || closing_) {
    return;
  }
  closing_ = true;
  web_contents_->DispatchBeforeUnload(false /* auto_cancel */);
  web_contents_->ClosePage();
  // If the WebContents doesn't close within the specified timeout, then signal
  // the page closure anyway so that the Delegate can delete the WebContents and
  // stop the page itself.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CastWebContentsImpl::OnClosePageTimeout,
                     weak_factory_.GetWeakPtr()),
      base::Milliseconds(1000));
}

void CastWebContentsImpl::Stop(int error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stopped_) {
    UpdatePageState();
    NotifyPageState();
    return;
  }
  last_error_ = error_code;
  closing_ = false;
  stopped_ = true;
  UpdatePageState();
  DCHECK_NE(PageState::IDLE, page_state_);
  DCHECK_NE(PageState::LOADING, page_state_);
  DCHECK_NE(PageState::LOADED, page_state_);
  NotifyPageState();
}

void CastWebContentsImpl::SetWebVisibilityAndPaint(bool visible) {
  if (!web_contents_) {
    return;
  }
  if (visible) {
    web_contents_->WasShown();
  } else {
    web_contents_->WasHidden();
  }
  if (web_contents_->GetVisibility() != content::Visibility::VISIBLE) {
    // Since we are managing the visibility, we need to ensure pages are
    // unfrozen in the event this occurred while in the background.
    web_contents_->SetPageFrozen(false);
  }
}

void CastWebContentsImpl::BlockMediaLoading(bool blocked) {
  if (media_blocker_) {
    media_blocker_->BlockMediaLoading(blocked);
  }
}

void CastWebContentsImpl::BlockMediaStarting(bool blocked) {
  if (media_blocker_) {
    media_blocker_->BlockMediaStarting(blocked);
  }
}

void CastWebContentsImpl::EnableBackgroundVideoPlayback(bool enabled) {
  if (media_blocker_) {
    media_blocker_->EnableBackgroundVideoPlayback(enabled);
  }
}

void CastWebContentsImpl::SetAppProperties(
    const std::string& app_id,
    const std::string& session_id,
    bool is_audio_app,
    const GURL& app_web_url,
    bool enforce_feature_permissions,
    const std::vector<int32_t>& feature_permissions,
    const std::vector<std::string>& additional_feature_permission_origins) {
  if (!web_contents_) {
    return;
  }
  shell::CastNavigationUIData::SetAppPropertiesForWebContents(
      web_contents_, session_id, is_audio_app);
  new shell::CastPermissionUserData(
      web_contents_, app_id, app_web_url, enforce_feature_permissions,
      feature_permissions, additional_feature_permission_origins);
}

void CastWebContentsImpl::SetGroupInfo(const std::string& session_id,
                                       bool is_multizone_launch) {
  shell::CastSessionIdMap::GetInstance()->SetGroupInfo(session_id,
                                                       is_multizone_launch);
}

void CastWebContentsImpl::AddBeforeLoadJavaScript(uint64_t id,
                                                  std::string_view script) {
  script_injector_.AddScriptForAllOrigins(id, std::string(script));
}

void CastWebContentsImpl::PostMessageToMainFrame(
    const std::string& target_origin,
    const std::string& data,
    std::vector<blink::WebMessagePort> ports) {
  DCHECK(!data.empty());

  std::u16string data_utf16;
  data_utf16 = base::UTF8ToUTF16(data);

  // If origin is set as wildcard, no origin scoping would be applied.
  std::optional<std::u16string> target_origin_utf16;
  constexpr char kWildcardOrigin[] = "*";
  if (target_origin != kWildcardOrigin) {
    target_origin_utf16 = base::UTF8ToUTF16(target_origin);
  }

  content::MessagePortProvider::PostMessageToFrame(
      web_contents()->GetPrimaryPage(), std::u16string(), target_origin_utf16,
      data_utf16, std::move(ports));
}

void CastWebContentsImpl::ExecuteJavaScript(
    const std::u16string& javascript,
    base::OnceCallback<void(base::Value)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!web_contents_ || closing_ || !main_frame_loaded_ ||
      !web_contents_->GetPrimaryMainFrame()) {
    return;
  }

  web_contents_->GetPrimaryMainFrame()->ExecuteJavaScript(javascript,
                                                          std::move(callback));
}

void CastWebContentsImpl::ConnectToBindingsService(
    mojo::PendingRemote<mojom::ApiBindings> api_bindings_remote) {
  DCHECK(api_bindings_remote);

  bindings_received_ = false;

  named_message_port_connector_ =
      std::make_unique<NamedMessagePortConnectorCast>(this);
  named_message_port_connector_->RegisterPortHandler(base::BindRepeating(
      &CastWebContentsImpl::OnPortConnected, base::Unretained(this)));

  api_bindings_.Bind(std::move(api_bindings_remote));
  // Fetch bindings and inject scripts into |script_injector_|.
  api_bindings_->GetAll(base::BindOnce(&CastWebContentsImpl::OnBindingsReceived,
                                       base::Unretained(this)));
}

void CastWebContentsImpl::SetEnabledForRemoteDebugging(bool enabled) {
  DCHECK(remote_debugging_server_);

  if (enabled && !params_->enabled_for_dev) {
    LOG(INFO) << "Enabling dev console for CastWebContentsImpl";
    remote_debugging_server_->EnableWebContentsForDebugging(web_contents_);
  } else if (!enabled && params_->enabled_for_dev) {
    LOG(INFO) << "Disabling dev console for CastWebContentsImpl";
    remote_debugging_server_->DisableWebContentsForDebugging(web_contents_);
  }
  params_->enabled_for_dev = enabled;

  // Propagate setting change to inner contents.
  for (auto& inner : inner_contents_) {
    inner->SetEnabledForRemoteDebugging(enabled);
  }
}

void CastWebContentsImpl::GetMainFramePid(GetMainFramePidCallback cb) {
  if (!web_contents_ || !web_contents_->GetPrimaryMainFrame()) {
    std::move(cb).Run(base::kNullProcessHandle);
    return;
  }

  auto* rph = web_contents_->GetPrimaryMainFrame()->GetProcess();
  if (!rph || rph->GetProcess().Handle() == base::kNullProcessHandle) {
    std::move(cb).Run(base::kNullProcessHandle);
    return;
  }
  std::move(cb).Run(rph->GetProcess().Handle());
}

bool CastWebContentsImpl::TryBindReceiver(
    mojo::GenericPendingReceiver& receiver) {
  // First try binding local interfaces.
  if (local_interfaces_.TryBindReceiver(receiver)) {
    return true;
  }

  const std::string interface_name = *receiver.interface_name();
  mojo::ScopedMessagePipeHandle interface_pipe = receiver.PassPipe();

  receiver =
      mojo::GenericPendingReceiver(interface_name, std::move(interface_pipe));
  remote_interfaces_.Bind(std::move(receiver));

  // The PendingReceiver has been passed along at this point, so return true.
  // Note that this doesn't guarantee that the interface will eventually be
  // connected.
  return true;
}

InterfaceBundle* CastWebContentsImpl::local_interfaces() {
  return &local_interfaces_;
}

bool CastWebContentsImpl::is_websql_enabled() {
  return params_->enable_websql;
}

bool CastWebContentsImpl::is_mixer_audio_enabled() {
  return params_->enable_mixer_audio;
}

void CastWebContentsImpl::OnClosePageTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!closing_ || stopped_) {
    return;
  }
  closing_ = false;
  Stop(net::OK);
}

void CastWebContentsImpl::RenderFrameCreated(
    content::RenderFrameHost* frame_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_host);

  // TODO(b/187758538): Merge the two ConfigureFeatures() calls.
  mojo::Remote<chromecast::shell::mojom::FeatureManager> feature_manager_remote;
  frame_host->GetRemoteInterfaces()->GetInterface(
      feature_manager_remote.BindNewPipeAndPassReceiver());
  feature_manager_remote->ConfigureFeatures(GetRendererFeatures());
  mojo::AssociatedRemote<chromecast::shell::mojom::FeatureManager>
      feature_manager_associated_remote;
  frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &feature_manager_associated_remote);
  feature_manager_associated_remote->ConfigureFeatures(GetRendererFeatures());

  mojo::AssociatedRemote<components::media_control::mojom::MediaPlaybackOptions>
      media_playback_options;
  frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &media_playback_options);
  media_playback_options->SetRendererType(
      ToContentRendererType(params_->renderer_type));

  // Send queryable values
  mojo::Remote<chromecast::shell::mojom::QueryableDataStore>
      queryable_data_store_remote;
  frame_host->GetRemoteInterfaces()->GetInterface(
      queryable_data_store_remote.BindNewPipeAndPassReceiver());
  for (const auto& value : QueryableData::GetValues()) {
    // base::Value is not copyable.
    queryable_data_store_remote->Set(value.first, value.second.Clone());
  }

  // Set up URL filter
  if (params_->url_filters) {
    mojo::AssociatedRemote<chromecast::mojom::ActivityUrlFilterConfiguration>
        activity_filter_setter;
    frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
        &activity_filter_setter);
    activity_filter_setter->SetFilter(
        chromecast::mojom::ActivityUrlFilterCriteria::New(
            params_->url_filters.value()));
  }

  // Set the background color for main frames.
  if (!frame_host->GetParent()) {
    if (params_->background_color == mojom::BackgroundColor::WHITE) {
      frame_host->GetView()->SetBackgroundColor(SK_ColorWHITE);
    } else if (params_->background_color == mojom::BackgroundColor::BLACK) {
      frame_host->GetView()->SetBackgroundColor(SK_ColorBLACK);
    } else if (params_->background_color ==
               mojom::BackgroundColor::TRANSPARENT) {
      frame_host->GetView()->SetBackgroundColor(SK_ColorTRANSPARENT);
    } else {
      frame_host->GetView()->SetBackgroundColor(chromecast::GetSwitchValueColor(
          switches::kCastAppBackgroundColor, SK_ColorBLACK));
    }
  }
}

std::vector<chromecast::shell::mojom::FeaturePtr>
CastWebContentsImpl::GetRendererFeatures() {
  std::vector<chromecast::shell::mojom::FeaturePtr> features;
  for (const auto pair : renderer_features_) {
    const std::string& name = pair.first;
    const base::Value& config_value = pair.second;
    const base::Value::Dict* maybe_config_dict = config_value.GetIfDict();

    // There are only 2 callers of `AddRendererFeatures` (both in
    // `runtime_application_service_impl.cc`) and they always provide
    // well-formed dictionaries as values.
    DCHECK(maybe_config_dict);
    base::Value::Dict config_dict = maybe_config_dict->Clone();

    features.push_back(
        chromecast::shell::mojom::Feature::New(name, std::move(config_dict)));
  }
  return features;
}

void CastWebContentsImpl::OnBindingsReceived(
    std::vector<chromecast::mojom::ApiBindingPtr> bindings) {
  bindings_received_ = true;

  if (bindings.empty()) {
    LOG(ERROR) << "ApiBindings remote sent empty bindings. Stopping the page.";
    Stop(net::ERR_UNEXPECTED);
  } else {
    constexpr uint64_t kBindingsIdStart = 0xFF0000;

    // Enumerate and inject all scripts in |bindings|.
    uint64_t bindings_id = kBindingsIdStart;
    for (auto& entry : bindings) {
      AddBeforeLoadJavaScript(bindings_id++, entry->script);
    }
  }

  DVLOG(1) << "Bindings has been received. Start loading URL if requested.";
  if (!pending_load_url_.is_empty()) {
    auto gurl = std::move(pending_load_url_);
    pending_load_url_ = GURL();
    LoadUrl(gurl);
  }
}

bool CastWebContentsImpl::OnPortConnected(
    std::string_view port_name,
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  DCHECK(api_bindings_);

  api_bindings_->Connect(
      std::string(port_name),
      cast_api_bindings::BlinkMessagePortAdapter::FromServerPlatformMessagePort(
          std::move(port))
          .PassPort());
  return true;
}

void CastWebContentsImpl::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Render process for main frame exited unexpectedly.";
  Stop(net::ERR_UNEXPECTED);
}

void CastWebContentsImpl::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(navigation_handle);
  if (!web_contents_ || closing_ || stopped_) {
    return;
  }

  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Main frame has an ongoing navigation. This might overwrite a
  // previously active navigation. We only care about tracking
  // the most recent main frame navigation.
  active_navigation_ = navigation_handle;

  // Main frame has begun navigating/loading.
  LOG(INFO) << "Navigation started: " << navigation_handle->GetURL();
  OnPageLoading();
  start_loading_ticks_ = base::TimeTicks::Now();
  GURL loading_url;
  content::NavigationEntry* nav_entry =
      web_contents()->GetController().GetVisibleEntry();
  if (nav_entry) {
    loading_url = nav_entry->GetVirtualURL();
  }
  TracePageLoadBegin(loading_url);
  UpdatePageState();
  DCHECK_EQ(page_state_, PageState::LOADING);
  NotifyPageState();
}

void CastWebContentsImpl::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(navigation_handle);
  if (!web_contents_ || closing_ || stopped_) {
    return;
  }
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }
  // Main frame navigation was redirected by the server.
  LOG(INFO) << "Navigation was redirected by server: "
            << navigation_handle->GetURL();
}

void CastWebContentsImpl::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle);
  if (!web_contents_ || closing_ || stopped_) {
    return;
  }

  // We want to honor the autoplay feature policy (via allow="autoplay") without
  // explicit user activation, since media on Cast is extremely likely to have
  // already been explicitly requested by a user via voice or over the network.
  // By spoofing the "high media engagement" signal, we can bypass the user
  // gesture requirement for autoplay.
  int32_t autoplay_flags = blink::mojom::kAutoplayFlagHighMediaEngagement;

  // Main frames should have autoplay enabled by default, since autoplay
  // delegation via parent frame doesn't work here.
  if (navigation_handle->IsInMainFrame()) {
    autoplay_flags |= blink::mojom::kAutoplayFlagForceAllow;
  }

  mojo::AssociatedRemote<blink::mojom::AutoplayConfigurationClient> client;
  navigation_handle->GetRenderFrameHost()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&client);
  auto autoplay_origin = url::Origin::Create(navigation_handle->GetURL());
  client->AddAutoplayFlags(autoplay_origin, autoplay_flags);

  // Skip injecting bindings scripts if |navigation_handle| is not
  // 'current' main frame navigation, e.g. another DidStartNavigation is
  // emitted. Also skip injecting for same document navigation and error page.
  if (navigation_handle == active_navigation_ &&
      !navigation_handle->IsErrorPage()) {
    // Injects registered bindings script into the main frame.
    script_injector_.InjectScriptsForURL(
        navigation_handle->GetURL(), navigation_handle->GetRenderFrameHost());
  }

  // Always allow mixed content (https and http in the same page) for cast.
  // TODO(https://crbug.com/333795270): Decide whether to use
  // kKeyAllowInsecureContent to configure allowing mixed content.
  auto content_settings = blink::CreateDefaultRendererContentSettings();
  content_settings->allow_mixed_content = true;
  navigation_handle->SetContentSettings(std::move(content_settings));

  // Notifies observers that the navigation of the main frame is ready.
  for (Observer& observer : sync_observers_) {
    observer.MainFrameReadyToCommitNavigation(navigation_handle);
  }
}

void CastWebContentsImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ignore sub-frame and non-current main frame navigation.
  if (navigation_handle != active_navigation_) {
    return;
  }
  active_navigation_ = nullptr;

  // If the navigation was not committed, it means either the page was a
  // download or error 204/205, or the navigation never left the previous
  // URL. Ignore these navigations.
  if (!navigation_handle->HasCommitted()) {
    LOG(WARNING) << "Navigation did not commit: url="
                 << navigation_handle->GetURL();
    return;
  }

  if (navigation_handle->IsErrorPage()) {
    const net::Error error_code = navigation_handle->GetNetErrorCode();
    LOG(ERROR) << "Got error on navigation: url=" << navigation_handle->GetURL()
               << ", error_code=" << error_code
               << ", description=" << net::ErrorToShortString(error_code);

    Stop(error_code);
    DCHECK_EQ(page_state_, PageState::ERROR);
  }

  // Notifies observers that the navigation of the main frame has finished
  // with no errors.
  for (auto& observer : observers_) {
    observer->MainFrameFinishedNavigation();
  }
}

void CastWebContentsImpl::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (page_state_ != PageState::LOADING || !web_contents_ ||
      render_frame_host != web_contents_->GetPrimaryMainFrame()) {
    return;
  }

  // Don't process load completion on the current document if the WebContents
  // is already in the process of navigating to a different page.
  if (active_navigation_) {
    return;
  }

  // The main frame finished loading. Before proceeding, we need to verify that
  // the loaded page is the one that was requested.
  TracePageLoadEnd(validated_url);
  int http_status_code = 0;
  content::NavigationEntry* nav_entry =
      web_contents()->GetController().GetVisibleEntry();
  if (nav_entry) {
    http_status_code = nav_entry->GetHttpStatusCode();
  }

  if (http_status_code != 0 && http_status_code / 100 != 2) {
    // An error HTML page was loaded instead of the content we requested.
    LOG(ERROR) << "Failed loading page for: " << validated_url
               << "; http status code: " << http_status_code;
    Stop(net::ERR_HTTP_RESPONSE_CODE_FAILURE);
    DCHECK_EQ(page_state_, PageState::ERROR);
    return;
  }
  // Main frame finished loading properly.
  base::TimeDelta load_time = base::TimeTicks::Now() - start_loading_ticks_;
  LOG(INFO) << "Finished loading page after " << load_time.InMilliseconds()
            << " ms, url=" << validated_url;
  OnPageLoaded();
}

void CastWebContentsImpl::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only report an error if we are the main frame.  See b/8433611.
  if (render_frame_host->GetParent()) {
    LOG(ERROR) << "Got error on sub-iframe: url=" << validated_url.spec()
               << ", error=" << error_code;
    return;
  }
  if (error_code == net::ERR_ABORTED) {
    // ERR_ABORTED means download was aborted by the app, typically this happens
    // when flinging URL for direct playback, the initial URLRequest gets
    // cancelled/aborted and then the same URL is requested via the buffered
    // data source for media::Pipeline playback.
    LOG(INFO) << "Load canceled: url=" << validated_url.spec();

    // We consider the page to be fully loaded in this case, since the app has
    // intentionally entered this state. If the app wanted to stop, it would
    // have called window.close() instead.
    OnPageLoaded();
    return;
  }

  LOG(ERROR) << "Got error on load: url=" << validated_url.spec()
             << ", error_code=" << error_code;

  TracePageLoadEnd(validated_url);
  Stop(error_code);
  DCHECK_EQ(PageState::ERROR, page_state_);
}

void CastWebContentsImpl::OnPageLoading() {
  closing_ = false;
  stopped_ = false;
  stop_notified_ = false;
  main_frame_loaded_ = false;
  last_error_ = net::OK;
}

void CastWebContentsImpl::OnPageLoaded() {
  main_frame_loaded_ = true;
  UpdatePageState();
  DCHECK(page_state_ == PageState::LOADED);
  NotifyPageState();
}

void CastWebContentsImpl::UpdatePageState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_state_ = page_state_;
  if (!web_contents_) {
    DCHECK(stopped_);
    page_state_ = PageState::DESTROYED;
  } else if (!stopped_) {
    if (main_frame_loaded_) {
      page_state_ = PageState::LOADED;
    } else {
      page_state_ = PageState::LOADING;
    }
  } else if (stopped_) {
    if (last_error_ != net::OK) {
      page_state_ = PageState::ERROR;
    } else {
      page_state_ = PageState::CLOSED;
    }
  }
}

void CastWebContentsImpl::NotifyPageState() {
  // Don't notify if the page state didn't change.
  if (last_state_ == page_state_) {
    return;
  }
  // Don't recursively notify the observers.
  if (notifying_) {
    return;
  }
  notifying_ = true;
  if (stopped_ && !stop_notified_) {
    stop_notified_ = true;
    for (auto& observer : observers_) {
      observer->PageStopped(page_state_, last_error_);
    }
    // Notifies the local observers.
    for (Observer& observer : sync_observers_) {
      observer.PageStopped(page_state_, last_error_);
    }
  } else {
    for (auto& observer : observers_) {
      observer->PageStateChanged(page_state_);
    }
    // Notifies the local observers.
    for (Observer& observer : sync_observers_) {
      observer.PageStateChanged(page_state_);
    }
  }
  notifying_ = false;
}

void CastWebContentsImpl::ResourceLoadComplete(
    content::RenderFrameHost* render_frame_host,
    const content::GlobalRequestID& request_id,
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  if (!web_contents_ ||
      render_frame_host != web_contents_->GetPrimaryMainFrame()) {
    return;
  }
  int net_error = resource_load_info.net_error;
  if (net_error == net::OK) {
    return;
  }
  metrics::CastMetricsHelper* metrics_helper =
      metrics::CastMetricsHelper::GetInstance();
  metrics_helper->RecordApplicationEventWithValue(
      "Cast.Platform.ResourceRequestError", net_error);
  LOG(ERROR) << "Resource \"" << resource_load_info.original_url << "\""
             << " failed to load with net_error=" << net_error
             << ", description=" << net::ErrorToShortString(net_error);
  shell::CastBrowserProcess::GetInstance()->connectivity_checker()->Check();
  for (auto& observer : observers_) {
    observer->ResourceLoadFailed();
  }
}

void CastWebContentsImpl::InnerWebContentsCreated(
    content::WebContents* inner_web_contents) {
  if (!params_->handle_inner_contents) {
    return;
  }

  mojom::CastWebViewParamsPtr params = mojom::CastWebViewParams::New();
  params->enabled_for_dev = params_->enabled_for_dev;
  params->background_color = params_->background_color;
  auto result = inner_contents_.insert(std::unique_ptr<CastWebContentsImpl>(
      new CastWebContentsImpl(inner_web_contents, std::move(params), this)));

  // Notifies remote observers.
  for (auto& observer : observers_) {
    mojo::PendingRemote<mojom::CastWebContents> pending_remote;
    result.first->get()->BindSharedReceiver(
        pending_remote.InitWithNewPipeAndPassReceiver());
    observer->InnerContentsCreated(std::move(pending_remote));
  }

  // Notifies the local observers.
  for (Observer& observer : sync_observers_) {
    observer.InnerContentsCreated(result.first->get(), this);
  }
}

void CastWebContentsImpl::TitleWasSet(content::NavigationEntry* entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!entry) {
    return;
  }
  for (auto& observer : observers_) {
    observer->UpdateTitle(base::UTF16ToUTF8(entry->GetTitle()));
  }
}

void CastWebContentsImpl::DidFirstVisuallyNonEmptyPaint() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metrics::CastMetricsHelper::GetInstance()->LogTimeToFirstPaint();

  for (auto& observer : observers_) {
    observer->DidFirstVisuallyNonEmptyPaint();
  }
}

void CastWebContentsImpl::WebContentsDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  closing_ = false;
  DisableDebugging();
  media_blocker_.reset();
  content::WebContentsObserver::Observe(nullptr);
  web_contents_ = nullptr;
  Stop(net::OK);
  RemoveCastWebContents(this);
  DCHECK_EQ(PageState::DESTROYED, page_state_);
}

void CastWebContentsImpl::DidUpdateFaviconURL(
    content::RenderFrameHost* render_frame_host,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (candidates.empty()) {
    return;
  }
  GURL icon_url;
  bool found_touch_icon = false;
  // icon search order:
  //  1) apple-touch-icon-precomposed
  //  2) apple-touch-icon
  //  3) icon
  for (auto& favicon : candidates) {
    if (favicon->icon_type ==
        blink::mojom::FaviconIconType::kTouchPrecomposedIcon) {
      icon_url = favicon->icon_url;
      break;
    } else if ((favicon->icon_type ==
                blink::mojom::FaviconIconType::kTouchIcon) &&
               !found_touch_icon) {
      found_touch_icon = true;
      icon_url = favicon->icon_url;
    } else if (!found_touch_icon) {
      icon_url = favicon->icon_url;
    }
  }

  for (auto& observer : observers_) {
    observer->UpdateFaviconURL(icon_url);
  }
}

void CastWebContentsImpl::MediaStartedPlaying(
    const MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metrics::CastMetricsHelper::GetInstance()->LogMediaPlay();
  for (auto& observer : observers_) {
    observer->MediaPlaybackChanged(true /* media_playing */);
  }

  // Notifies the local observers.
  for (Observer& observer : sync_observers_) {
    observer.MediaPlaybackChanged(true /* media_playing */);
  }
}

void CastWebContentsImpl::MediaStoppedPlaying(
    const MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id,
    content::WebContentsObserver::MediaStoppedReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metrics::CastMetricsHelper::GetInstance()->LogMediaPause();
  for (auto& observer : observers_) {
    observer->MediaPlaybackChanged(false /* media_playing */);
  }

  // Notifies the local observers.
  for (Observer& observer : sync_observers_) {
    observer.MediaPlaybackChanged(false /* media_playing */);
  }
}

void CastWebContentsImpl::TracePageLoadBegin(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "browser,navigation", "CastWebContentsImpl Launch", TRACE_ID_LOCAL(this),
      "URL", url.possibly_invalid_spec());
}

void CastWebContentsImpl::TracePageLoadEnd(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "browser,navigation", "CastWebContentsImpl Launch", TRACE_ID_LOCAL(this),
      "URL", url.possibly_invalid_spec());
}

void CastWebContentsImpl::DisableDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!params_->enabled_for_dev || !web_contents_) {
    return;
  }
  LOG(INFO) << "Disabling dev console for CastWebContentsImpl";
  remote_debugging_server_->DisableWebContentsForDebugging(web_contents_);
}

}  // namespace chromecast
