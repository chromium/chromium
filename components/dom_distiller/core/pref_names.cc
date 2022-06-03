// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/pref_names.h"

namespace dom_distiller {
namespace prefs {

// Path to the integer corresponding to user's preference theme.
const char kFont[] = "dom_distiller.font_family";
// Path to the integer corresponding to user's preference font family.
const char kTheme[] = "dom_distiller.theme";
// Path to the float corresponding to user's preference font scaling.
const char kFontScale[] = "dom_distiller.font_scale";
// Path to the boolean whether Reader Mode for Accessibility option is enabled.
const char kReaderForAccessibility[] = "dom_distiller.reader_for_accessibility";
// A boolean pref set to true if the option to use reader mode should be visible
// on articles, when available.
const char kOfferReaderMode[] = "dom_distiller.offer_reader_mode";

}  // namespace prefs
}  // namespace dom_distiller
