// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_SUGGESTIONS_WATCHER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_SUGGESTIONS_WATCHER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"

namespace extensions {
namespace api {
namespace omnibox {
namespace SendSuggestions {
struct Params;
}  // namespace SendSuggestions
}  // namespace omnibox
}  // namespace api
}  // namespace extensions

// This KeyedService is meant to observe omnibox suggestions and provide
// notifications to observers on suggestion changes.
//
// This watcher is part of the Omnibox Extensions API.
class OmniboxSuggestionsWatcher : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnOmniboxSuggestionsReady(
        extensions::api::omnibox::SendSuggestions::Params* suggestions) {}

    virtual void OnOmniboxDefaultSuggestionChanged() {}
  };

  OmniboxSuggestionsWatcher();
  ~OmniboxSuggestionsWatcher() override;
  OmniboxSuggestionsWatcher(const OmniboxSuggestionsWatcher&) = delete;
  OmniboxSuggestionsWatcher& operator=(const OmniboxSuggestionsWatcher&) =
      delete;

  void NotifySuggestionsReady(
      extensions::api::omnibox::SendSuggestions::Params* suggestions);
  void NotifyDefaultSuggestionChanged();

  // Add/remove observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  base::ObserverList<Observer> observers_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_SUGGESTIONS_WATCHER_H_
