// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_HANDLER_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_HANDLER_H_

#include <stddef.h>

#include <optional>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/favicon/core/favicon_url.h"
#include "components/favicon_base/favicon_callback.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

class SkBitmap;

namespace favicon {

class CoreFaviconService;

// FaviconHandler works with FaviconDriver to fetch the specific type of
// favicon.
//
// FetchFavicon() requests favicons from CoreFaviconService. CoreFaviconService
// is typically backed by a database (such as HistoryService) and
// asynchronously returns results. At this point FaviconHandler only knows the
// url of the page, and not necessarily the url of the favicon(s). To ensure
// FaviconHandler handles reloading stale favicons as well as reloading a
// favicon on page reload Faviconhandler always requests the favicon from the
// CoreFaviconService regardless of whether the active favicon is valid.
//
// After the navigation two types of events are delivered (which is
// first depends upon who is faster): notification from the CoreFaviconService
// of the request for the favicon
// (OnFaviconDataForInitialURLFromFaviconService), or the favicon urls from
// the page (OnUpdateCandidates()).
// . If CoreFaviconService has a valid up to date favicon for the page, use it.
// . When OnUpdateCandidates() is called, if it matches that of the current page
//   and the current page's favicon is set, do nothing (everything is up to
//   date).
// . On the other hand, if CoreFaviconService does not know the favicon for
//   url, or the favicon is out date, or the favicon urls do not match,
//   then download the favicon. Downloading only happens once
//   CoreFaviconService has completed the request *and* the set of urls is
//   known.
//
// DownloadCurrentCandidateOrAskFaviconService() does the following:
// . If the current favicon is valid but expired, download a new icon.
// . Otherwise ask CoreFaviconService to update the mapping from page url to
//   favicon url and call us back with the favicon. Remember, it is
//   possible for the CoreFaviconService to already have the favicon, just not
//   the mapping between page and favicon url. The callback for this is
//   OnFaviconData().
//
// OnFaviconData() either updates the favicon of the current page (if
// CoreFaviconService knew about the favicon), or requests the renderer to
// download the favicon.
//
// When the renderer downloads favicons, it considers the entire list of
// favicon candidates, if `download_largest_favicon_` is true, the largest
// favicon will be used, otherwise the one that best matches the preferred size
// is chosen (or the first one if there is no preferred  size). Once the
// matching favicon has been determined, SetFavicon is called which updates
// the page's favicon and notifies the database to save the favicon.

class FaviconHandler {
 public:
  class Delegate {
   public:
    // Mimics WebContents::ImageDownloadCallback.
    using ImageDownloadCallback = base::OnceCallback<void(
        int id,
        int status_code,
        const GURL& image_url,
        const std::vector<SkBitmap>& bitmaps,
        const std::vector<gfx::Size>& original_bitmap_sizes)>;

    using ManifestDownloadCallback = base::OnceCallback<void(
        const std::vector<favicon::FaviconURL>& favicons)>;

    // Starts the download for the given favicon. When finished, the callback
    // is called with the results. Returns the unique id of the download
    // request, which will also be passed to the callback. In case of error, 0
    // is returned and no callback will be called.
    // Bitmaps with pixel sizes larger than `max_bitmap_size` are filtered out
    // from the bitmap results. If there are no bitmap results <=
    // `max_bitmap_size`, the smallest bitmap is resized to `max_bitmap_size`
    // and is the only result. A `max_bitmap_size` of 0 means unlimited.
    virtual int DownloadImage(const GURL& url,
                              int max_image_size,
                              ImageDownloadCallback callback) = 0;

    // Downloads a WebManifest and returns the favicons listed there.
    virtual void DownloadManifest(const GURL& url,
                                  ManifestDownloadCallback callback) = 0;

    // Returns whether the user is operating in an off-the-record context.
    virtual bool IsOffTheRecord() = 0;

    // Notifies that the favicon image has been updated. Most delegates
    // propagate the notification to FaviconDriverObserver::OnFaviconUpdated().
    // See its documentation for details.
    virtual void OnFaviconUpdated(
        const GURL& page_url,
        FaviconDriverObserver::NotificationIconType notification_icon_type,
        const GURL& icon_url,
        bool icon_url_changed,
        const gfx::Image& image) = 0;

    // Notifies that a page that used to have a favicon (reported via
    // OnFaviconUpdated() above) stopped having it (e.g. it was removed via
    // javascript).
    virtual void OnFaviconDeleted(
        const GURL& page_url,
        FaviconDriverObserver::NotificationIconType notification_icon_type) = 0;
  };

  // `service` may be null (which means favicons are not saved). If `service`
  // is non-null it must outlive this class. `delegate` must not be nullptr and
  // must outlive this class.
  FaviconHandler(CoreFaviconService* service,
                 Delegate* delegate,
                 FaviconDriverObserver::NotificationIconType handler_type);

