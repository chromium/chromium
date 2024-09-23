// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/editor_menu/public/cpp/editor_helpers.h"

#include <string>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "ui/gfx/range/range.h"

namespace chromeos::editor_helpers {

namespace {

constexpr auto kStripedSymbols =
    base::MakeFixedFlatSet<char>({' ', '\t', '\n', '.', ','});

}

size_t NonWhitespaceAndSymbolsLength(const std::u16string& text,
                                     gfx::Range selection_range) {
  size_t start = selection_range.start();
  size_t end = selection_range.end();

  if (start >= end || end > text.length()) {
    return 0;
  }

  while (start < end && (kStripedSymbols.contains(text[start]) ||
                         kStripedSymbols.contains(text[end - 1]))) {
    if (kStripedSymbols.contains(text[start])) {
      ++start;
    } else {
      --end;
    }
  }

  return end - start;
}

}  // namespace chromeos::editor_helpers
