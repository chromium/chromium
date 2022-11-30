// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMANDER_COMMANDER_BACKEND_H_
#define CHROME_BROWSER_UI_COMMANDER_COMMANDER_BACKEND_H_

#include <string>

#include "base/callback.h"

class Browser;

namespace commander {

struct CommanderViewModel;

// An abstract interface for an object that responds to input from the UI layer
// and responds with a view model representing results or some action.
class CommanderBackend {
 public:
  using ViewModelUpdateCallback =
      base::RepeatingCallback<void(CommanderViewModel view_model)>;
  CommanderBackend() = default;
  virtual ~CommanderBackend() = default;

  // Called when user input changes. |text| is the full contents of
  // the textfield. |browser| is the browser the commander UI is currently
  // attached to.
  virtual void OnTextChanged(const std::u16string& text, Browser* browser) = 0;
  // Called when the user has selected (clicked or pressed enter) the option at
  // |command_index| from the result set identified by |result_set_id|.
  // If |result_set_id| is stale due to race conditions, this is a no-op to
  // ensure that we don't perform an action the user didn't intend.
  virtual void OnCommandSelected(size_t command_index, int result_set_id) = 0;
  // Called when the user has cancelled entering a composite command. This
  // should have the effect of returning the backend to the state it was in
  // previous to the composite command being selected.
  virtual void OnCompositeCommandCancelled() {}
  // Sets the callback to be used when a fresh view model is ready to be
  // displayed. Invocations of the callback are not necessarily 1:1 or
  // synchronous with user input, since some command sources may be async or
  // provide incremental results.
  virtual void SetUpdateCallback(ViewModelUpdateCallback callback) = 0;
  // Called when the UI layer is closed. This is a hook to allow the backend
  // to release any bound state.
  virtual void Reset() {}
};

}  // namespace commander

#endif  // CHROME_BROWSER_UI_COMMANDER_COMMANDER_BACKEND_H_