  FaviconHandler(const FaviconHandler&) = delete;
  FaviconHandler& operator=(const FaviconHandler&) = delete;

  ~FaviconHandler();

  // Initiates loading the favicon for the specified url. `is_same_document` is
  // true for fragment navigations and history pushState/replaceState, see
  // NavigationHandle::IsSameDocument().
  void FetchFavicon(const GURL& page_url, bool is_same_document);

  // Collects the candidate favicons as listed in the HTML head, as well as
  // the WebManifest URL if available (or empty URL otherwise).
  void OnUpdateCandidates(const GURL& page_url,
                          const std::vector<favicon::FaviconURL>& candidates,
                          const GURL& manifest_url);

  // Returns the supported icon types, inferred from the handler type as passed
  // in the constructor.
  const favicon_base::IconTypeSet& icon_types() const { return icon_types_; }

  // For testing.
  const std::vector<GURL> GetIconURLs() const;

  // Returns whether the handler is waiting for a download to complete or for
  // data from the CoreFaviconService. Reserved for testing.
  bool HasPendingTasksForTest();

  // Get the maximal icon size in pixels for a handler of type `handler_type`.
  // `candidates_from_web_manifest` represents whether the icons are coming
  // from a Web Manifest.
  static int GetMaximalIconSize(
      FaviconDriverObserver::NotificationIconType handler_type,
      bool candidates_from_web_manifest);

 private:
  // Used to track a candidate for the favicon.
  struct FaviconCandidate {
    // Builds a scored candidate by selecting the best bitmap sizes.
    static FaviconCandidate FromFaviconURL(
        const favicon::FaviconURL& favicon_url,
        const std::vector<int>& desired_pixel_sizes,
        bool want_largest_icon);

    // Compare function used for std::stable_sort to sort in descending order.
    static bool CompareScore(const FaviconCandidate& lhs,
                             const FaviconCandidate& rhs) {
      return lhs.score > rhs.score;
    }

    GURL icon_url;
    favicon_base::IconType icon_type = favicon_base::IconType::kInvalid;
    float score = 0;
  };

  struct DownloadedFavicon {
    FaviconCandidate candidate;
    gfx::Image image;
  };

  // Returns the set of relevant favicon_base::IconType values based on the
  // handler's type.
  static favicon_base::IconTypeSet GetIconTypesFromHandlerType(
      FaviconDriverObserver::NotificationIconType handler_type);

  // Called with the result of looking up cached icon data for the manifest's
  // URL, which is used as icon URL.
  void OnFaviconDataForManifestFromFaviconService(
      const std::vector<favicon_base::FaviconRawBitmapResult>&
          favicon_bitmap_results);

  // Called when the dowloading of a manifest completes.
  void OnDidDownloadManifest(const std::vector<FaviconURL>& candidates);

  // Called when the actual list of favicon candidates to be processed is
  // available, which can be either icon URLs listed in the HTML head instead
  // or, if a Web Manifest was provided, the list of icons there.
  void OnGotFinalIconURLCandidates(const std::vector<FaviconURL>& candidates);

  // Called when the history request for favicon data mapped to `url_` has
  // completed and the renderer has told us the icon URLs used by `url_`,
  // including the case where no relevant candidates exists.
  void OnGotInitialHistoryDataAndIconURLCandidates();

  // See description above class for details.
  void OnFaviconDataForInitialURLFromFaviconService(const std::vector<
      favicon_base::FaviconRawBitmapResult>& favicon_bitmap_results);

  // If the favicon currently mapped to `url_` has expired, downloads the
  // current candidate favicon from the renderer. Otherwise requests data for
  // the current favicon from history. If data is requested from history,
  // OnFaviconData() is called with the history data once it has been retrieved.
  void DownloadCurrentCandidateOrAskFaviconService();

  // Requests the favicon for `icon_url` from the favicon service. Unless in
  // incognito, it also updates the page URL (url_) to `icon_url` mappings.
  void GetFaviconAndUpdateMappingsUnlessIncognito(
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      favicon_base::FaviconResultsCallback callback);

  // See description above class for details.
  void OnFaviconData(const std::vector<favicon_base::FaviconRawBitmapResult>&
                         favicon_bitmap_results);

  // Schedules a download for the specified entry. This adds the request to
  // image_download_requests_.
  void ScheduleImageDownload(const GURL& image_url,
                             favicon_base::IconType icon_type);

  // Triggered when a download of an image has finished. `bitmaps` and
  // `original_bitmap_sizes` must contain the same number of elements (i.e. same
  // vector size).
  void OnDidDownloadFavicon(
      favicon_base::IconType icon_type,
      int id,
      int http_status_code,
      const GURL& image_url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes);

  // Returns true if the next candidate in `final_candidates_` should be
  // downloaded.
  bool ShouldDownloadNextCandidate() const;

