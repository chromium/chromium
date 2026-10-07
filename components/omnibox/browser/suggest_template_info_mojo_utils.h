// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SUGGEST_TEMPLATE_INFO_MOJO_UTILS_H_
#define COMPONENTS_OMNIBOX_BROWSER_SUGGEST_TEMPLATE_INFO_MOJO_UTILS_H_

#include "components/omnibox/browser/suggest_template_info.mojom.h"

struct AutocompleteMatch;

namespace suggest_template_info {

// Creates the SuggestTemplateInfo Mojom object to render `match` with, i.e. the
// client-supported, rendering-relevant subset of SuggestTemplateInfo.
mojom::SuggestTemplateInfoPtr CreateSuggestTemplateInfo(
    const AutocompleteMatch& match);

}  // namespace suggest_template_info

#endif  // COMPONENTS_OMNIBOX_BROWSER_SUGGEST_TEMPLATE_INFO_MOJO_UTILS_H_
