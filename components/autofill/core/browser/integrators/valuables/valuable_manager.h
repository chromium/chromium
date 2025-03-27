// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_VALUABLES_VALUABLE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_VALUABLES_VALUABLE_MANAGER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/valuables/valuable_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"

namespace autofill {

// This class handles Valuables (e.g. Loyalty Cards) related functionality such
// as retrieving the valuable value, managing valuable-related suggestions, and
// handling form submission data when a valuable field is present.
// TODO(crbug.com/405371277): Implement all the required functionality.
class ValuableManager {
 public:
  // Callback to notify the caller of the manager when fetching the value
  // of a Loyalty Card has finished.
  using OnValuableFetchedCallback =
      base::OnceCallback<void(const std::u16string& value)>;

  ValuableManager();
  ValuableManager(const ValuableManager&) = delete;
  ValuableManager& operator=(const ValuableManager&) = delete;

  virtual ~ValuableManager();

  // Returns the full Valuable value corresponding to the `valuable_id`. Note
  // that for some Valuables, such as loyalty cards, the full card value is not
  // available in the client. As this may require a network round-trip,
  // `on_valuable_fetched` is run once the value is fetched.
  virtual void FetchValue(ValuableId valuable_id,
                          OnValuableFetchedCallback on_valuable_fetched);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_VALUABLES_VALUABLE_MANAGER_H_
