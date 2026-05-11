// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/slim_web_view/slim_web_view_guest.h"

#include <optional>
#include <string>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "components/guest_view/browser/guest_view_event.h"
#include "components/guest_view/browser/guest_view_histogram_value.h"
#include "components/guest_view/browser/slim_web_view/grit/slim_web_view_strings.h"
#include "components/guest_view/browser/slim_web_view/request_utils.h"
#include "components/guest_view/browser/slim_web_view/slim_web_view_constants.h"
#include "components/url_pattern/simple_url_pattern_matcher.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/security_principal.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace guest_view {

namespace {

const char kStoragePartitionId[] = "partition";

const char kPersistPrefix[] = "persist:";

void ParsePartitionParam(const base::DictValue& create_params,
                         std::string* storage_partition_id,
                         bool* persist_storage) {
  const std::string* partition_str =
      create_params.FindString(kStoragePartitionId);
  if (!partition_str) {
    return;
  }

  // Since the "persist:" prefix is in ASCII, base::StartsWith will work fine on
  // UTF-8 encoded |partition_id|. If the prefix is a match, we can safely
  // remove the prefix without splicing in the middle of a multi-byte codepoint.
  // We can use the rest of the string as UTF-8 encoded one.
  if (base::StartsWith(*partition_str, kPersistPrefix,
                       base::CompareCase::SENSITIVE)) {
    size_t index = partition_str->find(":");
    CHECK(index != std::string::npos);
    // It is safe to do index + 1, since we tested for the full prefix above.
    *storage_partition_id = partition_str->substr(index + 1);

    if (storage_partition_id->empty()) {
      return;
    }
    *persist_storage = true;
  } else {
    *storage_partition_id = *partition_str;
    *persist_storage = false;
  }
}

// Parses the creation params for the `beforeSendHeaders` option. Returns
// the parsed params or an error message.
base::expected<std::optional<BeforeSendHeadersParams>, std::string>
ParseBeforeSendHeadersParams(const base::DictValue& create_params) {
  const base::DictValue* before_send_headers_dict =
      create_params.FindDict("beforeSendHeaders");
  if (!before_send_headers_dict) {
    return base::ok(std::nullopt);
  }
  std::optional<BeforeSendHeadersParams> params;
  params.emplace();
  const auto* resource_types =
      before_send_headers_dict->FindList("resourceTypes");
  if (resource_types) {
    for (const auto& resource_type : *resource_types) {
      if (!resource_type.is_string()) {
        return base::unexpected("resourceTypes elements must be strings.");
      }
      std::optional<RequestResourceType> parsed_resource_type =
          ParseRequestResourceType(resource_type.GetString());
      if (!parsed_resource_type) {
        return base::unexpected("Invalid resourceType provided.");
      }
      params->resource_types.insert(*parsed_resource_type);
    }
  }
  auto include_sub_frame_requests =
      before_send_headers_dict->FindBool("includeSubFrameRequests");
  if (include_sub_frame_requests) {
    params->include_sub_frame_requests = *include_sub_frame_requests;
  }
  const auto* add_headers = before_send_headers_dict->FindList("addHeaders");
  if (add_headers) {
    for (const auto& add_header : *add_headers) {
      if (!add_header.is_dict()) {
        return base::unexpected("addHeaders elements must be dictionaries.");
      }
      const auto* name = add_header.GetDict().FindString("name");
      if (!name) {
        return base::unexpected(
            "addHeaders dictionary must contain a 'name' string.");
      }
      const auto* value = add_header.GetDict().FindString("value");
      if (!value) {
        return base::unexpected(
            "addHeaders dictionary must contain a 'value' string.");
      }
      if (!net::HttpUtil::IsValidHeaderName(*name)) {
        return base::unexpected("Invalid header name provided.");
      }
      if (!net::HttpUtil::IsValidHeaderValue(*value)) {
        return base::unexpected("Invalid header value provided.");
      }
      if (!net::HttpUtil::IsSafeHeader(*name, *value)) {
        return base::unexpected("Unsafe header provided.");
      }
      params->add_headers.SetHeader(*name, *value);
    }
  }
  if (params->IsEmpty()) {
    // These params have no effect, so we can ignore them.
    return base::ok(std::nullopt);
  }
  return base::ok(std::move(params));
}

base::expected<std::optional<OriginCheckParams>, std::string>
ParseOriginCheckParams(const base::DictValue& create_params) {
  const base::DictValue* allowed_origins_dict =
      create_params.FindDict("allowedOrigins");
  if (!allowed_origins_dict) {
    return base::ok(std::nullopt);
  }
  std::optional<OriginCheckParams> params;
  params.emplace();
  const auto* resource_types = allowed_origins_dict->FindList("resourceTypes");
  if (resource_types) {
    for (const auto& resource_type : *resource_types) {
      if (!resource_type.is_string()) {
        return base::unexpected("resourceTypes elements must be strings.");
      }
      auto parsed_resource_type =
          ParseRequestResourceType(resource_type.GetString());
      if (!parsed_resource_type) {
        return base::unexpected("Invalid resourceType provided.");
      }
      params->resource_types.insert(*parsed_resource_type);
    }
  }
  const auto* allowed_origins =
      allowed_origins_dict->FindList("allowedOrigins");
  if (allowed_origins) {
    for (const auto& allowed_origin : *allowed_origins) {
      if (!allowed_origin.is_string()) {
        return base::unexpected("allowedOrigins elements must be strings.");
      }
      auto allowed_origin_string = allowed_origin.GetString();
      allowed_origin_string +=
          allowed_origin_string.ends_with("/") ? "*" : "/*";
      auto pattern = url_pattern::SimpleUrlPatternMatcher::Create(
          allowed_origin_string, nullptr);
      if (!pattern.has_value()) {
        return base::unexpected("Invalid url pattern provided: " +
                                allowed_origin_string);
      }
      params->allowed_origin_patterns.push_back(std::move(pattern.value()));
    }
  }
  if (params->IsEmpty()) {
    // These params have no effect, so we can ignore them.
    return base::ok(std::nullopt);
  }
  return base::ok(std::move(params));
}

std::string WindowOpenDispositionToString(
    WindowOpenDisposition window_open_disposition) {
  switch (window_open_disposition) {
    case WindowOpenDisposition::IGNORE_ACTION:
      return "ignore";
    case WindowOpenDisposition::SAVE_TO_DISK:
      return "save_to_disk";
    case WindowOpenDisposition::CURRENT_TAB:
      return "current_tab";
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      return "new_background_tab";
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      return "new_foreground_tab";
    case WindowOpenDisposition::NEW_WINDOW:
      return "new_window";
    case WindowOpenDisposition::NEW_POPUP:
      return "new_popup";
    default:
      NOTREACHED() << "Unknown Window Open Disposition";
  }
}

std::string TerminationStatusToString(base::TerminationStatus status) {
  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      return "normal";
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_STILL_RUNNING:
      return "abnormal";
#if BUILDFLAG(IS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
      return "oom killed";
#endif
#if BUILDFLAG(IS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
      return "oom";
#endif
    case base::TERMINATION_STATUS_OOM:
      return "oom";
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      return "killed";
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      return "crashed";
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      return "failed to launch";
#if BUILDFLAG(IS_WIN)
    case base::TERMINATION_STATUS_INTEGRITY_FAILURE:
      return "integrity failure";
#endif
    case base::TERMINATION_STATUS_EVICTED_FOR_MEMORY:
      return "evicted for memory";
    case base::TERMINATION_STATUS_MAX_ENUM:
      break;
  }
  NOTREACHED() << "Unknown Termination Status.";
}

}  // namespace

