// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_BAD_MESSAGE_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_BAD_MESSAGE_H_

#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/unique_ids.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace autofill {

class FormData;

namespace bad_message {

namespace internal {

// Returns `true` if the predicate `pred` is `true` for all arguments that it
// can be applied to.
template <typename Pred, typename... Args>
bool ValidateArguments(Pred&& pred, const Args&... args) {
  auto recursion_helper = [&pred](const auto& arg) {
    if constexpr (requires { std::invoke(pred, arg); }) {
      return std::invoke(pred, arg);
    }
    return true;
  };
  return (recursion_helper(args) && ...);
}

// Returns true if `trigger_source` is a trigger source that may be used in
// renderer -> browser communication. Kills the renderer and returns false
// otherwise.
bool CheckSingleValidTriggerSource(
    AutofillSuggestionTriggerSource trigger_source);

template <typename... Args>
bool CheckValidTriggerSource(const Args&... args) {
  return ValidateArguments(&CheckSingleValidTriggerSource, args...);
}

// Returns true if `form.fields` contains a field identified by `field_id`.
// Kills the renderer otherwise.
bool CheckFieldInForm(const FormData& form, FieldRendererId field_id);

// The default case for the overload below where there is no FormData parameter.
template <typename... Args>
  requires(!std::same_as<std::remove_cvref_t<Args>, FormData> && ...)
bool CheckFieldInForm(const Args&... args) {
  return true;
}

// Returns true if all `FieldRendererId`s among `args...` are elements of
// `form`. Kills the renderer otherwise.
//
// The intended use is to call `CheckFieldInForm(args...)` in Mojo receiver
// implementations, where `args...` are the arguments of the Mojo message.
template <typename... Args>
bool CheckFieldInForm(const FormData& form, const Args&... args) {
  return ValidateArguments(
      [&form](FieldRendererId field_id) {
        return CheckFieldInForm(form, field_id);
      },
      args...);
}

}  // namespace internal

// Returns true if `frame` is not prerendering (when autofill updates are
// disallowed). Kills the renderer if we are prerendering.
bool CheckFrameNotPrerendering(content::RenderFrameHost* frame);

// Returns true if the following checks pass:
// - The trigger source is allowed to be sent by the renderer.
// - If the first argument is a `FormData` and it is succeeded by
// `FieldRendererId` arguments, then they must all correspond to entries in
// `FormData::fields()`.
// Returns false and kills the renderer otherwise.
template <typename... Args>
bool CheckArgs(const Args&... args) {
  return internal::CheckValidTriggerSource(args...) &&
         internal::CheckFieldInForm(args...);
}

}  // namespace bad_message
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_BAD_MESSAGE_H_
