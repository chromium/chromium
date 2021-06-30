// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/common/internal_plugin_helpers.h"

#include "base/feature_list.h"
#include "pdf/pdf_features.h"

namespace pdf {

const char kInternalPluginMimeType[] = "application/x-google-chrome-pdf";

bool IsInternalPluginExternallyHandled() {
  return base::FeatureList::IsEnabled(chrome_pdf::features::kPdfUnseasoned);
}

}  // namespace pdf