// static
const guest_view::GuestViewHistogramValue SlimWebViewGuest::HistogramValue =
    guest_view::GuestViewHistogramValue::kSlimWebView;

// static
std::unique_ptr<GuestViewBase> SlimWebViewGuest::Create(
    content::RenderFrameHost* owner_render_frame_host) {
  return base::WrapUnique(new SlimWebViewGuest(owner_render_frame_host));
}

SlimWebViewGuest::~SlimWebViewGuest() = default;

base::WeakPtr<SlimWebViewGuest> SlimWebViewGuest::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SlimWebViewGuest::Navigate(const GURL& url) {
  TRACE_EVENT_INSTANT("content", "SlimWebViewGuest::Navigate",
                      perfetto::Flow::FromPointer(this));
  content::NavigationController::LoadURLParams load_url_params(url);
  GetController().LoadURLWithParams(load_url_params);
}

bool SlimWebViewGuest::HasAllowedOrigins() const {
  return !!allowed_origins_params_;
}

base::expected<void, std::string> SlimWebViewGuest::IsUrlAllowed(
    RequestResourceType resource_type,
    const GURL& url) const {
  if (!url.is_valid()) {
    return base::unexpected("URL is not valid.");
  }
  if (url.IsAboutBlank()) {
    return base::ok();
  }
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return base::unexpected("URL must be https, http, or about:blank.");
  }
  if (!allowed_origins_params_) {
    return base::ok();
  }
  if (!allowed_origins_params_->resource_types.contains(resource_type)) {
    return base::ok();
  }
  url::Origin candidate_origin = url::Origin::Create(url);
  for (const auto& pattern : allowed_origins_params_->allowed_origin_patterns) {
    if (pattern->Match(url)) {
      return base::ok();
    }
  }
  return base::unexpected("URL origin is not allowed: " +
                          candidate_origin.Serialize());
}

