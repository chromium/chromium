// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/test_helper.h"

#include <memory>
#include <string>

#include "content/public/browser/presentation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

MockWebContentsPresentationManager::MockWebContentsPresentationManager() =
    default;
MockWebContentsPresentationManager::~MockWebContentsPresentationManager() =
    default;

bool MockWebContentsPresentationManager::HasDefaultPresentationRequest() const {
  return default_presentation_request_.has_value();
}

const content::PresentationRequest&
MockWebContentsPresentationManager::GetDefaultPresentationRequest() const {
  return *default_presentation_request_;
}

void MockWebContentsPresentationManager::SetDefaultPresentationRequest(
    const content::PresentationRequest& request) {
  default_presentation_request_ = request;
}

void MockWebContentsPresentationManager::NotifyMediaRoutesChanged(
    const std::vector<media_router::MediaRoute>& routes) {
  for (auto& observer : observers_) {
    observer.OnPresentationsChanged(!routes.empty());
  }
}

void MockWebContentsPresentationManager::AddObserver(
    content::PresentationObserver* observer) {
  observers_.AddObserver(observer);
}

void MockWebContentsPresentationManager::RemoveObserver(
    content::PresentationObserver* observer) {
  observers_.RemoveObserver(observer);
}

base::WeakPtr<WebContentsPresentationManager>
MockWebContentsPresentationManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
