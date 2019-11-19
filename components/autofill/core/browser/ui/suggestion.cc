// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/suggestion.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"

namespace autofill {

Suggestion::Suggestion() = default;
Suggestion::Suggestion(const Suggestion& other) = default;
Suggestion::Suggestion(Suggestion&& other) = default;

Suggestion::Suggestion(base::string16 value) : value(std::move(value)) {}

Suggestion::Suggestion(base::StringPiece value,
                       base::StringPiece label,
                       std::string icon,
                       int frontend_id)
    : frontend_id(frontend_id),
      value(base::UTF8ToUTF16(value)),
      label(base::UTF8ToUTF16(label)),
      icon(std::move(icon)) {}

Suggestion& Suggestion::operator=(const Suggestion& other) = default;
Suggestion& Suggestion::operator=(Suggestion&& other) = default;

Suggestion::~Suggestion() = default;

}  // namespace autofill