SlimWebViewGuest::SlimWebViewGuest(
    content::RenderFrameHost* owner_render_frame_host)
    : GuestView<SlimWebViewGuest>(owner_render_frame_host) {}

bool SlimWebViewGuest::GuestHandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  CHECK(base::FeatureList::IsEnabled(features::kGuestViewMPArch));
  return false;
}

bool SlimWebViewGuest::IsWebContentsCreationOverridden(
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));
  return true;
}

// Transfers the responsibility of handling window creation to the client
// through the `newwindow` event.
content::WebContents* SlimWebViewGuest::CreateCustomWebContents(
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    bool is_new_browsing_instance,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    const content::StoragePartitionConfig& partition_config,
    content::SessionStorageNamespace* session_storage_namespace) {
  CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));

  base::DictValue request_info;
  request_info.Set(slim_web_view::kInitialHeight,
                   window_features.bounds.height());
  request_info.Set(slim_web_view::kInitialWidth,
                   window_features.bounds.width());
  request_info.Set(slim_web_view::kTargetURL, target_url.spec());
  request_info.Set(slim_web_view::kWindowOpenDisposition,
                   WindowOpenDispositionToString(disposition));
  base::DictValue args;
  args.Set(slim_web_view::kRequestInfo, std::move(request_info));
  DispatchEventToView(std::make_unique<GuestViewEvent>(
      slim_web_view::kEventNewWindow, std::move(args)));

  return nullptr;
}

void SlimWebViewGuest::RendererUnresponsive(
    content::WebContents* source,
    content::RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));

  DispatchEventToView(std::make_unique<GuestViewEvent>(
      slim_web_view::kEventUnresponsive, base::DictValue()));
}

void SlimWebViewGuest::RequestMediaAccessPermission(
    content::WebContents* source,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));

  GuestRequestMediaAccessPermission(request, std::move(callback));
}

void SlimWebViewGuest::CanDownload(const GURL& url,
                                   const std::string& request_method,
                                   base::OnceCallback<void(bool)> callback) {
  CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));
  permission_helper_.CanDownload(url, request_method, std::move(callback));
}

