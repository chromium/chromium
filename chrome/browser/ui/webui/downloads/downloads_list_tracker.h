// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_LIST_TRACKER_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_LIST_TRACKER_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class DownloadManager;
}

// A class that tracks all downloads activity and keeps a sorted representation
// of the downloads as chrome://downloads wants to display them.
class DownloadsListTracker
    : public download::AllDownloadItemNotifier::Observer {
 public:
  DownloadsListTracker(content::DownloadManager* download_manager,
                       mojo::PendingRemote<downloads::mojom::Page> page);

  DownloadsListTracker(const DownloadsListTracker&) = delete;
  DownloadsListTracker& operator=(const DownloadsListTracker&) = delete;

  ~DownloadsListTracker() override;

  // Clears all downloads on the page if currently sending updates and resets
  // chunk tracking data.
  void Reset();

  // This class only cares about downloads that match |search_terms|.
  // An empty list shows all downloads (the default). Returns true if
  // |search_terms| differ from the current ones.
  bool SetSearchTerms(const std::vector<std::string>& search_terms);

  // Starts sending updates and sends a capped amount of downloads. Tracks which
  // downloads have been sent. Re-call this to send more.
  void StartAndSendChunk();

  // Stops sending updates to the page.
  void Stop();

  // Returns the number of dangerous download items that have been sent to the
  // page. Does not count those which we know about but are not yet displayed
  // on the page, e.g. due to not having scrolled far enough yet.
  // Note this includes items that have been cancelled; they still display a
  // warning in grayed out text.
  int NumDangerousItemsSent() const;

  // Returns the first dangerous item that is not cancelled, i.e. it is the
  // first (topmost) item to be shown on the page with an active warning.
  // Returns nullptr if none are found.
  download::DownloadItem* GetFirstActiveWarningItem();

  content::DownloadManager* GetMainNotifierManager() const;
  content::DownloadManager* GetOriginalNotifierManager() const;

  // AllDownloadItemNotifier::Observer:
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* download_item) override;
  void OnDownloadUpdated(content::DownloadManager* manager,
                         download::DownloadItem* download_item) override;
  void OnDownloadRemoved(content::DownloadManager* manager,
                         download::DownloadItem* download_item) override;

 protected:
  // Testing constructor.
  DownloadsListTracker(
      content::DownloadManager* download_manager,
      mojo::PendingRemote<downloads::mojom::Page> page,
      base::RepeatingCallback<bool(const download::DownloadItem&)>);

  // Creates a dictionary value that's sent to the page as JSON.
  virtual downloads::mojom::DataPtr CreateDownloadData(
      download::DownloadItem* item) const;

  // Exposed for testing.
  bool IsIncognito(const download::DownloadItem& item) const;

  const download::DownloadItem* GetItemForTesting(size_t index) const;

  void SetChunkSizeForTesting(size_t chunk_size);

 private:
  FRIEND_TEST_ALL_PREFIXES(DownloadsListTrackerTest,
                           CreateDownloadData_UrlFormatting_OmitUserPass);
  FRIEND_TEST_ALL_PREFIXES(DownloadsListTrackerTest,
                           CreateDownloadData_UrlFormatting_Idn);
  FRIEND_TEST_ALL_PREFIXES(DownloadsListTrackerTest,
                           CreateDownloadData_UrlFormatting_Long);
  FRIEND_TEST_ALL_PREFIXES(DownloadsListTrackerTest,
                           CreateDownloadData_UrlFormatting_VeryLong);
  FRIEND_TEST_ALL_PREFIXES(DownloadsListTrackerTest,
                           CreateDownloadData_ReferrerUrlPresent);
  FRIEND_TEST_ALL_PREFIXES(DownloadsListTrackerTest,
                           CreateDownloadData_ReferrerUrlNotPresent);
  FRIEND_TEST_ALL_PREFIXES(
      DownloadsListTrackerTest,
      CreateDownloadData_ReferrerUrlFormatting_OmitUserPass);
  FRIEND_TEST_ALL_PREFIXES(DownloadsListTrackerTest,
                           CreateDownloadData_ReferrerUrlFormatting_Idn);
  FRIEND_TEST_ALL_PREFIXES(DownloadsListTrackerTest,
                           CreateDownloadData_ReferrerUrlFormatting_Long);
  FRIEND_TEST_ALL_PREFIXES(DownloadsListTrackerTest,
                           CreateDownloadData_ReferrerUrlFormatting_VeryLong);
  FRIEND_TEST_ALL_PREFIXES(DownloadsListTrackerTest, RenamingProgress);

#if BUILDFLAG(FULL_SAFE_BROWSING)
  FRIEND_TEST_ALL_PREFIXES(DownloadsListTrackerTest,
                           CreateDownloadData_SafeBrowsing);
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

  struct StartTimeComparator {
    bool operator()(const download::DownloadItem* a,
                    const download::DownloadItem* b) const;
  };
  using SortedSet = std::set<raw_ptr<download::DownloadItem, SetExperimental>,
                             StartTimeComparator>;

  // Called by both constructors to initialize common state.
  void Init();

  // Clears and re-inserts all downloads items into |sorted_items_|.
  void RebuildSortedItems();

  // Whether |item| should show on the current page.
  bool ShouldShow(const download::DownloadItem& item) const;

  // Returns the index of |item| in |sorted_items_|.
  size_t GetIndex(const SortedSet::iterator& item) const;

  // Calls "insertItems" if sending updates and the page knows about |insert|.
  void InsertItem(const SortedSet::iterator& insert);

  // Calls "updateItem" if sending updates and the page knows about |update|.
  void UpdateItem(const SortedSet::iterator& update);

  // Removes the item that corresponds to |remove| and sends "removeItems"
  // if sending updates.
  void RemoveItem(const SortedSet::iterator& remove);

  // Calculates and returns the percent complete of |download_item|.
  int GetPercentComplete(download::DownloadItem* download_item) const;

  download::AllDownloadItemNotifier main_notifier_;
  std::unique_ptr<download::AllDownloadItemNotifier> original_notifier_;

  mojo::Remote<downloads::mojom::Page> page_;

  // Callback used to determine if an item should show on the page. Set to
  // |ShouldShow()| in default constructor, passed in while testing.
  base::RepeatingCallback<bool(const download::DownloadItem&)> should_show_;

  // When this is true, all changes to downloads that affect the page are sent
  // via JavaScript.
  bool sending_updates_ = false;

  SortedSet sorted_items_;

  // The number of items sent to the page so far.
  size_t sent_to_page_ = 0u;

  // The maximum number of items sent to the page at a time.
  size_t chunk_size_ = 20u;

  // Current search terms.
  std::vector<std::u16string> search_terms_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_LIST_TRACKER_H_
