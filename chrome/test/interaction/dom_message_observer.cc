// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/dom_message_observer.h"

#include "base/functional/bind.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "ui/base/interaction/element_tracker.h"

DomMessageObserverBase::DomMessageObserverBase(
    ui::ElementIdentifier instrumented_webcontents)
    : queue_(ui::ElementTracker::GetElementTracker()
                 ->GetElementInAnyContext(instrumented_webcontents)
                 ->AsA<TrackedElementWebContents>()
                 ->owner()
                 ->web_contents()) {
  AddCallback();
}

void DomMessageObserverBase::AddCallback() {
  queue_.SetOnMessageAvailableCallback(base::BindOnce(
      &DomMessageObserverBase::OnMessageAvailable, base::Unretained(this)));
}

void DomMessageObserverBase::OnMessageAvailable() {
  std::string message;
  while (queue_.PopMessage(&message)) {
    OnMessageReceived(message);
  }
  AddCallback();
}

LatestDomMessageObserver::LatestDomMessageObserver(
    ui::ElementIdentifier instrumented_webcontents)
    : DomMessageObserverBase(instrumented_webcontents) {}
LatestDomMessageObserver::~LatestDomMessageObserver() = default;

void LatestDomMessageObserver::OnMessageReceived(const std::string& message) {
  OnStateObserverStateChanged(message);
}

DomMessageHistoryObserver::DomMessageHistoryObserver(
    ui::ElementIdentifier instrumented_webcontents)
    : DomMessageObserverBase(instrumented_webcontents) {}
DomMessageHistoryObserver::~DomMessageHistoryObserver() = default;

void DomMessageHistoryObserver::OnMessageReceived(const std::string& message) {
  messages_.push_back(message);
  OnStateObserverStateChanged(messages_);
}
