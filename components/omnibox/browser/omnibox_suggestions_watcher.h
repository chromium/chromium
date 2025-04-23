// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_SUGGESTIONS_WATCHER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_SUGGESTIONS_WATCHER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/extension_suggestion.h"

// This KeyedService is meant to observe omnibox suggestions and provide
// notifications to observers on suggestion changes.
//
// This watcher is part of the Omnibox Extensions API.
class OmniboxSuggestionsWatcher : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnOmniboxSuggestionsReady(
        const std::vector<ExtensionSuggestion>& suggestions,
        const int request_id,
        const std::string& extension_id) {}

    virtual void OnOmniboxDefaultSuggestionChanged() {}
  };

  OmniboxSuggestionsWatcher();
  ~OmniboxSuggestionsWatcher() override;
  OmniboxSuggestionsWatcher(const OmniboxSuggestionsWatcher&) = delete;
  OmniboxSuggestionsWatcher& operator=(const OmniboxSuggestionsWatcher&) =
      delete;

  void NotifySuggestionsReady(
      const std::vector<ExtensionSuggestion>& suggestions,
      const int request_id,
      const std::string& extension_id);
  void NotifyDefaultSuggestionChanged();

  // Add/remove observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  base::ObserverList<Observer> observers_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_SUGGESTIONS_WATCHER_H_
