// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_MEMORY_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_MEMORY_H_

#include <map>
#include <string>

#include "base/optional.h"

namespace autofill_assistant {
// Data shared between scripts and actions.
class ClientMemory {
 public:
  ClientMemory();
  virtual ~ClientMemory();

  // GUID of the currently selected credit card, if any. It will be an empty
  // optional if user didn't select anything, empty string if user selected
  // 'Fill manually', or the guid of a selected card.
  virtual base::Optional<std::string> selected_card();

  // GUID of the currently selected address for |name|. It will be an empty
  // optional if user didn't select anything, empty string if user selected
  // 'Fill manually', or the guid of a selected address.
  virtual base::Optional<std::string> selected_address(const std::string& name);

  // Set the |guid| of the selected card.
  virtual void set_selected_card(const std::string& guid);

  // Set the |guid| of the selected address for |name|.
  virtual void set_selected_address(const std::string& name,
                                    const std::string& guid);

 private:
  base::Optional<std::string> selected_card_;

  // GUID of the selected addresses (keyed by name).
  std::map<std::string, std::string> selected_addresses_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_MEMORY_H_
