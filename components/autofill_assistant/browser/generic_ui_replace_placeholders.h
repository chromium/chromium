// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_GENERIC_UI_REPLACE_PLACEHOLDERS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_GENERIC_UI_REPLACE_PLACEHOLDERS_H_

#include <string>

#include "base/containers/flat_map.h"
#include "components/autofill_assistant/browser/generic_ui.pb.h"

namespace autofill_assistant {

// Replaces all occurrences of |placeholders| in view- and model identifiers
// occurring in |in_out_proto|. Ignores other placeholders.
void ReplacePlaceholdersInGenericUi(
    GenericUserInterfaceProto* in_out_proto,
    const base::flat_map<std::string, std::string>& placeholders);

// Same as |ReplacePlaceholdersInGenericUi|, for a single callback.
void ReplacePlaceholdersInCallback(
    CallbackProto* in_out_proto,
    const base::flat_map<std::string, std::string>& placeholders);

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_GENERIC_UI_REPLACE_PLACEHOLDERS_H_
