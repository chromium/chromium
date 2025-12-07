// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/autocomplete_change_observer.h"

#include "chrome/browser/omnibox/autocomplete_controller_emitter_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"

AutocompleteChangeObserver::AutocompleteChangeObserver(Profile* profile) {
  scoped_observation_.Observe(
      AutocompleteControllerEmitterFactory::GetForBrowserContext(profile));
}

AutocompleteChangeObserver::~AutocompleteChangeObserver() = default;

void AutocompleteChangeObserver::Wait() {
  run_loop_.Run();
}

void AutocompleteChangeObserver::OnResultChanged(
    AutocompleteController* controller,
    bool default_match_changed) {
  if (run_loop_.running()) {
    run_loop_.Quit();
  }
}
