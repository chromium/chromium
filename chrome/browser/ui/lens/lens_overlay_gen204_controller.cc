// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_gen204_controller.h"

#include "base/containers/span.h"
#include "base/format_macros.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "components/base32/base32.h"
#include "components/lens/lens_features.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace lens {

namespace {

using LatencyType = LensOverlayGen204Controller::LatencyType;

constexpr int kMaxDownloadBytes = 1024 * 1024;

// Task completion ids.
constexpr int kCopyAsImageTaskCompletionID = 233325;
constexpr int kCopyTextTaskCompletionID = 198153;
constexpr int kSaveAsImageTaskCompletionID = 233326;
constexpr int kSelectTextTaskCompletionID = 198157;
constexpr int kTranslateTaskCompletionID = 198158;

// Semantic event ids.
constexpr int kTextGleamsViewStartSemanticEventID = 234181;
constexpr int kTextGleamsViewEndSemanticEventID = 234180;

// Query parameter keys.
constexpr char kEncodedAnalyticsIdParameter[] = "cad";
constexpr char kGen204IdentifierQueryParameter[] = "plla";
constexpr char kLatencyRequestTypeQueryParameter[] = "rt";
constexpr char kVisualInputTypeQueryParameter[] = "vit";
// Event id param used for both semantic events and task completions.
constexpr char kEventIdParameter[] = "rcid";

// Request type parameter values.
constexpr char kFullPageObjectsFetchLatencyId[] = "fpof";
constexpr char kFullPageTranslateFetchLatencyId[] = "fptf";
constexpr char kPageContentUploadLatencyId[] = "pcu";
constexpr char kPartialPageContentUploadLatencyId[] = "ppcu";
constexpr char kInteractionFetchLatencyId[] = "lif";
constexpr char kFetchStickyClusterInfoLatencyId[] = "sct";
constexpr char kInvocationToInitialClusterInfoRequestLatencyId[] = "cstcirs";
constexpr char kInvocationToInitialFullObjectsRequestLatencyId[] = "cstiors";
constexpr char kInvocationToInitialInteractionRequestLatencyId[] = "cstiirs";
constexpr char kInvocationToInitialPageContentRequestLatencyId[] = "cstipcurs";
constexpr char kInvocationToInitialPartialPageContentRequestLatencyId[] =
    "cstippcurs";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("lens_overlay_gen204", R"(
        semantics {
          sender: "Lens"
          description: "A request to the gen204 endpoint for the Lens "
            "Overlay feature in Chrome."
          trigger: "The user triggered a Lens Overlay Flow by entering "
            "the experience via the right click menu option for "
            "searching images on the page. This annotation corresponds "
            "to the gen204 logging network requests sent by the Lens "
            "overlay to track latency and interaction data when the "
            "user is opted into metrics reporting."
          data: "Timestamp and interaction data. Only the action type "
            "(e.g. the  user selected text) and timestamp data is sent, "
            "along with basic state information from the query controller."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "hujasonx@google.com"
            }
            contacts {
              email: "lens-chrome@google.com"
            }
          }
          user_data {
            type: USER_CONTENT
            type: WEB_CONTENT
          }
          last_reviewed: "2024-09-24"
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This feature is only shown in menus by default and does "
            "nothing without explicit user action. It will be disabled if "
            "the user is not opted into metrics reporting, which is on by "
            "default."
          chrome_policy {
            LensOverlaySettings {
              LensOverlaySettings: 1
            }
            MetricsReportingEnabled{
              policy_options {mode: MANDATORY}
              MetricsReportingEnabled: false
            }
          }
        }
      )");

