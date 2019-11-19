// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/history_ui_favicon_request_handler_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/features.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/image/image_png_rep.h"
#include "url/gurl.h"

namespace favicon {

namespace {

void RecordFaviconAvailabilityAndLatencyMetric(
    HistoryUiFaviconRequestOrigin origin_for_uma,
    base::Time request_start_time_for_uma,
    FaviconAvailability availability) {
  base::TimeDelta latency = base::Time::Now() - request_start_time_for_uma;
  switch (origin_for_uma) {
    case HistoryUiFaviconRequestOrigin::kHistory:
      UMA_HISTOGRAM_ENUMERATION("Sync.SyncedHistoryFaviconAvailability.HISTORY",
                                availability);
      UMA_HISTOGRAM_TIMES("Sync.SyncedHistoryFaviconLatency.HISTORY", latency);
      break;
    case HistoryUiFaviconRequestOrigin::kHistorySyncedTabs:
      UMA_HISTOGRAM_ENUMERATION(
          "Sync.SyncedHistoryFaviconAvailability.SYNCED_TABS", availability);
      UMA_HISTOGRAM_TIMES("Sync.SyncedHistoryFaviconLatency.SYNCED_TABS",
                          latency);
      break;
    case HistoryUiFaviconRequestOrigin::kRecentTabs:
      UMA_HISTOGRAM_ENUMERATION(
          "Sync.SyncedHistoryFaviconAvailability.RECENTLY_CLOSED_TABS",
          availability);
      UMA_HISTOGRAM_TIMES(
          "Sync.SyncedHistoryFaviconLatency.RECENTLY_CLOSED_TABS", latency);
      break;
  }
}

void RecordFaviconServerGroupingMetric(
    HistoryUiFaviconRequestOrigin origin_for_uma,
    int group_size) {
  DCHECK_GE(group_size, 0);
  switch (origin_for_uma) {
    case HistoryUiFaviconRequestOrigin::kHistory:
      base::UmaHistogramCounts100(
          "Sync.RequestGroupSizeForSyncedHistoryFavicons.HISTORY", group_size);
      break;
    case HistoryUiFaviconRequestOrigin::kHistorySyncedTabs:
      base::UmaHistogramCounts100(
          "Sync.RequestGroupSizeForSyncedHistoryFavicons.SYNCED_TABS",
          group_size);
      break;
    case HistoryUiFaviconRequestOrigin::kRecentTabs:
      base::UmaHistogramCounts100(
          "Sync.RequestGroupSizeForSyncedHistoryFavicons.RECENTLY_CLOSED_TABS",
          group_size);
      break;
  }
}

// Parameter used for local bitmap queries by page url. The url is an origin,
// and it may not have had a favicon associated with it. A trickier case is when
// it only has domain-scoped cookies, but visitors are redirected to HTTPS on
// visiting. It defaults to a HTTP scheme, but the favicon will be associated
// with the HTTPS URL and hence won't be found if we include the scheme in the
// lookup. Set |fallback_to_host|=true so the favicon database will fall back to
// matching only the hostname to have the best chance of finding a favicon.
// TODO(victorvianna): Consider passing this as a parameter in the API.
const bool kFallbackToHost = true;

// Parameter used for local bitmap queries by page url.
favicon_base::IconTypeSet GetIconTypesForLocalQuery() {
  return {favicon_base::IconType::kFavicon, favicon_base::IconType::kTouchIcon,
          favicon_base::IconType::kTouchPrecomposedIcon,
          favicon_base::IconType::kWebManifestIcon};
}

GURL GetGroupIdentifier(const GURL& page_url, const GURL& icon_url) {
  // If it is not possible to find a mapped icon url, identify the group of
  // |page_url| by itself.
  return !icon_url.is_empty() ? icon_url : page_url;
}

}  // namespace

HistoryUiFaviconRequestHandlerImpl::HistoryUiFaviconRequestHandlerImpl(
    const SyncedFaviconGetter& synced_favicon_getter,
    const CanSendHistoryDataGetter& can_send_history_data_getter,
    FaviconService* favicon_service,
    LargeIconService* large_icon_service)
    : favicon_service_(favicon_service),
      large_icon_service_(large_icon_service),
      synced_favicon_getter_(synced_favicon_getter),
      can_send_history_data_getter_(can_send_history_data_getter) {
  DCHECK(favicon_service);
  DCHECK(large_icon_service);
}

HistoryUiFaviconRequestHandlerImpl::~HistoryUiFaviconRequestHandlerImpl() {}

void HistoryUiFaviconRequestHandlerImpl::GetRawFaviconForPageURL(
    const GURL& page_url,
    int desired_size_in_pixel,
    favicon_base::FaviconRawBitmapCallback callback,
    FaviconRequestPlatform request_platform,
    HistoryUiFaviconRequestOrigin request_origin_for_uma,
    const GURL& icon_url_for_uma,
    base::CancelableTaskTracker* tracker) {
  // First attempt to find the icon locally.
  favicon_service_->GetRawFaviconForPageURL(
      page_url, GetIconTypesForLocalQuery(), desired_size_in_pixel,
      kFallbackToHost,
      base::BindOnce(
          &HistoryUiFaviconRequestHandlerImpl::OnBitmapLocalDataAvailable,
          weak_ptr_factory_.GetWeakPtr(), CanQueryGoogleServer(), page_url,
          desired_size_in_pixel,
          /*response_callback=*/std::move(callback), request_platform,
          request_origin_for_uma, icon_url_for_uma, base::Time::Now(), tracker),
      tracker);
}

void HistoryUiFaviconRequestHandlerImpl::GetFaviconImageForPageURL(
    const GURL& page_url,
    favicon_base::FaviconImageCallback callback,
    HistoryUiFaviconRequestOrigin request_origin_for_uma,
    const GURL& icon_url_for_uma,
    base::CancelableTaskTracker* tracker) {
  // First attempt to find the icon locally.
  favicon_service_->GetFaviconImageForPageURL(
      page_url,
      base::BindOnce(
          &HistoryUiFaviconRequestHandlerImpl::OnImageLocalDataAvailable,
          weak_ptr_factory_.GetWeakPtr(), CanQueryGoogleServer(), page_url,
          /*response_callback=*/std::move(callback), request_origin_for_uma,
          icon_url_for_uma, base::Time::Now(), tracker),
      tracker);
}

void HistoryUiFaviconRequestHandlerImpl::OnBitmapLocalDataAvailable(
    bool can_query_google_server,
    const GURL& page_url,
    int desired_size_in_pixel,
    favicon_base::FaviconRawBitmapCallback response_callback,
    FaviconRequestPlatform platform,
    HistoryUiFaviconRequestOrigin origin_for_uma,
    const GURL& icon_url_for_uma,
    base::Time request_start_time_for_uma,
    base::CancelableTaskTracker* tracker,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  if (bitmap_result.is_valid()) {
    // The icon comes from local storage now even though it may have been
    // originally retrieved from Google server.
    RecordFaviconAvailabilityAndLatencyMetric(origin_for_uma,
                                              request_start_time_for_uma,
                                              FaviconAvailability::kLocal);
    large_icon_service_->TouchIconFromGoogleServer(bitmap_result.icon_url);
    std::move(response_callback).Run(bitmap_result);
    return;
  }

  if (can_query_google_server) {
    // TODO(victorvianna): Avoid using AdaptCallbackForRepeating.
    base::RepeatingCallback<void(const favicon_base::FaviconRawBitmapResult&)>
        repeating_response_callback =
            base::AdaptCallbackForRepeating(std::move(response_callback));
    RequestFromGoogleServer(
        page_url,
        /*empty_response_callback=*/
        base::BindOnce(repeating_response_callback,
                       favicon_base::FaviconRawBitmapResult()),
        /*local_lookup_callback=*/
        base::BindOnce(
            base::IgnoreResult(&FaviconService::GetRawFaviconForPageURL),
            base::Unretained(favicon_service_), page_url,
            GetIconTypesForLocalQuery(), desired_size_in_pixel, kFallbackToHost,
            repeating_response_callback, tracker),
        origin_for_uma, icon_url_for_uma, request_start_time_for_uma);
    return;
  }

  favicon_base::FaviconRawBitmapResult sync_bitmap_result =
      synced_favicon_getter_.Run(page_url);
  if (sync_bitmap_result.is_valid()) {
    // If request to sync succeeds, resize bitmap to desired size and send.
    RecordFaviconAvailabilityAndLatencyMetric(
        origin_for_uma, request_start_time_for_uma, FaviconAvailability::kSync);
    std::move(response_callback)
        .Run(favicon_base::ResizeFaviconBitmapResult({sync_bitmap_result},
                                                     desired_size_in_pixel));
    return;
  }

  // If sync does not have the favicon, send empty response.
  RecordFaviconAvailabilityAndLatencyMetric(origin_for_uma,
                                            request_start_time_for_uma,
                                            FaviconAvailability::kNotAvailable);
  std::move(response_callback).Run(favicon_base::FaviconRawBitmapResult());
}

void HistoryUiFaviconRequestHandlerImpl::OnImageLocalDataAvailable(
    bool can_query_google_server,
    const GURL& page_url,
    favicon_base::FaviconImageCallback response_callback,
    HistoryUiFaviconRequestOrigin origin_for_uma,
    const GURL& icon_url_for_uma,
    base::Time request_start_time_for_uma,
    base::CancelableTaskTracker* tracker,
    const favicon_base::FaviconImageResult& image_result) {
  if (!image_result.image.IsEmpty()) {
    // The icon comes from local storage now even though it may have been
    // originally retrieved from Google server.
    RecordFaviconAvailabilityAndLatencyMetric(origin_for_uma,
                                              request_start_time_for_uma,
                                              FaviconAvailability::kLocal);
    large_icon_service_->TouchIconFromGoogleServer(image_result.icon_url);
    std::move(response_callback).Run(image_result);
    return;
  }

  if (can_query_google_server) {
    // TODO(victorvianna): Avoid using AdaptCallbackForRepeating.
    base::RepeatingCallback<void(const favicon_base::FaviconImageResult&)>
        repeating_response_callback =
            base::AdaptCallbackForRepeating(std::move(response_callback));
    // We use CreateForDesktop because GetFaviconImageForPageURL is only called
    // by desktop.
    RequestFromGoogleServer(
        page_url,
        /*empty_response_callback=*/
        base::BindOnce(repeating_response_callback,
                       favicon_base::FaviconImageResult()),
        /*local_lookup_callback=*/
        base::BindOnce(
            base::IgnoreResult(&FaviconService::GetFaviconImageForPageURL),
            base::Unretained(favicon_service_), page_url,
            repeating_response_callback, tracker),
        origin_for_uma, icon_url_for_uma, request_start_time_for_uma);
    return;
  }

  favicon_base::FaviconRawBitmapResult sync_bitmap_result =
      synced_favicon_getter_.Run(page_url);
  if (sync_bitmap_result.is_valid()) {
    // If request to sync succeeds, convert the retrieved bitmap to image and
    // send.
    RecordFaviconAvailabilityAndLatencyMetric(
        origin_for_uma, request_start_time_for_uma, FaviconAvailability::kSync);
    favicon_base::FaviconImageResult sync_image_result;
    sync_image_result.image =
        gfx::Image::CreateFrom1xPNGBytes(sync_bitmap_result.bitmap_data.get());
    std::move(response_callback).Run(sync_image_result);
    return;
  }

  // If sync does not have the favicon, send empty response.
  RecordFaviconAvailabilityAndLatencyMetric(origin_for_uma,
                                            request_start_time_for_uma,
                                            FaviconAvailability::kNotAvailable);
  std::move(response_callback).Run(favicon_base::FaviconImageResult());
}

void HistoryUiFaviconRequestHandlerImpl::RequestFromGoogleServer(
    const GURL& page_url,
    base::OnceClosure empty_response_callback,
    base::OnceClosure local_lookup_callback,
    HistoryUiFaviconRequestOrigin origin_for_uma,
    const GURL& icon_url_for_uma,
    base::Time request_start_time_for_uma) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "history_ui_favicon_request_handler_get_favicon",
          R"(
      semantics {
        sender: "Favicon Request Handler"
        description:
          "Sends a request to Google favicon server to rerieve the favicon "
          "bitmap for an entry in the history UI or the recent tabs menu UI."
        trigger:
          "The user visits chrome://history, chrome://history/syncedTabs or "
          "uses the Recent Tabs menu. A request can be sent if Chrome does not "
          "have a favicon for a particular page url. Only happens if history "
          "sync (with no custom passphrase) is enabled."
        data: "Page URL and desired icon size."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "You can disable this by toggling Sync and Google services > Manage "
          "sync > History"
        chrome_policy {
            SyncDisabled {
              policy_options {mode: MANDATORY}
              SyncDisabled: true
            }
          }
      })");
  // Increase count of theoretical waiting callbacks in the group of |page_url|.
  GURL group_identifier = GetGroupIdentifier(page_url, icon_url_for_uma);
  int group_count =
      ++group_callbacks_count_[group_identifier];  // Map defaults to 0.
  // If grouping was implemented, only the first requested page url of a group
  // would be effectively sent to the server and become responsible for calling
  // all callbacks in its group once done.
  GURL group_to_clear = group_count == 1 ? group_identifier : GURL();
  // If |trim_url_path| parameter in the experiment is true, the path part of
  // the url is stripped in the request, but result is stored under the complete
  // url.
  bool should_trim_url_path = base::GetFieldTrialParamByFeatureAsBool(
      kEnableHistoryFaviconsGoogleServerQuery, "trim_url_path",
      /* default_value= */ false);
  large_icon_service_
      ->GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          page_url,
          /*may_page_url_be_private=*/true, should_trim_url_path,
          traffic_annotation,
          base::BindOnce(
              &HistoryUiFaviconRequestHandlerImpl::OnGoogleServerDataAvailable,
              weak_ptr_factory_.GetWeakPtr(),
              std::move(empty_response_callback),
              std::move(local_lookup_callback), origin_for_uma, group_to_clear,
              request_start_time_for_uma));
}

