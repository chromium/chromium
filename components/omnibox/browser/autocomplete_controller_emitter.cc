// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "autocomplete_controller_emitter.h"

#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_result.h"

AutocompleteControllerEmitter::AutocompleteControllerEmitter() = default;
AutocompleteControllerEmitter::~AutocompleteControllerEmitter() = default;

void AutocompleteControllerEmitter::AddObserver(
    AutocompleteController::Observer* observer) {
  observers_.AddObserver(observer);
}

void AutocompleteControllerEmitter::RemoveObserver(
    AutocompleteController::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AutocompleteControllerEmitter::OnStart(AutocompleteController* controller,
                                            const AutocompleteInput& input) {
  for (auto& observer : observers_) {
    observer.OnStart(controller, input);
  }
}

void AutocompleteControllerEmitter::OnResultChanged(
    AutocompleteController* controller,
    bool default_match_changed) {
  for (auto& observer : observers_) {
    observer.OnResultChanged(controller, default_match_changed);
  }
}

void AutocompleteControllerEmitter::OnMlScored(
    AutocompleteController* controller,
    const AutocompleteResult& result) {
  for (auto& observer : observers_)
    observer.OnMlScored(controller, result);
}
