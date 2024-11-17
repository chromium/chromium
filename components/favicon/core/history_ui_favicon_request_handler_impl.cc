// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/history_ui_favicon_request_handler_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/image/image_png_rep.h"

namespace favicon {

namespace {

void RecordFaviconAvailabilityAndLatencyMetric(
    HistoryUiFaviconRequestOrigin origin_for_uma,
    FaviconAvailability availability) {
  switch (origin_for_uma) {
    case HistoryUiFaviconRequestOrigin::kHistory:
      UMA_HISTOGRAM_ENUMERATION("Sync.SyncedHistoryFaviconAvailability.HISTORY",
                                availability);
      break;
    case HistoryUiFaviconRequestOrigin::kHistorySyncedTabs:
      UMA_HISTOGRAM_ENUMERATION(
          "Sync.SyncedHistoryFaviconAvailability.SYNCED_TABS", availability);
      break;
    case HistoryUiFaviconRequestOrigin::kRecentTabs:
      UMA_HISTOGRAM_ENUMERATION(
          "Sync.SyncedHistoryFaviconAvailability.RECENTLY_CLOSED_TABS",
          availability);
      break;
  }
}

// Parameter used for local bitmap queries by page url. The url is an origin,
// and it may not have had a favicon associated with it. A trickier case is when
// it only has domain-scoped cookies, but visitors are redirected to HTTPS on
// visiting. It defaults to a HTTP scheme, but the favicon will be associated
// with the HTTPS URL and hence won't be found if we include the scheme in the
// lookup. Set `fallback_to_host`=true so the favicon database will fall back to
// matching only the hostname to have the best chance of finding a favicon.
const bool kFallbackToHost = true;

// Parameter used for local bitmap queries by page url.
favicon_base::IconTypeSet GetIconTypesForLocalQuery() {
  return {favicon_base::IconType::kFavicon, favicon_base::IconType::kTouchIcon,
          favicon_base::IconType::kTouchPrecomposedIcon,
          favicon_base::IconType::kWebManifestIcon};
}

}  // namespace

HistoryUiFaviconRequestHandlerImpl::HistoryUiFaviconRequestHandlerImpl(
    const CanSendHistoryDataGetter& can_send_history_data_getter,
    FaviconService* favicon_service,
    LargeIconService* large_icon_service)
    : favicon_service_(favicon_service),
      large_icon_service_(large_icon_service),
      can_send_history_data_getter_(can_send_history_data_getter) {
  DCHECK(favicon_service);
  DCHECK(large_icon_service);
}

HistoryUiFaviconRequestHandlerImpl::~HistoryUiFaviconRequestHandlerImpl() =
    default;

void HistoryUiFaviconRequestHandlerImpl::GetRawFaviconForPageURL(
    const GURL& page_url,
    int desired_size_in_pixel,
    favicon_base::FaviconRawBitmapCallback callback,
    HistoryUiFaviconRequestOrigin request_origin_for_uma) {
  // First attempt to find the icon locally.
  favicon_service_->GetRawFaviconForPageURL(
      page_url, GetIconTypesForLocalQuery(), desired_size_in_pixel,
      kFallbackToHost,
      base::BindOnce(
          &HistoryUiFaviconRequestHandlerImpl::OnBitmapLocalDataAvailable,
          weak_ptr_factory_.GetWeakPtr(), page_url, desired_size_in_pixel,
          /*response_callback=*/std::move(callback), request_origin_for_uma),
      &cancelable_task_tracker_);
}

void HistoryUiFaviconRequestHandlerImpl::GetFaviconImageForPageURL(
    const GURL& page_url,
    favicon_base::FaviconImageCallback callback,
    HistoryUiFaviconRequestOrigin request_origin_for_uma) {
  // First attempt to find the icon locally.
  favicon_service_->GetFaviconImageForPageURL(
      page_url,
      base::BindOnce(
          &HistoryUiFaviconRequestHandlerImpl::OnImageLocalDataAvailable,
          weak_ptr_factory_.GetWeakPtr(), page_url,
          /*response_callback=*/std::move(callback), request_origin_for_uma),
      &cancelable_task_tracker_);
}

void HistoryUiFaviconRequestHandlerImpl::OnBitmapLocalDataAvailable(
    const GURL& page_url,
    int desired_size_in_pixel,
    favicon_base::FaviconRawBitmapCallback response_callback,
    HistoryUiFaviconRequestOrigin origin_for_uma,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  if (bitmap_result.is_valid()) {
    // The icon comes from local storage now even though it may have been
    // originally retrieved from Google server.
    RecordFaviconAvailabilityAndLatencyMetric(origin_for_uma,
                                              FaviconAvailability::kLocal);
    large_icon_service_->TouchIconFromGoogleServer(bitmap_result.icon_url);
    std::move(response_callback).Run(bitmap_result);
    return;
  }

  if (can_send_history_data_getter_.Run()) {
    // base::SplitOnceCallback() is necessary here because
    // `response_callback` is needed to build both the empty response and local
    // lookup callbacks. This is safe because only one of the two is called.
    auto split_response_callback =
        base::SplitOnceCallback(std::move(response_callback));
    RequestFromGoogleServer(
        page_url,
        /*empty_response_callback=*/
        base::BindOnce(std::move(split_response_callback.first),
                       favicon_base::FaviconRawBitmapResult()),
        /*local_lookup_callback=*/
        base::BindOnce(
            base::IgnoreResult(&FaviconService::GetRawFaviconForPageURL),
            // base::Unretained() is safe here as RequestFromGoogleServer()
            // doesn't execute the callback if `this` is deleted.
            base::Unretained(favicon_service_), page_url,
            GetIconTypesForLocalQuery(), desired_size_in_pixel, kFallbackToHost,
            std::move(split_response_callback.second),
            &cancelable_task_tracker_),
        origin_for_uma);
    return;
  }

  // Send empty response.
  RecordFaviconAvailabilityAndLatencyMetric(origin_for_uma,
                                            FaviconAvailability::kNotAvailable);
  std::move(response_callback).Run(favicon_base::FaviconRawBitmapResult());
}

void HistoryUiFaviconRequestHandlerImpl::OnImageLocalDataAvailable(
    const GURL& page_url,
    favicon_base::FaviconImageCallback response_callback,
    HistoryUiFaviconRequestOrigin origin_for_uma,
    const favicon_base::FaviconImageResult& image_result) {
  if (!image_result.image.IsEmpty()) {
    // The icon comes from local storage now even though it may have been
    // originally retrieved from Google server.
    RecordFaviconAvailabilityAndLatencyMetric(origin_for_uma,
                                              FaviconAvailability::kLocal);
    large_icon_service_->TouchIconFromGoogleServer(image_result.icon_url);
    std::move(response_callback).Run(image_result);
    return;
  }

  if (can_send_history_data_getter_.Run()) {
    // base::SplitOnceCallback() is necessary here because
    // `response_callback` is needed to build both the empty response and local
    // lookup callbacks. This is safe because only one of the two is called.
    auto split_response_callback =
        base::SplitOnceCallback(std::move(response_callback));
    RequestFromGoogleServer(
        page_url,
        /*empty_response_callback=*/
        base::BindOnce(std::move(split_response_callback.first),
                       favicon_base::FaviconImageResult()),
        /*local_lookup_callback=*/
        base::BindOnce(
            base::IgnoreResult(&FaviconService::GetFaviconImageForPageURL),
            // base::Unretained() is safe here as RequestFromGoogleServer()
            // doesn't execture the callback if `this` is deleted.
            base::Unretained(favicon_service_), page_url,
            std::move(split_response_callback.second),
            &cancelable_task_tracker_),
        origin_for_uma);
    return;
  }

  // Send empty response.
  RecordFaviconAvailabilityAndLatencyMetric(origin_for_uma,
                                            FaviconAvailability::kNotAvailable);
  std::move(response_callback).Run(favicon_base::FaviconImageResult());
}

void HistoryUiFaviconRequestHandlerImpl::RequestFromGoogleServer(
    const GURL& page_url,
    base::OnceClosure empty_response_callback,
    base::OnceClosure local_lookup_callback,
    HistoryUiFaviconRequestOrigin origin_for_uma) {
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
  large_icon_service_
      ->GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          page_url,
          /*should_trim_page_url_path=*/false, traffic_annotation,
          base::BindOnce(
              &HistoryUiFaviconRequestHandlerImpl::OnGoogleServerDataAvailable,
              weak_ptr_factory_.GetWeakPtr(),
              std::move(empty_response_callback),
              std::move(local_lookup_callback), origin_for_uma));
}

void HistoryUiFaviconRequestHandlerImpl::OnGoogleServerDataAvailable(
    base::OnceClosure empty_response_callback,
    base::OnceClosure local_lookup_callback,
    HistoryUiFaviconRequestOrigin origin_for_uma,
    favicon_base::GoogleFaviconServerRequestStatus status) {
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
                                              FaviconAvailability::kLocal);
    std::move(local_lookup_callback).Run();
  } else {
    RecordFaviconAvailabilityAndLatencyMetric(
        origin_for_uma, FaviconAvailability::kNotAvailable);
    std::move(empty_response_callback).Run();
  }
}

}  // namespace favicon
