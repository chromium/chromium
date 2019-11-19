// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/query_result_manager.h"

#include <unordered_set>
#include <utility>

#include "base/stl_util.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "content/public/browser/browser_thread.h"
#include "url/origin.h"

namespace media_router {

// MediaSinkObserver that propagates results back to |result_manager|.
// An instance of this class is associated with each registered MediaSource.
class QueryResultManager::MediaSourceMediaSinksObserver
    : public MediaSinksObserver {
 public:
  MediaSourceMediaSinksObserver(MediaCastMode cast_mode,
                                const MediaSource& source,
                                const url::Origin& origin,
                                MediaRouter* router,
                                QueryResultManager* result_manager)
      : MediaSinksObserver(router, source, origin),
        cast_mode_(cast_mode),
        source_(source),
        result_manager_(result_manager) {
    DCHECK(result_manager);
  }

  ~MediaSourceMediaSinksObserver() override {}

  // MediaSinksObserver:
  void OnSinksReceived(const std::vector<MediaSink>& result) override {
    latest_sink_ids_.clear();
    for (const MediaSink& sink : result)
      latest_sink_ids_.push_back(sink.id());

    result_manager_->SetSinksCompatibleWithSource(cast_mode_, source_, result);
    result_manager_->NotifyOnResultsUpdated();
  }

  // Returns the most recent sink IDs that were passed to |OnSinksReceived()|.
  void GetLatestSinkIds(std::vector<MediaSink::Id>* sink_ids) const {
    DCHECK(sink_ids);
    *sink_ids = latest_sink_ids_;
  }

  MediaCastMode cast_mode() const { return cast_mode_; }

 private:
  const MediaCastMode cast_mode_;
  const MediaSource source_;
  std::vector<MediaSink::Id> latest_sink_ids_;
  QueryResultManager* const result_manager_;
};

// Observes for all the available sinks.
class QueryResultManager::AnyMediaSinksObserver : public MediaSinksObserver {
 public:
  AnyMediaSinksObserver(MediaRouter* router, QueryResultManager* result_manager)
      : MediaSinksObserver(router), result_manager_(result_manager) {}

  ~AnyMediaSinksObserver() override {}

  // MediaSinksObserver:
  void OnSinksReceived(const std::vector<MediaSink>& sinks) override {
    result_manager_->UpdateSinkList(sinks);
    result_manager_->NotifyOnResultsUpdated();
  }

 private:
  QueryResultManager* const result_manager_;
};

QueryResultManager::QueryResultManager(MediaRouter* router) : router_(router) {
  DCHECK(router_);
  auto observer = std::make_unique<AnyMediaSinksObserver>(router_, this);
  observer->Init();
  sinks_observers_[MediaSource()] = std::move(observer);
}

QueryResultManager::~QueryResultManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void QueryResultManager::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void QueryResultManager::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void QueryResultManager::SetSourcesForCastMode(
    MediaCastMode cast_mode,
    const std::vector<MediaSource>& sources,
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (sources.empty()) {
    LOG(WARNING) << "SetSourcesForCastMode called with empty sources for "
                 << cast_mode;
    return;
  }
  if (!AreSourcesValidForCastMode(cast_mode, sources)) {
    LOG(WARNING) << "SetSourcesForCastMode called with invalid sources for "
                 << cast_mode;
    return;
  }

  RemoveOldSourcesForCastMode(cast_mode, sources);
  AddObserversForCastMode(cast_mode, sources, origin);
  cast_mode_sources_[cast_mode] = sources;
  NotifyOnResultsUpdated();
}

void QueryResultManager::RemoveSourcesForCastMode(MediaCastMode cast_mode) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RemoveOldSourcesForCastMode(cast_mode, std::vector<MediaSource>());
  cast_mode_sources_.erase(cast_mode);
  NotifyOnResultsUpdated();
}

CastModeSet QueryResultManager::GetSupportedCastModes() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CastModeSet modes;
  for (const auto& cast_mode_pair : cast_mode_sources_)
    modes.insert(cast_mode_pair.first);

  return modes;
}

std::unique_ptr<MediaSource> QueryResultManager::GetSourceForCastModeAndSink(
    MediaCastMode cast_mode,
    MediaSink::Id sink_id) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto sink_entry = sinks_with_sources_.find(sink_id);
  if (sink_entry == sinks_with_sources_.end())
    return nullptr;
  return GetHighestPrioritySourceForCastModeAndSink(cast_mode,
                                                    sink_entry->second);
}

std::vector<MediaSource> QueryResultManager::GetSourcesForCastMode(
    MediaCastMode cast_mode) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto& cast_mode_it = cast_mode_sources_.find(cast_mode);
  return cast_mode_it == cast_mode_sources_.end() ? std::vector<MediaSource>()
                                                  : cast_mode_it->second;
}

