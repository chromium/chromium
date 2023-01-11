// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/discovery/media_sink_service_base.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "components/media_router/common/media_route.h"

#include <vector>

namespace {
// Timeout amount for |discovery_timer_|.
const constexpr base::TimeDelta kDiscoveryTimeout = base::Seconds(3);
}  // namespace

namespace media_router {

MediaSinkServiceBase::MediaSinkServiceBase(
    const OnSinksDiscoveredCallback& callback)
    : discovery_timer_(std::make_unique<base::OneShotTimer>()),
      on_sinks_discovered_cb_(callback) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

MediaSinkServiceBase::~MediaSinkServiceBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MediaSinkServiceBase::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void MediaSinkServiceBase::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

const base::flat_map<MediaSink::Id, MediaSinkInternal>&
MediaSinkServiceBase::GetSinks() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sinks_;
}

const MediaSinkInternal* MediaSinkServiceBase::GetSinkById(
    const MediaSink::Id& sink_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = sinks_.find(sink_id);
  return it != sinks_.end() ? &it->second : nullptr;
}

const MediaSinkInternal* MediaSinkServiceBase::GetSinkByRoute(
    const MediaRoute& route) const {
  return GetSinkById(route.media_sink_id());
}

void MediaSinkServiceBase::AddOrUpdateSink(const MediaSinkInternal& sink) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sinks_.insert_or_assign(sink.sink().id(), sink);
  for (auto& observer : observers_)
    observer.OnSinkAddedOrUpdated(sink);

  StartTimer();
}

void MediaSinkServiceBase::RemoveSink(const MediaSinkInternal& sink) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveSinkById(sink.sink().id());
}

void MediaSinkServiceBase::RemoveSinkById(const MediaSink::Id& sink_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = sinks_.find(sink_id);
  if (it == sinks_.end())
    return;

  MediaSinkInternal sink = std::move(it->second);
  sinks_.erase(it);
  for (auto& observer : observers_)
    observer.OnSinkRemoved(sink);

  StartTimer();
}

void MediaSinkServiceBase::SetTimerForTest(
    std::unique_ptr<base::OneShotTimer> timer) {
  discovery_timer_ = std::move(timer);
}

void MediaSinkServiceBase::AddSinkForTest(const MediaSinkInternal& sink) {
  sinks_.insert_or_assign(sink.sink().id(), sink);
}

void MediaSinkServiceBase::StartTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (discovery_timer_->IsRunning())
    return;

  discovery_timer_->Start(
      FROM_HERE, kDiscoveryTimeout,
      base::BindOnce(&MediaSinkServiceBase::OnDiscoveryComplete,
                     base::Unretained(this)));
}

void MediaSinkServiceBase::OnDiscoveryComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  discovery_timer_->Stop();
  RecordDeviceCounts();

  // Only send discovered sinks back to MediaRouter if the list changed.
  if (sinks_ == previous_sinks_) {
    DVLOG(2) << "No update to sink list.";
    return;
  }

  DVLOG(2) << "Send sinks to media router, [size]: " << sinks_.size();

  std::vector<MediaSinkInternal> sinks;
  for (const auto& sink_it : sinks_)
    sinks.push_back(sink_it.second);

  for (auto& observer : observers_)
    observer.OnSinksDiscovered(sinks);
  on_sinks_discovered_cb_.Run(std::move(sinks));
  previous_sinks_ = sinks_;
}

}  // namespace media_router
