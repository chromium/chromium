// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/page_context.h"

namespace send_tab_to_self {

PageContext::PageContext() = default;
PageContext::PageContext(const PageContext&) = default;
PageContext::PageContext(PageContext&&) = default;
PageContext& PageContext::operator=(const PageContext&) = default;
PageContext& PageContext::operator=(PageContext&&) = default;
PageContext::~PageContext() = default;

}  // namespace send_tab_to_self
