// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_H_

#include <string>

#include "base/functional/callback.h"

namespace autofill {
struct FormFieldData;
}  // namespace autofill

namespace compose {

// The interface for embedder-independent, tab-specific compose logic.
class ComposeManager {
 public:
  // The callback to Autofill. When run, it fills the passed string into the
  // form field on which it was triggered.
  using ComposeCallback = base::OnceCallback<void(const std::u16string&)>;

  virtual ~ComposeManager() = default;

  // Trigger methods for the compose offer.
  enum class TriggerMethod {
    kAutofillPopup,
    kContextMenu,
  };
  // Returns whether compose is available for this `trigger_method and
  // `trigger_field`.
  virtual bool ShouldOfferCompose(
      TriggerMethod trigger_method,
      const autofill::FormFieldData& trigger_field) = 0;

  virtual void OpenCompose(ComposeCallback callback) = 0;
};

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_H_
