// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_QUERY_RESULT_MANAGER_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_QUERY_RESULT_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/media_router/cast_modes_with_media_sources.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/media_sink_with_cast_modes.h"
#include "chrome/browser/ui/media_router/media_sink_with_cast_modes_observer.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"

namespace url {
class Origin;
}  // namespace url

namespace media_router {

class MediaRouter;
class MediaSinksObserver;

// The Media Router dialog allows the user to initiate casting using one of
// several actions (each represented by a cast mode).  Each cast mode is
// associated with a vector of media sources.  This class allows the dialog to
// receive lists of MediaSinks compatible with the cast modes available through
// the dialog.
//
// Typical use:
//
//   url::Origin origin{GURL("https://origin.com")};
//   MediaSinkWithCastModesObserver* observer = ...;
//   QueryResultManager result_manager(router);
//   result_manager.AddObserver(observer);
//   result_manager.SetSourcesForCastMode(MediaCastMode::PRESENTATION,
//       {MediaSourceForPresentationUrl("http://google.com")}, origin);
//   result_manager.SetSourcesForCastMode(MediaCastMode::TAB_MIRROR,
//       {MediaSourceForTab(123)}, origin);
//   ...
//   [Updates will be received by observer via OnSinksUpdated()]
//   ...
//   [When info on MediaSource is needed, i.e. when requesting route for a mode]
//   CastModeSet cast_modes = result_manager.GetSupportedCastModes();
//   [Logic to select a MediaCastMode from the set]
//   std::unique_ptr<MediaSource> source =
//       result_manager.GetSourceForCastModeAndSink(
//           MediaCastMode::TAB_MIRROR, sink_of_interest);
//   if (source) {
//     ...
//   }
//
// Not thread-safe.  Must be used on the UI thread.
class QueryResultManager {
 public:
  explicit QueryResultManager(MediaRouter* media_router);

  QueryResultManager(const QueryResultManager&) = delete;
  QueryResultManager& operator=(const QueryResultManager&) = delete;

  ~QueryResultManager();

  // Adds/removes an observer that is notified with query results.
  void AddObserver(MediaSinkWithCastModesObserver* observer);
  void RemoveObserver(MediaSinkWithCastModesObserver* observer);

  // Requests a list of MediaSinks compatible with |sources| for |cast_mode|
  // from |origin|. |sources| should be in descending order of priority.
  // Results are sent to all observers registered with AddObserver().
  //
  // Starts new queries in the Media Router for sources that we have no existing
  // queries for, and stops queries for sources no longer associated with any
  // cast mode.
  //
  // If |sources| is empty or contains a source that has already been registered
  // with another cast mode, no new queries are begun.
  void SetSourcesForCastMode(MediaCastMode cast_mode,
                             const std::vector<MediaSource>& sources,
                             const url::Origin& origin);

  // Stops notifying observers for |cast_mode|, and removes it from the set of
  // supported cast modes.
  void RemoveSourcesForCastMode(MediaCastMode cast_mode);

  // Gets the set of cast modes that are being actively queried.
  CastModeSet GetSupportedCastModes() const;

  // Gets the highest-priority source for the cast mode that is supported by
  // the sink. Returns an empty unique_ptr if there isn't any.
  std::unique_ptr<MediaSource> GetSourceForCastModeAndSink(
      MediaCastMode cast_mode,
      MediaSink::Id sink_id) const;

  // Returns all the sources registered for |cast_mode|.  Returns an empty
  // vector if there is none.
  std::vector<MediaSource> GetSourcesForCastMode(MediaCastMode cast_mode) const;

  // Returns all of the currently known sinks with the cast modes they support
  std::vector<MediaSinkWithCastModes> GetSinksWithCastModes() const;

 private:
  class MediaSourceMediaSinksObserver;
  class AnyMediaSinksObserver;

  FRIEND_TEST_ALL_PREFIXES(QueryResultManagerTest, Observers);
  FRIEND_TEST_ALL_PREFIXES(QueryResultManagerTest, StartRoutesDiscovery);
  FRIEND_TEST_ALL_PREFIXES(QueryResultManagerTest, MultipleQueries);
  FRIEND_TEST_ALL_PREFIXES(QueryResultManagerTest, MultipleUrls);
  FRIEND_TEST_ALL_PREFIXES(QueryResultManagerTest, AddInvalidSource);

  // Stops and destroys the MediaSinksObservers for media sources that
  // |cast_mode| used to support, but isn't in |new_sources|, and disassociates
  // them from sinks.
  void RemoveOldSourcesForCastMode(MediaCastMode cast_mode,
                                   const std::vector<MediaSource>& new_sources);

  // Creates observers and starts queries for each source in |sources| that
  // doesn't already have an associated observer.
  void AddObserversForCastMode(MediaCastMode cast_mode,
                               const std::vector<MediaSource>& sources,
                               const url::Origin& origin);

  // Modifies the set of sinks compatible with |cast_mode| and |source|
  // to |new_sinks|.
  void SetSinksCompatibleWithSource(MediaCastMode cast_mode,
                                    const MediaSource& source,
                                    const std::vector<MediaSink>& new_sinks);

  // Updates the overall list of sinks to match |sinks|.
  void UpdateSinkList(const std::vector<MediaSink>& sinks);

  // Returns the highest-priority source for |cast_mode| contained in
  // |sources_for_sink|. Returns an empty unique_ptr if none exists.
  std::unique_ptr<MediaSource> GetHighestPrioritySourceForCastModeAndSink(
      MediaCastMode cast_mode,
      const CastModesWithMediaSources& sources_for_sink) const;

  // Returns true if every source in |sources| is either not registered yet, or
  // associated with |cast_mode|. This check prevents a source from being
  // associated with two cast modes.
  bool AreSourcesValidForCastMode(
      MediaCastMode cast_mode,
      const std::vector<MediaSource>& sources) const;

  // Notifies observers that results have been updated.
  void NotifyOnResultsUpdated();

  // MediaSinksObservers that listen for compatible MediaSink updates.
  // Each observer is associated with a MediaSource. Results received by
  // observers are propagated back to this class.
  // A nullopt for the MediaSource indicates that the observer is
  // listening for all MediaSink updates regardless of the MediaSource
  // associated with them.
  std::map<std::optional<MediaSource>,
           std::unique_ptr<MediaSinksObserver>,
           MediaSource::Cmp>
      sinks_observers_;

  // Holds registrations of MediaSources for cast modes.
  std::map<MediaCastMode, std::vector<MediaSource>> cast_mode_sources_;

  // Holds sinks along with the cast modes and sources they support.
  std::map<MediaSink::Id, CastModesWithMediaSources> sinks_with_sources_;

  // Holds all known sinks, including those that do not support the sources in
  // |cast_mode_sources_|.
  // NOTE: Not all Media Route Providers support sink queries with an empty
  // source, so |all_sinks_| may be missing some sinks that
  // |sinks_with_sources_| has. Therefore the two collections must be tracked
  // separately for now. crbug.com/929937 tracks this.
  std::vector<MediaSink> all_sinks_;

  // Registered observers.
  base::ObserverList<MediaSinkWithCastModesObserver> observers_;

  // Not owned by this object.
  const raw_ptr<MediaRouter> router_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_QUERY_RESULT_MANAGER_H_
