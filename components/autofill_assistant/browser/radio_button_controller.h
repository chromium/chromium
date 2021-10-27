// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_RADIO_BUTTON_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_RADIO_BUTTON_CONTROLLER_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/user_model.h"

namespace autofill_assistant {

class RadioButtonController {
 public:
  RadioButtonController(UserModel* user_model);
  ~RadioButtonController();
  RadioButtonController(const RadioButtonController&) = delete;
  RadioButtonController& operator=(const RadioButtonController&) = delete;

  base::WeakPtr<RadioButtonController> GetWeakPtr();

  // Adds |model_identifier| to the list of model identifiers belonging to
  // |radio_group|. Creates |radio_group| if necessary.
  void AddRadioButtonToGroup(const std::string& radio_group,
                             const std::string& model_identifier);

  // Removes |model_identifier| from the list of model identifiers belonging to
  // |radio_group|. Does nothing if |radio_group| or |model_identifier| do not
  // exist.
  void RemoveRadioButtonFromGroup(const std::string& radio_group,
                                  const std::string& model_identifier);

  // Ensures that only |selected_model_identifier| is set to true in
  // |radio_group|. Does nothing if |radio_group| or |selected_model_identifier|
  // do not exist.
  bool UpdateRadioButtonGroup(const std::string& radio_group,
                              const std::string& selected_model_identifier);

 private:
  // Maps radiogroup identifiers to the list of corresponding model identifiers.
  std::map<std::string, std::set<std::string>> radio_groups_;

 private:
  friend class RadioButtonControllerTest;

  UserModel* user_model_;
  base::WeakPtrFactory<RadioButtonController> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_RADIO_BUTTON_CONTROLLER_H_
