// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCH_TYPE_CONVERTERS_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCH_TYPE_CONVERTERS_H_

#include "components/contextual_search/contextual_search_types.h"
#include "components/omnibox/composebox/composebox_query.mojom-forward.h"

namespace contextual_search {

composebox_query::mojom::FileUploadStatus ToMojom(FileUploadStatus status);
FileUploadStatus FromMojom(composebox_query::mojom::FileUploadStatus status);

composebox_query::mojom::FileUploadErrorType ToMojom(FileUploadErrorType type);
FileUploadErrorType FromMojom(
    composebox_query::mojom::FileUploadErrorType type);

}  // namespace contextual_search

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCH_TYPE_CONVERTERS_H_
