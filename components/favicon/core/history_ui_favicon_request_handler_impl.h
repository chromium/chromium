// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_HISTORY_UI_FAVICON_REQUEST_HANDLER_IMPL_H_
#define COMPONENTS_FAVICON_CORE_HISTORY_UI_FAVICON_REQUEST_HANDLER_IMPL_H_

#include <map>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/favicon/core/history_ui_favicon_request_handler.h"
#include "components/favicon_base/favicon_types.h"

namespace favicon {

class FaviconService;
class LargeIconService;

// Where the icon sent in the response is coming from. Used for metrics.
enum class FaviconAvailability {
  // Icon recovered from local storage (but may originally come from server).
  kLocal = 0,
  // Icon recovered using sync.
  kSync = 1,
  // Icon not found.
  kNotAvailable = 2,
  kMaxValue = kNotAvailable,
};

// Implementation class for HistoryUiFaviconRequestHandler.
class HistoryUiFaviconRequestHandlerImpl
    : public HistoryUiFaviconRequestHandler {
 public:
  // Callback that requests the synced bitmap for a page url.
  using SyncedFaviconGetter =
      base::RepeatingCallback<favicon_base::FaviconRawBitmapResult(
          const GURL&)>;

  // Callback that checks whether user settings allow to query the favicon
  // server using history data (in particular it must check that history sync is
  // enabled and no custom passphrase is set).
  using CanSendHistoryDataGetter = base::RepeatingCallback<bool()>;

  HistoryUiFaviconRequestHandlerImpl(
      const SyncedFaviconGetter& synced_favicon_getter,
      const CanSendHistoryDataGetter& can_send_history_data_getter,
      FaviconService* favicon_service,
      LargeIconService* large_icon_service);

  ~HistoryUiFaviconRequestHandlerImpl() override;

  void GetRawFaviconForPageURL(
      const GURL& page_url,
      int desired_size_in_pixel,
      favicon_base::FaviconRawBitmapCallback callback,
      FaviconRequestPlatform request_platform,
      HistoryUiFaviconRequestOrigin request_origin_for_uma,
      const GURL& icon_url_for_uma,
      base::CancelableTaskTracker* tracker) override;

  void GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::FaviconImageCallback callback,
      HistoryUiFaviconRequestOrigin request_origin_for_uma,
      const GURL& icon_url_for_uma,
      base::CancelableTaskTracker* tracker) override;

 private:
  // Called after the first attempt to retrieve the icon bitmap from local
  // storage. If request succeeded, sends the result. Otherwise attempts to
  // retrieve from sync or the Google favicon server depending whether
  // |favicon::kEnableHistoryFaviconsGoogleServerQuery| is enabled.
  // TODO(https://crbug.com/978775): Stop plumbing can_query_google_server once
  // the feature experiment is no longer being run.
  void OnBitmapLocalDataAvailable(
      bool can_query_google_server,
      const GURL& page_url,
      int desired_size_in_pixel,
      favicon_base::FaviconRawBitmapCallback response_callback,
      FaviconRequestPlatform platform,
      HistoryUiFaviconRequestOrigin origin_for_uma,
      const GURL& icon_url_for_uma,
      base::Time request_start_time_for_uma,
      base::CancelableTaskTracker* tracker,
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // Called after the first attempt to retrieve the icon image from local
  // storage. If request succeeded, sends the result. Otherwise attempts to
  // retrieve from sync or the Google favicon server depending whether
  // |favicon::kEnableHistoryFaviconsGoogleServerQuery| is enabled.
  void OnImageLocalDataAvailable(
      bool can_query_google_server,
      const GURL& page_url,
      favicon_base::FaviconImageCallback response_callback,
      HistoryUiFaviconRequestOrigin origin_for_uma,
      const GURL& icon_url_for_uma,
      base::Time request_start_time_for_uma,
      base::CancelableTaskTracker* tracker,
      const favicon_base::FaviconImageResult& image_result);

  // Requests an icon from Google favicon server. Since requests work by
  // populating local storage, a |local_lookup_callback| will be needed in case
  // of success and an |empty_response_callback| in case of failure.
  void RequestFromGoogleServer(const GURL& page_url,
                               base::OnceClosure empty_response_callback,
                               base::OnceClosure local_lookup_callback,
                               HistoryUiFaviconRequestOrigin origin_for_uma,
                               const GURL& icon_url_for_uma,
                               base::Time request_start_time_for_uma);

  // Called once the request to the favicon server has finished. If the request
  // succeeded, |local_lookup_callback| is called to effectively retrieve the
  // icon, otherwise |empty_response_callback| is called. If |group_to_clear|
  // is non-empty, records the size of the associated group as UMA and clears
  // it, simulating the execution of its array of waiting callbacks.
  void OnGoogleServerDataAvailable(
      base::OnceClosure empty_response_callback,
      base::OnceClosure local_lookup_callback,
      HistoryUiFaviconRequestOrigin origin_for_uma,
      const GURL& group_to_clear,
      base::Time request_start_time_for_uma,
      favicon_base::GoogleFaviconServerRequestStatus status);

  bool CanQueryGoogleServer() const;

  FaviconService* const favicon_service_;

  LargeIconService* const large_icon_service_;

  SyncedFaviconGetter const synced_favicon_getter_;

  CanSendHistoryDataGetter const can_send_history_data_getter_;

  // Map from a group identifier to the number of callbacks in that group which
  // would be waiting for execution. Used for recording metrics for the possible
  // benefit of grouping.
  std::map<GURL, int> group_callbacks_count_;

  base::WeakPtrFactory<HistoryUiFaviconRequestHandlerImpl> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(HistoryUiFaviconRequestHandlerImpl);
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_HISTORY_UI_FAVICON_REQUEST_HANDLER_IMPL_H_
