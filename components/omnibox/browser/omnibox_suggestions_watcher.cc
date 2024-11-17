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
    extensions::api::omnibox::SendSuggestions::Params* suggestions) {
  for (auto& observer : observers_)
    observer.OnOmniboxSuggestionsReady(suggestions);
}

void OmniboxSuggestionsWatcher::NotifyDefaultSuggestionChanged() {
  for (auto& observer : observers_)
    observer.OnOmniboxDefaultSuggestionChanged();
}
