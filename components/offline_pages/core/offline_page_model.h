// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_MODEL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_MODEL_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_pages/core/offline_event_logger.h"
#include "components/offline_pages/core/offline_page_archive_publisher.h"
#include "components/offline_pages/core/offline_page_archiver.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_page_visuals.h"
#include "components/offline_pages/core/page_criteria.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/supports_user_data.h"
#endif

namespace offline_pages {

struct ClientId;

// Service for saving pages offline, storing the offline copy and metadata, and
// retrieving them upon request.
//
// DownloadUIAdapter instances can be optionally attached to OfflinePageModel
// using base::SupportsUserData. This is limited to Android, as
// DownloadUIAdapter is only used on Android.
//
// TODO(fgorski): Things to describe:
// * how to cancel requests and what to expect
class OfflinePageModel :
#if BUILDFLAG(IS_ANDROID)
    public base::SupportsUserData,
#endif
    public KeyedService {
 public:
  // Describes the parameters to control how to save a page.
  struct SavePageParams {
    SavePageParams();
    SavePageParams(const SavePageParams& other);
    ~SavePageParams();

    // The last committed URL of the page to save.
    GURL url;

    // The identification used by the client.
    ClientId client_id;

    // Used for the offline_id for the saved file if non-zero. If it is
    // kInvalidOfflineId, a new, random ID will be generated.
    int64_t proposed_offline_id;

    // The original URL of the page to save. Empty if no redirect occurs.
    GURL original_url;

    // Whether the page is being saved in the background.
    bool is_background;

    // The app package that the request originated from.
    std::string request_origin;
  };

  // Observer of the OfflinePageModel.
  class Observer {
   public:
    // Invoked when the model has finished loading.
    virtual void OfflinePageModelLoaded(OfflinePageModel* model) = 0;

    // Invoked when the model is being updated due to adding an offline page.
    virtual void OfflinePageAdded(OfflinePageModel* model,
                                  const OfflinePageItem& added_page) = 0;

    // Invoked when an offline copy related to |offline_id| was deleted.
    virtual void OfflinePageDeleted(const OfflinePageItem& deleted_page) = 0;

    // Invoked when a thumbnail for an offline page is added.
    virtual void ThumbnailAdded(OfflinePageModel* model,
                                int64_t offline_id,
                                const std::string& added_thumbnail_data) {}

    // Invoked when a favicon for an offline page is added.
    virtual void FaviconAdded(OfflinePageModel* model,
                              int64_t offline_id,
                              const std::string& added_favicon_data) {}

   protected:
    virtual ~Observer() = default;
  };

  using MultipleOfflinePageItemResult =
      offline_pages::MultipleOfflinePageItemResult;
  using DeletePageResult = offline_pages::DeletePageResult;
  using SavePageResult = offline_pages::SavePageResult;

  // Returns true if saving an offline page may be attempted for |url|.
  static bool CanSaveURL(const GURL& url);

  OfflinePageModel();
  ~OfflinePageModel() override;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  static const int64_t kInvalidOfflineId = 0;

  // Attempts to save a page offline per |save_page_params|. Requires that the
  // model is loaded.  Generates a new offline id or uses the proposed offline
  // id in |save_page_params| and returns it.
  //
  // Example usage:
  //   class ArchiverImpl : public OfflinePageArchiver {
  //     // This is a class that knows how to create archiver
  //     void CreateArchiver(...) override;
  //     ...
  //   }
  //
  //   // In code using the OfflinePagesModel to save a page:
  //   std::unique_ptr<ArchiverImpl> archiver(new ArchiverImpl());
  //   // Callback is of type SavePageCallback.
  //   model->SavePage(url, std::move(archiver), std::move(callback));
  //
  // TODO(crbug.com/41392683): This method's implementation shouldn't
  // take ownership of OfflinePageArchiver.
  virtual void SavePage(const SavePageParams& save_page_params,
                        std::unique_ptr<OfflinePageArchiver> archiver,
                        content::WebContents* web_contents,
                        SavePageCallback callback) = 0;

  // Adds a page entry to the metadata store.
  virtual void AddPage(const OfflinePageItem& page,
                       AddPageCallback callback) = 0;

  // Marks that the offline page related to the passed |offline_id| has been
  // accessed. Its access info, including last access time and access count,
  // will be updated. Requires that the model is loaded.
  virtual void MarkPageAccessed(int64_t offline_id) = 0;

  // Deletes pages that match |criteria|.
  virtual void DeletePagesWithCriteria(const PageCriteria& criteria,
                                       DeletePageCallback callback) = 0;

  // Deletes cached offline pages matching the URL predicate.
  virtual void DeleteCachedPagesByURLPredicate(const UrlPredicate& predicate,
                                               DeletePageCallback callback) = 0;

  // Gets all offline pages.
  virtual void GetAllPages(MultipleOfflinePageItemCallback callback) = 0;

  // Returns zero or one offline pages associated with a specified |offline_id|.
  virtual void GetPageByOfflineId(int64_t offline_id,
                                  SingleOfflinePageItemCallback callback) = 0;

  // Returns all offline pages that match |criteria|. The returned list is
  // sorted by descending creation date so that the most recent offline page
  // will be the first element of the list.
  virtual void GetPagesWithCriteria(
      const PageCriteria& criteria,
      MultipleOfflinePageItemCallback callback) = 0;

  // Gets all offline ids where the offline page has the matching client id.
  virtual void GetOfflineIdsForClientId(const ClientId& client_id,
                                        MultipleOfflineIdCallback callback) = 0;

  // Stores a new page thumbnail in the page_thumbnails table.
  virtual void StoreThumbnail(int64_t offline_id, std::string thumbnail) = 0;

  // Stores a new favicon in the page_thumbnails table.
  virtual void StoreFavicon(int64_t offline_id, std::string favicon) = 0;

  // Reads a thumbnail and favicon from the page_thumbnails table. Calls
  // callback with nullptr if the thumbnail was not found.
  virtual void GetVisualsByOfflineId(int64_t offline_id,
                                     GetVisualsCallback callback) = 0;

  // Checks if a thumbnail for a specific |offline_id| exists in the
  // page_thumbnails table. Calls callback with the bool result.
  virtual void GetVisualsAvailability(
      int64_t offline_id,
      base::OnceCallback<void(VisualsAvailability)> callback) = 0;

  // Publishes an offline page from the internal offline page directory.  This
  // includes putting it in a public directory, updating the system download
  // manager, if any, and updating the offline page model database.
  //
  // TODO(crbug.com/41392683): This method's implementation shouldn't
  // take ownership of OfflinePageArchiver.
  virtual void PublishInternalArchive(
      const OfflinePageItem& offline_page,
      PublishPageCallback publish_done_callback) = 0;

  // Get the archive directory based on client policy of the namespace.
  virtual const base::FilePath& GetArchiveDirectory(
      const std::string& name_space) const = 0;

  // Returns whether given archive file is in the internal directory.
  virtual bool IsArchiveInInternalDir(
      const base::FilePath& file_path) const = 0;

  // Returns the logger. Ownership is retained by the model.
  virtual OfflineEventLogger* GetLogger() = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_MODEL_H_