void HistoryUiFaviconRequestHandlerImpl::OnGoogleServerDataAvailable(
    base::OnceClosure empty_response_callback,
    base::OnceClosure local_lookup_callback,
    HistoryUiFaviconRequestOrigin origin_for_uma,
    const GURL& group_to_clear,
    base::Time request_start_time_for_uma,
    favicon_base::GoogleFaviconServerRequestStatus status) {
  if (!group_to_clear.is_empty()) {
    auto it = group_callbacks_count_.find(group_to_clear);
    RecordFaviconServerGroupingMetric(origin_for_uma, it->second);
    group_callbacks_count_.erase(it);
  }
  // When parallel requests return the same icon url, we get FAILURE_ON_WRITE,
  // since we're trying to write repeated values to the DB. Or if a write
  // operation to that icon url was already completed, we may get status
  // FAILURE_ICON_EXISTS_IN_DB. In both cases we're able to subsequently
  // retrieve the icon from local storage.
  if (status == favicon_base::GoogleFaviconServerRequestStatus::SUCCESS ||
      status ==
          favicon_base::GoogleFaviconServerRequestStatus::FAILURE_ON_WRITE ||
      status == favicon_base::GoogleFaviconServerRequestStatus::
                    FAILURE_ICON_EXISTS_IN_DB) {
    // We record the metrics as kLocal since the icon will be subsequently
    // retrieved from local storage.
    RecordFaviconAvailabilityAndLatencyMetric(origin_for_uma,
                                              request_start_time_for_uma,
                                              FaviconAvailability::kLocal);
    std::move(local_lookup_callback).Run();
  } else {
    RecordFaviconAvailabilityAndLatencyMetric(
        origin_for_uma, request_start_time_for_uma,
        FaviconAvailability::kNotAvailable);
    std::move(empty_response_callback).Run();
  }
}

bool HistoryUiFaviconRequestHandlerImpl::CanQueryGoogleServer() const {
  return can_send_history_data_getter_.Run() &&
         base::FeatureList::IsEnabled(kEnableHistoryFaviconsGoogleServerQuery);
}

}  // namespace favicon
