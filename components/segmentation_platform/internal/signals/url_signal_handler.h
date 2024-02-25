// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_URL_SIGNAL_HANDLER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_URL_SIGNAL_HANDLER_H_

#include "base/containers/flat_set.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation_traits.h"
#include "base/sequence_checker.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

namespace segmentation_platform {

class UkmDatabase;

// The URL signals from various sources will go through this signal handler
// before being written to the database.
class UrlSignalHandler {
 public:
  using FindCallback = base::OnceCallback<void(bool, const std::string&)>;

  // Delegate class, usually one per each HistoryService, used for checking if
  // an URL is part of the history database on-demand.
  class HistoryDelegate {
   public:
    // Fast check in local cache for the given URL. This method is optional,
    // only used for optimization. Always return false is a valid
    // implementation.
    virtual bool FastCheckUrl(const GURL& url);
    // Query the history database to check if the |url| is part of the database.
    virtual void FindUrlInHistory(const GURL& url, FindCallback callback) = 0;
    // Getters for getting profile id.
    virtual const std::string& profile_id() = 0;

    virtual ~HistoryDelegate() = default;
  };

  explicit UrlSignalHandler(UkmDatabase* ukm_database);
  ~UrlSignalHandler();

  UrlSignalHandler(const UrlSignalHandler&) = delete;
  UrlSignalHandler& operator=(const UrlSignalHandler&) = delete;

  // Called by UKM observer when source URL for the |source_id| is updated.
  void OnUkmSourceUpdated(ukm::SourceId source_id,
                          const std::vector<GURL>& urls);

  // Called by history service when a visit is added for the |url|. This
  // notification should mean that the |url| will be stored in history database
  // URL table until it is removed by OnUrlsRemovedFromHistory().
  void OnHistoryVisit(const GURL& url, const std::string& profile_id);

  // Called when |urls| are removed from the history database.
  void OnUrlsRemovedFromHistory(const std::vector<GURL>& urls, bool all_urls);

  // Add/Remove history delegates.
  void AddHistoryDelegate(HistoryDelegate* history_delegate);
  void RemoveHistoryDelegate(HistoryDelegate* history_delegate);

 private:
  // Checks each history delegate if the |url| is in the database.
  void CheckHistoryForUrl(const GURL& url, FindCallback callback);

  // Used as a callback to get result from the delegate. |delegates_checked| is
  // used to store the list of delegates already checked, to continue checking
  // remaining delegates.
  void ContinueCheckingHistory(
      const GURL& url,
      std::unique_ptr<base::flat_set<HistoryDelegate*>> delegates_checked,
      FindCallback callback,
      bool found,
      const std::string& profile_id);

  // Called when finished checking all the history delegates.
  void OnCheckedHistory(ukm::SourceId source_id,
                        const GURL& url,
                        bool in_history,
                        const std::string& profile_id);

  raw_ptr<UkmDatabase> ukm_database_;

  base::flat_set<raw_ptr<HistoryDelegate>> history_delegates_;
  // When true, the handler only does a fast check and skips the database
  // checks. It is possible to miss URLs in UKM database if this was true.
  bool skip_history_db_check_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<UrlSignalHandler> weak_factory_{this};
};

}  // namespace segmentation_platform

namespace base {

template <>
struct ScopedObservationTraits<
    segmentation_platform::UrlSignalHandler,
    segmentation_platform::UrlSignalHandler::HistoryDelegate> {
  static void AddObserver(
      segmentation_platform::UrlSignalHandler* source,
      segmentation_platform::UrlSignalHandler::HistoryDelegate* observer) {
    source->AddHistoryDelegate(observer);
  }
  static void RemoveObserver(
      segmentation_platform::UrlSignalHandler* source,
      segmentation_platform::UrlSignalHandler::HistoryDelegate* observer) {
    source->RemoveHistoryDelegate(observer);
  }
};

}  // namespace base

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_URL_SIGNAL_HANDLER_H_
