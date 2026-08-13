// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_query_controller_types.h"

namespace lens {

PageContent::PageContent() : content_type_(lens::MimeType::kUnknown) {}
PageContent::PageContent(std::vector<uint8_t> bytes,
                         lens::MimeType content_type)
    : bytes_(bytes), content_type_(content_type) {}
PageContent::PageContent(const PageContent& other) = default;
PageContent::~PageContent() = default;

}  // namespace lens
