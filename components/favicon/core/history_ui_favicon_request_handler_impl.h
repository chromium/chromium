// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_HISTORY_UI_FAVICON_REQUEST_HANDLER_IMPL_H_
#define COMPONENTS_FAVICON_CORE_HISTORY_UI_FAVICON_REQUEST_HANDLER_IMPL_H_

#include <map>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/history_ui_favicon_request_handler.h"
#include "components/favicon_base/favicon_types.h"

namespace favicon {

class FaviconService;
class LargeIconService;

// Where the icon sent in the response is coming from. Used for metrics.
enum class FaviconAvailability {
  // Icon recovered from local storage (but may originally come from server).
  kLocal = 0,
  // DEPRECATED: No icon is retrieved using sync in this layer anymore.
  // Icon recovered using sync.
  kDeprecatedSync = 1,
  // Icon not found.
  kNotAvailable = 2,
  kMaxValue = kNotAvailable,
};

// Implementation class for HistoryUiFaviconRequestHandler.
class HistoryUiFaviconRequestHandlerImpl
    : public HistoryUiFaviconRequestHandler {
 public:
  // Callback that checks whether user settings allow to query the favicon
  // server using history data (in particular it must check that history sync is
  // enabled and no custom passphrase is set).
  using CanSendHistoryDataGetter = base::RepeatingCallback<bool()>;

  HistoryUiFaviconRequestHandlerImpl(
      const CanSendHistoryDataGetter& can_send_history_data_getter,
      FaviconService* favicon_service,
      LargeIconService* large_icon_service);

  ~HistoryUiFaviconRequestHandlerImpl() override;

  void GetRawFaviconForPageURL(
      const GURL& page_url,
      int desired_size_in_pixel,
      favicon_base::FaviconRawBitmapCallback callback,
      HistoryUiFaviconRequestOrigin request_origin_for_uma) override;
  void GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::FaviconImageCallback callback,
      HistoryUiFaviconRequestOrigin request_origin_for_uma) override;

 private:
  // Called after the first attempt to retrieve the icon bitmap from local
  // storage. If request succeeded, sends the result. Otherwise, if allowed by
  // user settings, (i.e. if |can_send_history_data_getter_| returns true),
  // attempts to retrieve from the Google favicon server.
  void OnBitmapLocalDataAvailable(
      const GURL& page_url,
      int desired_size_in_pixel,
      favicon_base::FaviconRawBitmapCallback response_callback,
      HistoryUiFaviconRequestOrigin origin_for_uma,
      base::Time request_start_time_for_uma,
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // Called after the first attempt to retrieve the icon image from local
  // storage. If request succeeded, sends the result. Otherwise, if allowed by
  // user settings, (i.e. if |can_send_history_data_getter_| returns true),
  // attempts to retrieve from the Google favicon server.
  void OnImageLocalDataAvailable(
      const GURL& page_url,
      favicon_base::FaviconImageCallback response_callback,
      HistoryUiFaviconRequestOrigin origin_for_uma,
      base::Time request_start_time_for_uma,
      const favicon_base::FaviconImageResult& image_result);

  // Requests an icon from Google favicon server. Since requests work by
  // populating local storage, a |local_lookup_callback| will be needed in case
  // of success and an |empty_response_callback| in case of failure. Neither
  // callback is run if |this| is deleted before completion.
  void RequestFromGoogleServer(const GURL& page_url,
                               base::OnceClosure empty_response_callback,
                               base::OnceClosure local_lookup_callback,
                               HistoryUiFaviconRequestOrigin origin_for_uma,
                               base::Time request_start_time_for_uma);

  // Called once the request to the favicon server has finished. If the request
  // succeeded, |local_lookup_callback| is called to effectively retrieve the
  // icon, otherwise |empty_response_callback| is called.
  void OnGoogleServerDataAvailable(
      base::OnceClosure empty_response_callback,
      base::OnceClosure local_lookup_callback,
      HistoryUiFaviconRequestOrigin origin_for_uma,
      base::Time request_start_time_for_uma,
      favicon_base::GoogleFaviconServerRequestStatus status);

  FaviconService* const favicon_service_;

  LargeIconService* const large_icon_service_;

  CanSendHistoryDataGetter const can_send_history_data_getter_;

  // Needed for using FaviconService.
  base::CancelableTaskTracker cancelable_task_tracker_;

  base::WeakPtrFactory<HistoryUiFaviconRequestHandlerImpl> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(HistoryUiFaviconRequestHandlerImpl);
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_HISTORY_UI_FAVICON_REQUEST_HANDLER_IMPL_H_