std::string LatencyIdForType(LatencyType latency_type) {
  switch (latency_type) {
    case LatencyType::kInvocationToInitialClusterInfoRequestSent:
      return kInvocationToInitialClusterInfoRequestLatencyId;
    case LatencyType::kInvocationToInitialFullPageObjectsRequestSent:
      return kInvocationToInitialFullObjectsRequestLatencyId;
    case LatencyType::kInvocationToInitialInteractionRequestSent:
      return kInvocationToInitialInteractionRequestLatencyId;
    case LatencyType::kInvocationToInitialPageContentRequestSent:
      return kInvocationToInitialPageContentRequestLatencyId;
    case LatencyType::kInvocationToInitialPartialPageContentRequestSent:
      return kInvocationToInitialPartialPageContentRequestLatencyId;
    case LatencyType::kFullPageObjectsRequestFetchLatency:
      return kFullPageObjectsFetchLatencyId;
    case LatencyType::kFullPageTranslateRequestFetchLatency:
      return kFullPageTranslateFetchLatencyId;
    case LatencyType::kInteractionRequestFetchLatency:
      return kInteractionFetchLatencyId;
    case LatencyType::kPageContentUploadLatency:
      return kPageContentUploadLatencyId;
    case LatencyType::kPartialPageContentUploadLatency:
      return kPartialPageContentUploadLatencyId;
  }
}

}  // namespace

LensOverlayGen204Controller::LensOverlayGen204Controller() = default;
LensOverlayGen204Controller::~LensOverlayGen204Controller() = default;

void LensOverlayGen204Controller::OnQueryFlowStart(
    lens::LensOverlayInvocationSource invocation_source,
    Profile* profile,
    uint64_t gen204_id) {
  invocation_source_ = invocation_source;
  profile_ = profile;
  gen204_id_ = gen204_id;
}

void LensOverlayGen204Controller::SendLatencyGen204IfEnabled(
    LatencyType latency_type,
    base::TimeDelta latency_duration,
    std::string vit_query_param_value,
    std::optional<base::TimeDelta> cluster_info_latency,
    std::optional<std::string> encoded_analytics_id) {
  if (profile_ && lens::features::GetLensOverlaySendLatencyGen204()) {
    std::string cluster_info_latency_string =
        cluster_info_latency.has_value() &&
                latency_type == LatencyType::kFullPageObjectsRequestFetchLatency
            ? base::StringPrintf(
                  ",%s.%s", kFetchStickyClusterInfoLatencyId,
                  base::NumberToString(cluster_info_latency->InMilliseconds())
                      .c_str())
            : "";
    // PRIu64 and PRId64 are macros for formatting uint64_t and int64_t
    // respectively, allowing us to bypass using NumberToString.
    std::string query = base::StringPrintf(
        "gen_204?atyp=csi&%s=%" PRIu64 "&%s=%s.%" PRId64 "%s&s=web&%s=%s",
        kGen204IdentifierQueryParameter, gen204_id_,
        kLatencyRequestTypeQueryParameter,
        LatencyIdForType(latency_type).c_str(),
        latency_duration.InMilliseconds(), cluster_info_latency_string,
        kVisualInputTypeQueryParameter, vit_query_param_value);
    auto fetch_url = GURL(TemplateURLServiceFactory::GetForProfile(profile_)
                              ->search_terms_data()
                              .GoogleBaseURLValue())
                         .Resolve(query);
    fetch_url =
        lens::AppendInvocationSourceParamToURL(fetch_url, invocation_source_);
    if (encoded_analytics_id.has_value()) {
      fetch_url = net::AppendOrReplaceQueryParameter(
          fetch_url, kEncodedAnalyticsIdParameter,
          encoded_analytics_id.value());
    }
    CheckMetricsConsentAndIssueGen204NetworkRequest(fetch_url);
  }
}

