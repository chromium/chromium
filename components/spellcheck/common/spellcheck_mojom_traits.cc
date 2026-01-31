// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/common/spellcheck_mojom_traits.h"

#include <algorithm>

#include "components/spellcheck/common/spellcheck_decoration.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"

namespace mojo {

spellcheck::mojom::Decoration
EnumTraits<spellcheck::mojom::Decoration, spellcheck::Decoration>::ToMojom(
    spellcheck::Decoration decoration) {
  switch (decoration) {
    case spellcheck::Decoration::SPELLING:
      return spellcheck::mojom::Decoration::kSpelling;
    case spellcheck::Decoration::GRAMMAR:
      return spellcheck::mojom::Decoration::kGrammar;
  }
  NOTREACHED();
}

bool EnumTraits<spellcheck::mojom::Decoration, spellcheck::Decoration>::
    FromMojom(spellcheck::mojom::Decoration input,
              spellcheck::Decoration* output) {
  switch (input) {
    case spellcheck::mojom::Decoration::kSpelling:
      *output = spellcheck::Decoration::SPELLING;
      return true;
    case spellcheck::mojom::Decoration::kGrammar:
      *output = spellcheck::Decoration::GRAMMAR;
      return true;
  }
  NOTREACHED();
}

bool StructTraits<
    spellcheck::mojom::SpellCheckResultDataView,
    SpellCheckResult>::Read(spellcheck::mojom::SpellCheckResultDataView input,
                            SpellCheckResult* output) {
  if (!input.ReadDecoration(&output->decoration))
    return false;
  output->location = input.location();
  output->length = input.length();
  output->should_hide_suggestion_menu = input.should_hide_suggestion_menu();
  if (!input.ReadReplacements(&output->replacements))
    return false;
  return true;
}

bool StructTraits<spellcheck::mojom::SpellingMarkerDataView,
                  spellcheck::SpellingMarker>::
    Read(spellcheck::mojom::SpellingMarkerDataView input,
         spellcheck::SpellingMarker* output) {
  if (!input.ReadMarkerType(&output->marker_type)) {
    return false;
  }

  if (input.start() > input.end()) {
    return false;
  }

  output->start = input.start();
  output->end = input.end();

  return true;
}

}  // namespace mojo
