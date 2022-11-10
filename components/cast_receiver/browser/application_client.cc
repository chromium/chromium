// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/public/application_client.h"

#include "base/supports_user_data.h"
#include "components/media_control/browser/media_blocker.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace cast_receiver {
namespace {

// Key in the WebContents's UserData here the instance for a given WebContents
// is stored.
const char kApplicationControlsUserDataKey[] =
    "components/cast_receiver/browser/application_client";

// Helper function to call the method on each observer
template <typename TObserver, typename TFunc, typename... TArgs>
void NotifyObservers(base::ObserverList<TObserver>& observers,
                     TFunc&& func,
                     TArgs&&... args) {
  for (auto& observer : observers) {
    (observer.*func)(args...);
  }
}

// This class acts as a wrapper around WebContents-specific classes, acting on
// them based on changes to it. Specifically, it handles connection of any
// cross-process mojo APIs.
class ApplicationControlsImpl : public ApplicationClient::ApplicationControls,
                                public base::SupportsUserData::Data {
 public:
  explicit ApplicationControlsImpl(content::WebContents& web_contents)
      : media_blocker_(&web_contents) {}
  ~ApplicationControlsImpl() override = default;

  media_control::MediaBlocker& GetMediaBlocker() override {
    return media_blocker_;
  }

 private:
  media_control::MediaBlocker media_blocker_;
};

}  // namespace

ApplicationClient::ApplicationControls::~ApplicationControls() = default;

ApplicationClient::ApplicationClient() : weak_factory_(this) {}

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

void ApplicationClient::OnWebContentsCreated(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  web_contents->SetUserData(
      &kApplicationControlsUserDataKey,
      std::make_unique<ApplicationControlsImpl>(*web_contents));
}

void ApplicationClient::OnStreamingResolutionChanged(
    const gfx::Rect& size,
    const media::VideoTransformation& transformation) {
  NotifyObservers(streaming_resolution_observer_list_,
                  &StreamingResolutionObserver::OnStreamingResolutionChanged,
                  size, transformation);
}

void ApplicationClient::OnForegroundApplicationChanged(
    RuntimeApplication* app) {
  NotifyObservers(application_state_observer_list_,
                  &ApplicationStateObserver::OnForegroundApplicationChanged,
                  app);
}

ApplicationClient::ApplicationControls&
ApplicationClient::GetApplicationControls(
    const content::WebContents& web_contents) {
  ApplicationControlsImpl* instance = static_cast<ApplicationControlsImpl*>(
      web_contents.GetUserData(&kApplicationControlsUserDataKey));
  DCHECK(instance);
  return *instance;
}

}  // namespace cast_receiver
