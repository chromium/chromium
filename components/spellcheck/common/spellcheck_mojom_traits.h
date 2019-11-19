// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_MOJOM_TRAITS_H_
#define COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_MOJOM_TRAITS_H_

#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_result.h"

namespace mojo {

template <>
struct EnumTraits<spellcheck::mojom::Decoration, SpellCheckResult::Decoration> {
  static spellcheck::mojom::Decoration ToMojom(SpellCheckResult::Decoration);
  static bool FromMojom(spellcheck::mojom::Decoration,
                        SpellCheckResult::Decoration*);
};

template <>
struct StructTraits<spellcheck::mojom::SpellCheckResultDataView,
                    SpellCheckResult> {
  static SpellCheckResult::Decoration decoration(
      const SpellCheckResult& result) {
    return result.decoration;
  }

  static int32_t location(const SpellCheckResult& result) {
    return result.location;
  }

  static int32_t length(const SpellCheckResult& result) {
    return result.length;
  }

  static const std::vector<base::string16>& replacements(
      const SpellCheckResult& result) {
    return result.replacements;
  }

  static bool Read(spellcheck::mojom::SpellCheckResultDataView,
                   SpellCheckResult*);
};

}  // namespace mojo

#endif  // COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_MOJOM_TRAITS_H_