void QueryResultManager::RemoveOldSourcesForCastMode(
    MediaCastMode cast_mode,
    const std::vector<MediaSource>& new_sources) {
  const auto& cast_mode_it = cast_mode_sources_.find(cast_mode);
  if (cast_mode_it == cast_mode_sources_.end())
    return;

  for (const MediaSource& source : cast_mode_it->second) {
    if (!base::Contains(new_sources, source)) {
      sinks_observers_.erase(source);
      SetSinksCompatibleWithSource(cast_mode, source, std::vector<MediaSink>());
    }
  }
}

void QueryResultManager::AddObserversForCastMode(
    MediaCastMode cast_mode,
    const std::vector<MediaSource>& sources,
    const url::Origin& origin) {
  for (const MediaSource& source : sources) {
    if (!base::Contains(sinks_observers_, source)) {
      auto observer = std::make_unique<MediaSourceMediaSinksObserver>(
          cast_mode, source, origin, router_, this);
      observer->Init();
      sinks_observers_[source] = std::move(observer);
    }
  }
}

void QueryResultManager::SetSinksCompatibleWithSource(
    MediaCastMode cast_mode,
    const MediaSource& source,
    const std::vector<MediaSink>& new_sinks) {
  std::unordered_set<MediaSink::Id> new_sink_ids;
  for (const MediaSink& sink : new_sinks)
    new_sink_ids.insert(sink.id());

  // (1) Iterate through current sink set, remove cast mode from those that
  // do not appear in latest result.
  for (auto it = sinks_with_sources_.begin(); it != sinks_with_sources_.end();
       /* no-op */) {
    const MediaSink::Id& sink_id = it->first;
    CastModesWithMediaSources& sources_for_sink = it->second;
    if (!base::Contains(new_sink_ids, sink_id))
      sources_for_sink.RemoveSource(cast_mode, source);
    if (sources_for_sink.IsEmpty()) {
      sinks_with_sources_.erase(it++);
    } else {
      ++it;
    }
  }

  // (2) Add / update sinks with latest result.
  for (const MediaSink& sink : new_sinks) {
    auto sink_it = sinks_with_sources_.find(sink.id());
    if (sink_it == sinks_with_sources_.end()) {
      sink_it = sinks_with_sources_
                    .emplace(sink.id(), CastModesWithMediaSources(sink))
                    .first;
    } else {
      sink_it->second.set_sink(sink);
    }
    sink_it->second.AddSource(cast_mode, source);
  }
}

void QueryResultManager::UpdateSinkList(const std::vector<MediaSink>& sinks) {
  all_sinks_ = sinks;
}

std::unique_ptr<MediaSource>
QueryResultManager::GetHighestPrioritySourceForCastModeAndSink(
    MediaCastMode cast_mode,
    const CastModesWithMediaSources& sources_for_sink) const {
  const auto& cast_mode_it = cast_mode_sources_.find(cast_mode);
  if (cast_mode_it == cast_mode_sources_.end())
    return nullptr;

  for (const MediaSource& source : cast_mode_it->second) {
    if (sources_for_sink.HasSource(cast_mode, source))
      return std::make_unique<MediaSource>(source.id());
  }
  return nullptr;
}

bool QueryResultManager::AreSourcesValidForCastMode(
    MediaCastMode cast_mode,
    const std::vector<MediaSource>& sources) const {
  const auto& cast_mode_it = cast_mode_sources_.find(cast_mode);
  bool has_cast_mode = cast_mode_it != cast_mode_sources_.end();
  // If a source has already been registered, then it must be associated with
  // |cast_mode|.
  return std::find_if(sources.begin(), sources.end(),
                      [=](const MediaSource& source) {
                        return base::Contains(sinks_observers_, source) &&
                               (!has_cast_mode ||
                                !base::Contains(cast_mode_it->second, source));
                      }) == sources.end();
}

void QueryResultManager::NotifyOnResultsUpdated() {
  std::vector<MediaSinkWithCastModes> sinks;
  for (const auto& sink_pair : sinks_with_sources_) {
    MediaSinkWithCastModes sink_with_cast_modes(sink_pair.second.sink());
    sink_with_cast_modes.cast_modes = sink_pair.second.GetCastModes();
    sinks.push_back(sink_with_cast_modes);
  }
  for (const auto& sink : all_sinks_) {
    if (!base::Contains(sinks_with_sources_, sink.id()))
      sinks.push_back(MediaSinkWithCastModes(sink));
  }
  for (QueryResultManager::Observer& observer : observers_)
    observer.OnResultsUpdated(sinks);
}

}  // namespace media_router
