// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTEXTUALIZATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTEXTUALIZATION_CONTROLLER_H_

// Controller responsible for handling contextualization logic for Lens flows.
// This includes grabbing content related to the page and issuing Lens requests
// so searchbox requests are contextualized.
class LensSearchContextualizationController {
 public:
  LensSearchContextualizationController();
  ~LensSearchContextualizationController();

  // Internal state machine. States are mutually exclusive. Exposed for testing.
  enum class State {
    // This is the default state. The contextualization flow is not currently
    // active.
    kOff,

    // TODO(crbug.com/335516480): Implement suspended state.
    kSuspended,
  };
  State state() { return state_; }

 private:
  // The current state of the contextualization flow.
  State state_ = State::kOff;
};
#endif  // CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTEXTUALIZATION_CONTROLLER_H_