  // Sets the image data for the favicon.
  void SetFavicon(const GURL& icon_url,
                  const gfx::Image& image,
                  favicon_base::IconType icon_type);

  // Deletes the favicon mappings for `page_urls_`, if:
  // - We're not in incognito.
  // - A mapping is known to exist (reflected by `notification_icon_type_`).
  // - All download attempts returned 404s OR no relevant candidate was
  //   provided (as per `icon_types_`).
  void MaybeDeleteFaviconMappings();

  // Notifies `driver_` that FaviconHandler found an icon which matches the
  // `handler_type_` criteria. NotifyFaviconUpdated() can be called multiple
  // times for the same page if:
  // - a better match is found for `handler_type_` (e.g. newer bitmap data)
  // - Javascript changes the page's icon URLs.
  void NotifyFaviconUpdated(
      const std::vector<favicon_base::FaviconRawBitmapResult>&
          favicon_bitmap_results);
  void NotifyFaviconUpdated(const GURL& icon_url,
                            favicon_base::IconType icon_type,
                            const gfx::Image& image);

  // Return the current candidate if any.
  const FaviconCandidate* current_candidate() const {
    DCHECK(final_candidates_);
    return current_candidate_index_ < final_candidates_->size()
               ? &final_candidates_.value()[current_candidate_index_]
               : nullptr;
  }

  // Returns the preferred size of the image. 0 means no preference (any size
  // will do).
  int preferred_icon_size() const {
    return download_largest_icon_ ? 0 : gfx::kFaviconSize;
  }

  // Used for the GetFaviconForPageURL() request looking up the page URL,
  // triggered in FetchFavicon().
  base::CancelableTaskTracker cancelable_task_tracker_for_page_url_;

  // Used for various CoreFaviconService methods triggered while processing
  // candidates.
  base::CancelableTaskTracker cancelable_task_tracker_for_candidates_;

  const FaviconDriverObserver::NotificationIconType handler_type_;

  // URL of the page(s) we're requesting the favicon for. They can be multiple
  // in case of in-page navigations (e.g. fragment navigations).
  base::flat_set<GURL> page_urls_;
  // The last page URL reported via FetchFavicon().
  GURL last_page_url_;

  // Whether we got data back for the initial request to the CoreFaviconService.
  bool got_favicon_from_history_;

  // Whether the history data returned in
  // OnFaviconDataForInitialURLFromFaviconService() is out of date or is known
  // to be incomplete. If true, it means there is a favicon mapped to `url_` in
  // history, but we need to redownload the icon URLs because the icon in the
  // database has expired or the data in the database is incomplete.
  bool initial_history_result_expired_or_incomplete_;

  // Whether FaviconHandler should ignore history state and determine the
  // optimal icon URL out of `image_urls_` for `url_` by downloading
  // `image_urls_` one by one.
  bool redownload_icons_;

  // Requests to the renderer to download a manifest.
  base::CancelableOnceCallback<Delegate::ManifestDownloadCallback::RunType>
      manifest_download_request_;

  // Requests to the renderer to download favicons.
  base::CancelableOnceCallback<Delegate::ImageDownloadCallback::RunType>
      image_download_request_;

  // The combination of the supported icon types.
  const favicon_base::IconTypeSet icon_types_;

  // Whether the largest icon should be downloaded.
  const bool download_largest_icon_;

  // Whether candidates have been received (OnUpdateCandidates() has been
  // called, regardless of whether the provided list was empty).
  bool candidates_received_;

  // Whether among the processed candidates at least one download attempt
  // resulted in an error other than a 404.
  bool error_other_than_404_found_;

  // The manifest URL from the renderer (or empty URL if none).
  GURL manifest_url_;

  // Original list of candidates provided to OnUpdateCandidates(), stored to
  // be able to fall back to, in case a manifest was provided and downloading it
  // failed (or provided no icons).
  std::vector<FaviconURL> non_manifest_original_candidates_;

  // The prioritized favicon candidates from the page back from the renderer.
  // Populated by OnGotFinalIconURLCandidates().
  std::optional<std::vector<FaviconCandidate>> final_candidates_;

  // The icon URL and the icon type of the favicon in the most recent
  // FaviconDriver::OnFaviconAvailable() notification.
  GURL notification_icon_url_;
  favicon_base::IconType notification_icon_type_;

  // The CoreFaviconService which implements favicon operations. May be null.
  raw_ptr<CoreFaviconService> service_;

  // This handler's delegate.
  raw_ptr<Delegate> delegate_;

  // The index of the favicon URL in `image_urls_` which is currently being
  // requested from history or downloaded.
  size_t current_candidate_index_;

  // Best image we've seen so far.  As images are downloaded from the page they
  // are stored here. When a satisfying icon is found (as defined in
  // UpdateFaviconCandidate()), the favicon service and the delegate are
  // notified.
  DownloadedFavicon best_favicon_;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_HANDLER_H_
