// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_MOJOM_TRAITS_H_
#define COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_MOJOM_TRAITS_H_

#include "components/spellcheck/common/spellcheck.mojom-shared.h"
#include "components/spellcheck/common/spellcheck_decoration.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/common/spelling_marker.h"

namespace mojo {

template <>
struct EnumTraits<spellcheck::mojom::Decoration, spellcheck::Decoration> {
  static spellcheck::mojom::Decoration ToMojom(spellcheck::Decoration);
  static bool FromMojom(spellcheck::mojom::Decoration, spellcheck::Decoration*);
};

template <>
struct StructTraits<spellcheck::mojom::SpellCheckResultDataView,
                    SpellCheckResult> {
  static spellcheck::Decoration decoration(const SpellCheckResult& result) {
    return result.decoration;
  }

  static int32_t location(const SpellCheckResult& result) {
    return result.location;
  }

  static int32_t length(const SpellCheckResult& result) {
    return result.length;
  }

  static const std::vector<std::u16string>& replacements(
      const SpellCheckResult& result) {
    return result.replacements;
  }

  static bool should_hide_suggestion_menu(const SpellCheckResult& result) {
    return result.should_hide_suggestion_menu;
  }

  static bool Read(spellcheck::mojom::SpellCheckResultDataView,
                   SpellCheckResult*);
};

template <>
struct StructTraits<spellcheck::mojom::SpellingMarkerDataView,
                    spellcheck::SpellingMarker> {
  static spellcheck::Decoration marker_type(
      const spellcheck::SpellingMarker& marker) {
    return marker.marker_type;
  }
  static uint32_t start(const spellcheck::SpellingMarker& marker) {
    return marker.start;
  }

  static uint32_t end(const spellcheck::SpellingMarker& marker) {
    return marker.end;
  }

  static bool Read(spellcheck::mojom::SpellingMarkerDataView,
                   spellcheck::SpellingMarker*);
};

}  // namespace mojo

#endif  // COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_MOJOM_TRAITS_H_
