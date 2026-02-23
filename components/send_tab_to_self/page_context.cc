// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/page_context.h"

namespace send_tab_to_self {

TextFragmentData::TextFragmentData() = default;
TextFragmentData::TextFragmentData(std::string text_start,
                                   std::string text_end,
                                   std::string prefix,
                                   std::string suffix)
    : text_start(std::move(text_start)),
      text_end(std::move(text_end)),
      prefix(std::move(prefix)),
      suffix(std::move(suffix)) {}
TextFragmentData::TextFragmentData(const TextFragmentData& other) = default;
TextFragmentData::TextFragmentData(TextFragmentData&& other) = default;
TextFragmentData& TextFragmentData::operator=(const TextFragmentData& other) =
    default;
TextFragmentData& TextFragmentData::operator=(TextFragmentData&& other) =
    default;
TextFragmentData::~TextFragmentData() = default;

bool TextFragmentData::IsEmpty() const {
  return text_start.empty();
}

bool TextFragmentData::operator==(const TextFragmentData& other) const =
    default;

ScrollPosition::ScrollPosition() = default;
ScrollPosition::ScrollPosition(const ScrollPosition&) = default;
ScrollPosition::ScrollPosition(ScrollPosition&&) = default;
ScrollPosition& ScrollPosition::operator=(const ScrollPosition&) = default;
ScrollPosition& ScrollPosition::operator=(ScrollPosition&&) = default;
ScrollPosition::~ScrollPosition() = default;

bool ScrollPosition::IsEmpty() const {
  return text_fragment.IsEmpty();
}

bool ScrollPosition::operator==(const ScrollPosition& other) const = default;

PageContext::FormField::FormField() = default;
PageContext::FormField::FormField(const FormField&) = default;
PageContext::FormField::FormField(FormField&&) = default;
PageContext::FormField& PageContext::FormField::operator=(const FormField&) =
    default;
PageContext::FormField& PageContext::FormField::operator=(FormField&&) =
    default;
PageContext::FormField::~FormField() = default;

bool PageContext::FormField::operator==(const FormField& other) const = default;

PageContext::FormFieldInfo::FormFieldInfo() = default;
PageContext::FormFieldInfo::FormFieldInfo(const FormFieldInfo&) = default;
PageContext::FormFieldInfo::FormFieldInfo(FormFieldInfo&&) = default;
PageContext::FormFieldInfo& PageContext::FormFieldInfo::operator=(
    const FormFieldInfo&) = default;
PageContext::FormFieldInfo& PageContext::FormFieldInfo::operator=(
    FormFieldInfo&&) = default;
PageContext::FormFieldInfo::~FormFieldInfo() = default;

bool PageContext::FormFieldInfo::operator==(const FormFieldInfo& other) const =
    default;

PageContext::PageContext() = default;
PageContext::PageContext(const PageContext&) = default;
PageContext::PageContext(PageContext&&) = default;
PageContext& PageContext::operator=(const PageContext&) = default;
PageContext& PageContext::operator=(PageContext&&) = default;
PageContext::~PageContext() = default;

bool PageContext::operator==(const PageContext& other) const = default;

}  // namespace send_tab_to_self
