// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/omnibox_mojom_traits.h"

#include "base/notreached.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_internals.mojom-shared.h"

namespace mojo {

bool StructTraits<mojom::ACMatchClassificationDataView,
                  AutocompleteMatch::ACMatchClassification>::
    Read(mojom::ACMatchClassificationDataView data,
         AutocompleteMatch::ACMatchClassification* out) {
  // These traits are only used for serializing to WebUI; no deserialization is
  // expected.
  NOTREACHED();
}

}  // namespace mojo
