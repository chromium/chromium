// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_gen204_controller.h"

#include "base/containers/span.h"
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

const int kMaxDownloadBytes = 1024 * 1024;
const int kCopyAsImageTaskCompletionID = 233325;
const int kCopyTextTaskCompletionID = 198153;
const int kSaveAsImageTaskCompletionID = 233326;
const int kSelectTextTaskCompletionID = 198157;
const int kTranslateTaskCompletionID = 198158;
constexpr char kGen204IdentifierQueryParameter[] = "plla";
constexpr char kRequestTypeQueryParameter[] = "rt";
constexpr char kFullPageObjectsFetchRequestType[] = "fpof";
constexpr char kFullPageTranslateFetchRequestType[] = "fptf";

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

}  // namespace

LensOverlayGen204Controller::LensOverlayGen204Controller(
    lens::LensOverlayInvocationSource invocation_source,
    Profile* profile)
    : invocation_source_(invocation_source), profile_(profile) {}
LensOverlayGen204Controller::~LensOverlayGen204Controller() = default;

void LensOverlayGen204Controller::SetGen204Id(uint64_t gen204_id) {
  gen204_id_ = gen204_id;
}

void LensOverlayGen204Controller::SendLatencyGen204IfEnabled(
    int64_t latency_ms,
    bool is_translate_query) {
  if (profile_ && lens::features::GetLensOverlaySendLatencyGen204() &&
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven()) {
    std::string query = base::StringPrintf(
        "gen_204?atyp=csi&%s=%s&%s=%s.%s&s=web",
        kGen204IdentifierQueryParameter,
        base::NumberToString(gen204_id_).c_str(), kRequestTypeQueryParameter,
        is_translate_query ? kFullPageTranslateFetchRequestType
                           : kFullPageObjectsFetchRequestType,
        base::NumberToString(latency_ms).c_str());
    auto fetch_url = GURL(TemplateURLServiceFactory::GetForProfile(profile_)
                              ->search_terms_data()
                              .GoogleBaseURLValue())
                         .Resolve(query);
    fetch_url =
        lens::AppendInvocationSourceParamToURL(fetch_url, invocation_source_);
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = fetch_url;
    latency_gen204_loader_ = network::SimpleURLLoader::Create(
        std::move(request), kTrafficAnnotationTag);
    latency_gen204_loader_->DownloadToString(
        profile_->GetURLLoaderFactory().get(),
        base::BindOnce(&LensOverlayGen204Controller::OnLatencyGen204Response,
                       base::Unretained(this)),
        kMaxDownloadBytes);
  }
}

void LensOverlayGen204Controller::SendTaskCompletionGen204IfEnabled(
    std::string encoded_analytics_id,
    lens::mojom::UserAction user_action) {
  if (profile_ && lens::features::GetLensOverlaySendTaskCompletionGen204() &&
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven()) {
    int task_id;
    switch (user_action) {
      case mojom::UserAction::kTextSelection:
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
        "gen_204?uact=4&%s=%s&rcid=%d&cad=%s", kGen204IdentifierQueryParameter,
        base::NumberToString(gen204_id_).c_str(), task_id,
        encoded_analytics_id.c_str());
    auto fetch_url = GURL(TemplateURLServiceFactory::GetForProfile(profile_)
                              ->search_terms_data()
                              .GoogleBaseURLValue())
                         .Resolve(query);
    fetch_url =
        lens::AppendInvocationSourceParamToURL(fetch_url, invocation_source_);
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = fetch_url;
    task_completion_gen204_loader_ = network::SimpleURLLoader::Create(
        std::move(request), kTrafficAnnotationTag);
    task_completion_gen204_loader_->DownloadToString(
        profile_->GetURLLoaderFactory().get(),
        base::BindOnce(
            &LensOverlayGen204Controller::OnTaskCompletionGen204Response,
            base::Unretained(this)),
        kMaxDownloadBytes);
  }
}

void LensOverlayGen204Controller::OnQueryFlowEnd(
    std::string encoded_analytics_id) {
  // TODO(b/364291616): Log and send text gleam render gen204s. This will
  // require a check in this function to send an end event if no explicit
  // end event was received from the overlay due to it closing.
}

void LensOverlayGen204Controller::OnLatencyGen204Response(
    std::unique_ptr<std::string> response_body) {
  latency_gen204_loader_.reset();
}

void LensOverlayGen204Controller::OnTaskCompletionGen204Response(
    std::unique_ptr<std::string> response_body) {
  task_completion_gen204_loader_.reset();
}

}  // namespace lens