void SlimWebViewGuest::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!IsObservedNavigationWithinGuest(navigation_handle)) {
    return;
  }
  // New window creation is not supported in SlimWebView.
  CHECK(!GetOpener());
  if (navigation_handle->IsSameDocument()) {
    return;
  }
  bool is_top_level =
      IsObservedNavigationWithinGuestMainFrame(navigation_handle);
  TRACE_EVENT_INSTANT(
      "content", "SlimWebViewGuest::DidStartNavigation - CrossDocument",
      perfetto::Flow::FromPointer(this), "isTopLevel", is_top_level);
  base::DictValue args;
  args.Set(guest_view::kUrl, navigation_handle->GetURL().spec());
  args.Set(guest_view::kIsTopLevel, is_top_level);
  DispatchEventToView(std::make_unique<GuestViewEvent>(
      slim_web_view::kEventLoadStart, std::move(args)));
}

void SlimWebViewGuest::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!IsObservedNavigationWithinGuest(navigation_handle)) {
    return;
  }
  if (navigation_handle->IsErrorPage() || !navigation_handle->HasCommitted()) {
    // Suppress loadabort for "mailto" URLs.
    // Also during destruction, the owner is null so there's no point
    // trying to send the event.
    if (!navigation_handle->GetURL().SchemeIs(url::kMailToScheme) &&
        owner_rfh()) {
      // If a load is blocked, either by WebRequest or security checks, the
      // navigation may or may not have committed. So if we don't see an error
      // code, mark it as blocked.
      net::Error error_code = navigation_handle->GetNetErrorCode();
      if (error_code == net::OK) {
        error_code = net::ERR_BLOCKED_BY_CLIENT;
      }
      LoadAbort(IsObservedNavigationWithinGuestMainFrame(navigation_handle),
                navigation_handle->GetURL(), error_code);
    }
    // On failed navigation, the webview fires a loadabort (for the failed
    // navigation) and then a loadcommit (for the error page).
    if (!navigation_handle->IsErrorPage()) {
      return;
    }
  }

  bool is_top_level =
      IsObservedNavigationWithinGuestMainFrame(navigation_handle);
  if (!navigation_handle->IsSameDocument()) {
    TRACE_EVENT_INSTANT(
        "content", "SlimWebViewGuest::DidFinishNavigation - CrossDocument",
        perfetto::Flow::FromPointer(this), "isTopLevel", is_top_level);
  }
  base::DictValue args;
  args.Set(guest_view::kUrl, navigation_handle->GetURL().spec());
  args.Set(guest_view::kIsTopLevel, is_top_level);
  DispatchEventToView(std::make_unique<GuestViewEvent>(
      slim_web_view::kEventLoadCommit, std::move(args)));
}

const char* SlimWebViewGuest::GetAPINamespace() const {
  NOTREACHED() << "SlimWebView doesn't use the extensions API";
}

int SlimWebViewGuest::GetTaskPrefix() const {
  return IDS_TASK_MANAGER_SLIM_WEB_VIEW_TAG_PREFIX;
}

void SlimWebViewGuest::GuestViewDocumentOnLoadCompleted() {
  TRACE_EVENT_INSTANT("content",
                      "SlimWebViewGuest::GuestViewDocumentOnLoadCompleted",
                      perfetto::Flow::FromPointer(this));
  DispatchEventToView(std::make_unique<GuestViewEvent>(
      slim_web_view::kEventContentLoad, base::DictValue()));
}

bool SlimWebViewGuest::IsAutoSizeSupported() const {
  return true;
}

void SlimWebViewGuest::GuestSizeChangedDueToAutoSize(
    const gfx::Size& old_size,
    const gfx::Size& new_size) {
  base::DictValue args;
  args.Set(slim_web_view::kOldHeight, old_size.height());
  args.Set(slim_web_view::kOldWidth, old_size.width());
  args.Set(slim_web_view::kNewHeight, new_size.height());
  args.Set(slim_web_view::kNewWidth, new_size.width());
  DispatchEventToView(std::make_unique<GuestViewEvent>(
      slim_web_view::kEventSizeChanged, std::move(args)));
}

void SlimWebViewGuest::GuestViewMainFrameProcessGone(
    base::TerminationStatus status) {
  base::DictValue args;
  args.Set(slim_web_view::kReason, TerminationStatusToString(status));
  args.Set(slim_web_view::kProcessId,
           GetGuestMainFrame()->GetProcess()->GetID().value());
  DispatchEventToView(std::make_unique<GuestViewEvent>(
      slim_web_view::kEventExit, std::move(args)));
}

