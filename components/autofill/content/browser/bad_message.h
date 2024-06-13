// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_BAD_MESSAGE_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_BAD_MESSAGE_H_

#include "components/autofill/core/common/unique_ids.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace autofill {

class FormData;

namespace bad_message {

// Returns true if `frame` is not prerendering (when autofill updates are
// disallowed). Kills the renderer if we are prerendering.
bool CheckFrameNotPrerendering(content::RenderFrameHost* frame);

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
  auto check_field_in_form = [&](const auto& arg) {
    if constexpr (std::same_as<std::remove_cvref_t<decltype(arg)>,
                               FieldRendererId>) {
      return bad_message::CheckFieldInForm(form, arg);
    }
    return true;
  };
  return (check_field_in_form(args) && ...);
}

}  // namespace bad_message
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_BAD_MESSAGE_H_
