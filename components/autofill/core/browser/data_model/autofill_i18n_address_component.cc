// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_i18n_address_component.h"
#include "base/ranges/algorithm.h"

namespace autofill {

I18nAddressComponent::~I18nAddressComponent() = default;

I18nAddressComponent::I18nAddressComponent(
    ServerFieldType storage_type,
    std::vector<std::unique_ptr<I18nAddressComponent>> children,
    unsigned int merge_mode)
    : AddressComponent(storage_type, nullptr, merge_mode),
      children_(std::move(children)) {
  for (const auto& child : children_) {
    child->SetParent(this);
    RegisterChildNode(child.get());
  }
}

}  // namespace autofill