void SlimWebViewGuest::GuestRequestMediaAccessPermission(
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  CHECK(!IsOwnedByControlledFrameEmbedder());
  permission_helper_.RequestMediaAccessPermission(request, std::move(callback));
}

void SlimWebViewGuest::MaybeRecreateGuestContents(
    content::RenderFrameHost* outer_contents_frame) {
  NOTREACHED() << "new window creation is not supported in SlimWebView";
}

void SlimWebViewGuest::CreateInnerPage(
    std::unique_ptr<GuestViewBase> owned_this,
    scoped_refptr<content::SiteInstance> site_instance,
    const base::DictValue& create_params,
    GuestPageCreatedCallback callback) {
  TRACE_EVENT_INSTANT("content", "SlimWebViewGuest::CreateInnerPage",
                      perfetto::Flow::FromPointer(this));
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    // TODO(crbug.com/460804848): Complete the implementation for MPArch.
    NOTIMPLEMENTED();
    RejectGuestCreation(std::move(owned_this), std::move(callback));
    return;
  }
  if (site_instance) {
    RejectGuestCreation(std::move(owned_this), std::move(callback));
    DVLOG(2) << "Rejected new window creation";
    return;
  }

  auto parse_result = ParseBeforeSendHeadersParams(create_params);
  if (!parse_result.has_value()) {
    DVLOG(2) << "Failed to parse beforeSendHeaders: " << parse_result.error();
    RejectGuestCreation(std::move(owned_this), std::move(callback));
    return;
  }
  before_send_headers_params_ = std::move(parse_result.value());

  auto origin_check_params_result = ParseOriginCheckParams(create_params);
  if (!origin_check_params_result.has_value()) {
    DVLOG(2) << "Failed to parse allowedOrigins: "
             << origin_check_params_result.error();
    RejectGuestCreation(std::move(owned_this), std::move(callback));
    return;
  }
  allowed_origins_params_ = std::move(origin_check_params_result.value());

  std::string storage_partition_id;
  bool persist_storage = false;
  ParsePartitionParam(create_params, &storage_partition_id, &persist_storage);
  content::StoragePartitionConfig partition_config =
      content::StoragePartitionConfig::Create(browser_context(),
                                              owner_rfh()
                                                  ->GetSiteInstance()
                                                  ->GetSecurityPrincipal()
                                                  .GetDeprecatedSiteURL()
                                                  .GetHost(),
                                              storage_partition_id,
                                              !persist_storage);

  scoped_refptr<content::SiteInstance> guest_site_instance =
      content::SiteInstance::CreateForGuest(browser_context(),
                                            partition_config);
  content::WebContents::CreateParams stored_params(
      browser_context(), std::move(guest_site_instance));
  stored_params.guest_delegate = this;
  SetCreateParams(create_params, stored_params);
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(stored_params);
  std::move(callback).Run(std::move(owned_this), std::move(new_contents));
}

void SlimWebViewGuest::GuestViewDidStopLoading() {
  TRACE_EVENT_INSTANT("content", "SlimWebViewGuest::GuestViewDidStopLoading",
                      perfetto::TerminatingFlow::FromPointer(this));
  DispatchEventToView(std::make_unique<GuestViewEvent>(
      slim_web_view::kEventLoadStop, base::DictValue()));
}

void SlimWebViewGuest::LoadAbort(bool is_top_level,
                                 const GURL& url,
                                 net::Error error_code) {
  base::DictValue args;
  args.Set(guest_view::kIsTopLevel, is_top_level);
  args.Set(guest_view::kUrl, url.possibly_invalid_spec());
  args.Set(guest_view::kCode, error_code);
  args.Set(guest_view::kReason, net::ErrorToShortString(error_code));
  DispatchEventToView(std::make_unique<GuestViewEvent>(
      slim_web_view::kEventLoadAbort, std::move(args)));
}

}  // namespace guest_view
