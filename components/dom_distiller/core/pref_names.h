// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_PREF_NAMES_H_
#define COMPONENTS_DOM_DISTILLER_CORE_PREF_NAMES_H_

namespace dom_distiller::prefs {

// Path to the integer corresponding to user's preference theme.
inline constexpr char kFont[] = "dom_distiller.font_family";
// Path to the integer corresponding to user's preference font family.
inline constexpr char kTheme[] = "dom_distiller.theme";
// Path to the float corresponding to user's preference font scaling.
inline constexpr char kFontScale[] = "dom_distiller.font_scale";
// Path to the boolean whether Reader Mode for Accessibility option is enabled.
inline constexpr char kReaderForAccessibility[] =
    "dom_distiller.reader_for_accessibility";

}  // namespace dom_distiller::prefs

#endif  // COMPONENTS_DOM_DISTILLER_CORE_PREF_NAMES_H_
