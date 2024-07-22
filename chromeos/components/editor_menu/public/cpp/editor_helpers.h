// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_HELPERS_H_
#define CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_HELPERS_H_

#include <string>

#include "base/component_export.h"
#include "ui/gfx/range/range.h"

namespace chromeos::editor_helpers {

size_t COMPONENT_EXPORT(EDITOR_MENU_PUBLIC_CPP)
    NonWhitespaceAndSymbolsLength(const std::u16string& text,
                                  gfx::Range selection_range);

}  // namespace chromeos::editor_helpers

#endif  // CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_HELPERS_H_
