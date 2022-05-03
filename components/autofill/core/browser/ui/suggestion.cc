// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/suggestion.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"

namespace autofill {

Suggestion::Text::Text() = default;

Suggestion::Text::Text(std::u16string value,
                       IsPrimary is_primary,
                       ShouldTruncate should_truncate)
    : value(value), is_primary(is_primary), should_truncate(should_truncate) {}

Suggestion::Text::Text(const Text& other) = default;
Suggestion::Text::Text(Text& other) = default;

Suggestion::Text& Suggestion::Text::operator=(const Text& other) = default;
Suggestion::Text& Suggestion::Text::operator=(Text&& other) = default;

Suggestion::Text::~Text() = default;

bool Suggestion::Text::operator==(const Suggestion::Text& text) const {
  return value == text.value && is_primary == text.is_primary &&
         should_truncate == text.should_truncate;
}

bool Suggestion::Text::operator!=(const Suggestion::Text& text) const {
  return !operator==(text);
}

Suggestion::Suggestion() = default;

Suggestion::Suggestion(std::u16string main_text)
    : main_text(std::move(main_text), Text::IsPrimary(true)) {}

Suggestion::Suggestion(base::StringPiece main_text,
                       base::StringPiece label,
                       std::string icon,
                       int frontend_id)
    : frontend_id(frontend_id),
      main_text(base::UTF8ToUTF16(main_text), Text::IsPrimary(true)),
      label(base::UTF8ToUTF16(label)),
      icon(std::move(icon)) {}

Suggestion::Suggestion(base::StringPiece main_text,
                       base::StringPiece minor_text,
                       base::StringPiece label,
                       std::string icon,
                       int frontend_id)
    : frontend_id(frontend_id),
      main_text(base::UTF8ToUTF16(main_text), Text::IsPrimary(true)),
      minor_text(base::UTF8ToUTF16(minor_text)),
      label(base::UTF8ToUTF16(label)),
      icon(std::move(icon)) {}

Suggestion::Suggestion(const Suggestion& other) = default;
Suggestion::Suggestion(Suggestion&& other) = default;

Suggestion& Suggestion::operator=(const Suggestion& other) = default;
Suggestion& Suggestion::operator=(Suggestion&& other) = default;

Suggestion::~Suggestion() = default;

}  // namespace autofill
