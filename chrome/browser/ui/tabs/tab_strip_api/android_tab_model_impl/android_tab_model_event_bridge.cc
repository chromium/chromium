// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_model_event_bridge.h"

#include "chrome/browser/android/tab_android.h"

namespace tabs_api {

AndroidTabModelEventBridge::AndroidTabModelEventBridge(TabModel* model)
    : model_(model) {
  model_->AddObserver(this);
}

AndroidTabModelEventBridge::~AndroidTabModelEventBridge() {
  model_->RemoveObserver(this);
}

void AndroidTabModelEventBridge::AddObserver(events::EventObserver* observer) {
  observers_.AddObserver(observer);
}

void AndroidTabModelEventBridge::RemoveObserver(
    events::EventObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AndroidTabModelEventBridge::Notify(events::Event event) const {
  for (auto& observer : observers_) {
    std::visit([&observer](const auto& e) { observer.OnEvent(e.Clone()); },
               event);
  }
}

void AndroidTabModelEventBridge::DidRemoveTabForClosure(TabAndroid* tab) {
  auto event = mojom::OnNodesClosedEvent::New();
  event->node_ids.push_back(NodeId::FromTabHandle(tab->GetHandle()));
  Notify(std::move(event));
}

}  // namespace tabs_api
