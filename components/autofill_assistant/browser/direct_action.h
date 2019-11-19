// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DIRECT_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DIRECT_ACTION_H_

#include <set>
#include <string>
#include <vector>

namespace autofill_assistant {

class DirectActionProto;

// Definition of a direct action to which a UserAction can be mapped.
//
// A direct action is an user action that originates not from the UI but instead
// from some other app or tool. This corresponds to Android's direct actions,
// first introduced in Android Q.
struct DirectAction {
  DirectAction();
  DirectAction(const DirectAction&);
  ~DirectAction();

  explicit DirectAction(const DirectActionProto& proto);

  bool empty() const { return names.empty(); }

  // Names of the direct action under which this action is available. Optional.
  std::set<std::string> names;

  // Arguments that must be set to run the direct action.
  std::vector<std::string> required_arguments;

  // Arguments that might be set to run the direct action.
  std::vector<std::string> optional_arguments;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DIRECT_ACTION_H_
