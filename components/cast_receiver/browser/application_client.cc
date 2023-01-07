// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/public/application_client.h"

namespace cast_receiver {
namespace {

// Helper function to call the method on each observer
template <typename TObserver, typename TFunc, typename... TArgs>
void NotifyObservers(base::ObserverList<TObserver>& observers,
                     TFunc&& func,
                     TArgs&&... args) {
  for (auto& observer : observers) {
    (observer.*func)(args...);
  }
}

}  // namespace

ApplicationClient::ApplicationClient() = default;

ApplicationClient::~ApplicationClient() = default;

void ApplicationClient::AddStreamingResolutionObserver(
    StreamingResolutionObserver* observer) {
  streaming_resolution_observer_list_.AddObserver(observer);
}

void ApplicationClient::RemoveStreamingResolutionObserver(
    StreamingResolutionObserver* observer) {
  streaming_resolution_observer_list_.RemoveObserver(observer);
}

void ApplicationClient::AddApplicationStateObserver(
    ApplicationStateObserver* observer) {
  application_state_observer_list_.AddObserver(observer);
}

void ApplicationClient::RemoveApplicationStateObserver(
    ApplicationStateObserver* observer) {
  application_state_observer_list_.RemoveObserver(observer);
}

void ApplicationClient::OnStreamingResolutionChanged(
    const gfx::Rect& size,
    const media::VideoTransformation& transformation) {
  NotifyObservers(streaming_resolution_observer_list_,
                  &StreamingResolutionObserver::OnStreamingResolutionChanged,
                  size, transformation);
}

void ApplicationClient::OnForegroundApplicationChanged(
    chromecast::RuntimeApplication* app) {
  NotifyObservers(application_state_observer_list_,
                  &ApplicationStateObserver::OnForegroundApplicationChanged,
                  app);
}

}  // namespace cast_receiver
