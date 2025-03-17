// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/history_ui_favicon_request_handler_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/image/image_png_rep.h"

namespace favicon {

namespace {

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
    bool fallback_to_host,
    favicon_base::FaviconRawBitmapCallback callback) {
  // First attempt to find the icon locally.
  favicon_service_->GetRawFaviconForPageURL(
      page_url, GetIconTypesForLocalQuery(), desired_size_in_pixel,
      fallback_to_host,
      base::BindOnce(
          &HistoryUiFaviconRequestHandlerImpl::OnBitmapLocalDataAvailable,
          weak_ptr_factory_.GetWeakPtr(), page_url, desired_size_in_pixel,
          fallback_to_host, /*response_callback=*/std::move(callback)),
      &cancelable_task_tracker_);
}

void HistoryUiFaviconRequestHandlerImpl::GetFaviconImageForPageURL(
    const GURL& page_url,
    favicon_base::FaviconImageCallback callback) {
  // First attempt to find the icon locally.
  favicon_service_->GetFaviconImageForPageURL(
      page_url,
      base::BindOnce(
          &HistoryUiFaviconRequestHandlerImpl::OnImageLocalDataAvailable,
          weak_ptr_factory_.GetWeakPtr(), page_url,
          /*response_callback=*/std::move(callback)),
      &cancelable_task_tracker_);
}

void HistoryUiFaviconRequestHandlerImpl::OnBitmapLocalDataAvailable(
    const GURL& page_url,
    int desired_size_in_pixel,
    bool fallback_to_host,
    favicon_base::FaviconRawBitmapCallback response_callback,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  if (bitmap_result.is_valid()) {
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
            GetIconTypesForLocalQuery(), desired_size_in_pixel,
            fallback_to_host, std::move(split_response_callback.second),
            &cancelable_task_tracker_));
    return;
  }

  // Send empty response.
  std::move(response_callback).Run(favicon_base::FaviconRawBitmapResult());
}

void HistoryUiFaviconRequestHandlerImpl::OnImageLocalDataAvailable(
    const GURL& page_url,
    favicon_base::FaviconImageCallback response_callback,
    const favicon_base::FaviconImageResult& image_result) {
  if (!image_result.image.IsEmpty()) {
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
            &cancelable_task_tracker_));
    return;
  }

  // Send empty response.
  std::move(response_callback).Run(favicon_base::FaviconImageResult());
}

void HistoryUiFaviconRequestHandlerImpl::RequestFromGoogleServer(
    const GURL& page_url,
    base::OnceClosure empty_response_callback,
    base::OnceClosure local_lookup_callback) {
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
              std::move(local_lookup_callback)));
}

void HistoryUiFaviconRequestHandlerImpl::OnGoogleServerDataAvailable(
    base::OnceClosure empty_response_callback,
    base::OnceClosure local_lookup_callback,
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
    std::move(local_lookup_callback).Run();
  } else {
    std::move(empty_response_callback).Run();
  }
}

}  // namespace favicon
