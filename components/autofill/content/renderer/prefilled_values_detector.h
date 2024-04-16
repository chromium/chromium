// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PREFILLED_VALUES_DETECTOR_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PREFILLED_VALUES_DETECTOR_H_

#include <string>
#include <string_view>

#include "base/containers/span.h"

namespace autofill {

// Returns a list of known username placeholders, all guaranteed to be lower
// case.
// This is only exposed for testing.
base::span<const std::string_view> KnownUsernamePlaceholders();

// Checks if the prefilled value of the username element is one of the known
// values possibly used as placeholders. The list of possible placeholder
// values comes from popular sites exhibiting this issue.
//
// If |username_value| is in KnownUsernamePlaceholder(), the password manager
// takes the liberty to override the contents of the username field.
//
// The |possible_email_domain| is supposed to be the eTLD+1 for which the
// credential was saved. So if the credential was saved for
// https://www.example.com, there is a chance that the website prefills
// the username field with "@example.com".
//
// TODO(crbug.com/41383074): Remove this once a stable solution is in place.
bool PossiblePrefilledUsernameValue(const std::string& username_value,
                                    const std::string& possible_email_domain);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PREFILLED_VALUES_DETECTOR_H_
