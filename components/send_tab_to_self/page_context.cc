// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/page_context.h"

namespace send_tab_to_self {

PageContext::FormField::FormField() = default;
PageContext::FormField::FormField(const FormField&) = default;
PageContext::FormField::FormField(FormField&&) = default;
PageContext::FormField& PageContext::FormField::operator=(const FormField&) =
    default;
PageContext::FormField& PageContext::FormField::operator=(FormField&&) =
    default;
PageContext::FormField::~FormField() = default;

PageContext::FormFieldInfo::FormFieldInfo() = default;
PageContext::FormFieldInfo::FormFieldInfo(const FormFieldInfo&) = default;
PageContext::FormFieldInfo::FormFieldInfo(FormFieldInfo&&) = default;
PageContext::FormFieldInfo& PageContext::FormFieldInfo::operator=(
    const FormFieldInfo&) = default;
PageContext::FormFieldInfo& PageContext::FormFieldInfo::operator=(
    FormFieldInfo&&) = default;
PageContext::FormFieldInfo::~FormFieldInfo() = default;

PageContext::PageContext() = default;
PageContext::PageContext(const PageContext&) = default;
PageContext::PageContext(PageContext&&) = default;
PageContext& PageContext::operator=(const PageContext&) = default;
PageContext& PageContext::operator=(PageContext&&) = default;
PageContext::~PageContext() = default;

}  // namespace send_tab_to_self
