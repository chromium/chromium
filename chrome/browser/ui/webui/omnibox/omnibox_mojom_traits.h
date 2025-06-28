// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_MOJOM_TRAITS_H_

#include "chrome/browser/ui/webui/omnibox/omnibox_internals.mojom-shared.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<mojom::ACMatchClassificationDataView,
                    ::AutocompleteMatch::ACMatchClassification> {
  static int offset(const AutocompleteMatch::ACMatchClassification& in) {
    return in.offset;
  }

  static int style(const AutocompleteMatch::ACMatchClassification& in) {
    return in.style;
  }

  static bool Read(mojom::ACMatchClassificationDataView data,
                   AutocompleteMatch::ACMatchClassification* out);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_MOJOM_TRAITS_H_