void LensOverlayGen204Controller::SendTaskCompletionGen204IfEnabled(
    std::string encoded_analytics_id,
    lens::mojom::UserAction user_action) {
  if (profile_ && lens::features::GetLensOverlaySendTaskCompletionGen204()) {
    int task_id;
    switch (user_action) {
      case mojom::UserAction::kTextSelection:
        [[fallthrough]];
      case mojom::UserAction::kTranslateTextSelection:
        task_id = kSelectTextTaskCompletionID;
        break;
      case mojom::UserAction::kCopyText:
        task_id = kCopyTextTaskCompletionID;
        break;
      case mojom::UserAction::kTranslateText:
        task_id = kTranslateTaskCompletionID;
        break;
      case mojom::UserAction::kCopyAsImage:
        task_id = kCopyAsImageTaskCompletionID;
        break;
      case mojom::UserAction::kSaveAsImage:
        task_id = kSaveAsImageTaskCompletionID;
        break;
      default:
        // Other user actions should not send an associated gen204 ping.
        return;
    }
    std::string query = base::StringPrintf(
        "gen_204?uact=4&%s=%" PRIu64 "&%s=%d&%s=%s",
        kGen204IdentifierQueryParameter, gen204_id_, kEventIdParameter, task_id,
        kEncodedAnalyticsIdParameter, encoded_analytics_id.c_str());
    auto fetch_url = GURL(TemplateURLServiceFactory::GetForProfile(profile_)
                              ->search_terms_data()
                              .GoogleBaseURLValue())
                         .Resolve(query);
    fetch_url =
        lens::AppendInvocationSourceParamToURL(fetch_url, invocation_source_);
    CheckMetricsConsentAndIssueGen204NetworkRequest(fetch_url);
  }
}

void LensOverlayGen204Controller::SendSemanticEventGen204IfEnabled(
    lens::mojom::SemanticEvent event) {
  if (profile_ && lens::features::GetLensOverlaySendSemanticEventGen204()) {
    int event_id;
    switch (event) {
      case mojom::SemanticEvent::kTextGleamsViewStart:
        event_id = kTextGleamsViewStartSemanticEventID;
        break;
      case mojom::SemanticEvent::kTextGleamsViewEnd:
        event_id = kTextGleamsViewEndSemanticEventID;
        break;
    }
    std::string query = base::StringPrintf(
        "gen_204?uact=1&%s=%d&zx=%" PRId64 "&%s=%" PRIu64, kEventIdParameter,
        event_id, base::Time::Now().InMillisecondsSinceUnixEpoch(),
        kGen204IdentifierQueryParameter, gen204_id_);
    auto fetch_url = GURL(TemplateURLServiceFactory::GetForProfile(profile_)
                              ->search_terms_data()
                              .GoogleBaseURLValue())
                         .Resolve(query);
    fetch_url =
        lens::AppendInvocationSourceParamToURL(fetch_url, invocation_source_);
    CheckMetricsConsentAndIssueGen204NetworkRequest(fetch_url);
  }
}

void LensOverlayGen204Controller::OnQueryFlowEnd(
    std::string encoded_analytics_id) {
  // Send a text gleams view end event because the event trigger from webui
  // will not fire when the overlay is closing. The server will dedupe
  // end events.
  SendSemanticEventGen204IfEnabled(mojom::SemanticEvent::kTextGleamsViewEnd);
  profile_ = nullptr;
}

void LensOverlayGen204Controller::
    CheckMetricsConsentAndIssueGen204NetworkRequest(GURL url) {
  if (!g_browser_process->GetMetricsServicesManager()
           ->IsMetricsConsentGiven()) {
    return;
  }
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  gen204_loaders_.push_back(network::SimpleURLLoader::Create(
      std::move(request), kTrafficAnnotationTag));
  gen204_loaders_.back()->DownloadToString(
      profile_->GetURLLoaderFactory().get(),
      base::BindOnce(&LensOverlayGen204Controller::OnGen204NetworkResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     gen204_loaders_.back().get()),
      kMaxDownloadBytes);
}

void LensOverlayGen204Controller::OnGen204NetworkResponse(
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response_body) {
  std::erase_if(
      gen204_loaders_,
      [source](const std::unique_ptr<network::SimpleURLLoader>& loader) {
        return loader.get() == source;
      });
}

}  // namespace lens
