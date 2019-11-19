// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CONTROLLER_EMITTER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CONTROLLER_EMITTER_H_

#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_controller_delegate.h"

#if !defined(OS_IOS)
#include "content/public/browser/browser_context.h"
#endif  // !defined(OS_IOS)

// Collects logs of all autocomplete queries and responses for a given profile
// and notifies observers (chrome://omnibox debug page).
class OmniboxControllerEmitter : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when new autocomplete queries are made from the omnibox
    // controller or when those queries' results change.
    virtual void OnOmniboxQuery(AutocompleteController* controller,
                                const AutocompleteInput& input) = 0;
    virtual void OnOmniboxResultChanged(bool default_match_changed,
                                        AutocompleteController* controller) = 0;
  };

#if !defined(OS_IOS)
  static OmniboxControllerEmitter* GetForBrowserContext(
      content::BrowserContext* browser_context);
#endif  // !defined(OS_IOS)

  OmniboxControllerEmitter();
  ~OmniboxControllerEmitter() override;

  // Add/remove observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notifies registered observers when new autocomplete queries are made from
  // the omnibox controller or when those queries' results change.
  void NotifyOmniboxQuery(AutocompleteController* controller,
                          const AutocompleteInput& input);
  void NotifyOmniboxResultChanged(bool default_match_changed,
                                  AutocompleteController* controller);

 private:
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxControllerEmitter);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CONTROLLER_EMITTER_H_
