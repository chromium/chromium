// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_ACTION_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_ACTION_UTIL_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/web/element_finder.h"

namespace autofill_assistant {
namespace element_action_util {
namespace {

template <typename T>
void RetainElementAndExecuteGetCallback(
    std::unique_ptr<ElementFinderResult> element,
    base::OnceCallback<void(const ClientStatus&, T)> callback,
    const ClientStatus& status,
    T result) {
  DCHECK(element != nullptr);
  std::move(callback).Run(status, result);
}

}  // namespace

using ElementActionCallback =
    base::OnceCallback<void(const ElementFinderResult&,
                            base::OnceCallback<void(const ClientStatus&)>)>;

using ElementActionVector = std::vector<ElementActionCallback>;

template <typename T>
using ElementActionGetCallback =
    base::OnceCallback<void(const ElementFinderResult&,
                            base::OnceCallback<void(const ClientStatus&, T)>)>;

// Run all |perform_actions| sequentially. Breaks the execution on any error
// and executes the |done| callback with the final status.
void PerformAll(std::unique_ptr<ElementActionVector> perform_actions,
                const ElementFinderResult& element,
                base::OnceCallback<void(const ClientStatus&)> done);

// Take ownership of the |element| and execute the |perform| callback with the
// element and the |done| callback as arguments, while retaining the element.
// If the initial status is not ok, execute the |done| callback immediately.
void TakeElementAndPerform(ElementActionCallback perform,
                           base::OnceCallback<void(const ClientStatus&)> done,
                           const ClientStatus& element_status,
                           std::unique_ptr<ElementFinderResult> element);

// Take ownership of the |element| and execute the |perform| callback with the
// element and the |done| callback as arguments, while retaining the element.
// If the initial status is not ok, execute the |done| callback with the default
// value immediately.
template <typename T>
void TakeElementAndGetProperty(
    ElementActionGetCallback<T> perform_and_get,
    T default_value,
    base::OnceCallback<void(const ClientStatus&, T)> done,
    const ClientStatus& element_status,
    std::unique_ptr<ElementFinderResult> element_result) {
  if (!element_status.ok()) {
    VLOG(1) << __func__ << " Failed to find element.";
    std::move(done).Run(element_status, default_value);
    return;
  }

  const ElementFinderResult* element_result_ptr = element_result.get();
  std::move(perform_and_get)
      .Run(*element_result_ptr,
           base::BindOnce(&RetainElementAndExecuteGetCallback<T>,
                          std::move(element_result), std::move(done)));
}

}  // namespace element_action_util
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_ACTION_UTIL_H_
