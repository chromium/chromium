// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_suggestions_watcher.h"
#include "base/observer_list.h"
#include "build/build_config.h"

OmniboxSuggestionsWatcher::OmniboxSuggestionsWatcher() = default;
OmniboxSuggestionsWatcher::~OmniboxSuggestionsWatcher() = default;

void OmniboxSuggestionsWatcher::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OmniboxSuggestionsWatcher::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OmniboxSuggestionsWatcher::NotifySuggestionsReady(
    const std::vector<ExtensionSuggestion>& suggestions,
    const int request_id,
    const std::string& extension_id) {
  for (auto& observer : observers_)
    observer.OnOmniboxSuggestionsReady(suggestions, request_id, extension_id);
}

void OmniboxSuggestionsWatcher::NotifyDefaultSuggestionChanged() {
  for (auto& observer : observers_)
    observer.OnOmniboxDefaultSuggestionChanged();
}
