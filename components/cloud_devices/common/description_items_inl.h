// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLOUD_DEVICES_COMMON_DESCRIPTION_DESCRIPTION_ITEMS_INL_H_
#define COMPONENTS_CLOUD_DEVICES_COMMON_DESCRIPTION_DESCRIPTION_ITEMS_INL_H_

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/numerics/safe_conversions.h"
#include "base/values.h"
#include "components/cloud_devices/common/description_items.h"

// Implementation of templates defined in header file.
// This file should be included from CC file with implementation of device
// specific capabilities.

namespace cloud_devices {

template <class Option, class Traits>
ListCapability<Option, Traits>::ListCapability() {
  Reset();
}

template <class Option, class Traits>
ListCapability<Option, Traits>::~ListCapability() {
}

template <class Option, class Traits>
bool ListCapability<Option, Traits>::IsValid() const {
  if (empty())
    return false;  // This type of capabilities can't be empty.
  for (size_t i = 0; i < options_.size(); ++i) {
    if (!Traits::IsValid(options_[i]))
      return false;
  }
  return true;
}

template <class Option, class Traits>
bool ListCapability<Option, Traits>::LoadFrom(
    const CloudDeviceDescription& description) {
  Reset();
  const base::Value* options_value =
      description.GetItem(Traits::GetCapabilityPath(), base::Value::Type::LIST);
  if (!options_value)
    return false;
  base::span<const base::Value> options = options_value->GetList();
  for (const base::Value& option_value : options) {
    Option option;
    if (!option_value.is_dict() || !Traits::Load(option_value, &option))
      return false;
    AddOption(std::move(option));
  }
  return IsValid();
}

template <class Option, class Traits>
void ListCapability<Option, Traits>::SaveTo(
    CloudDeviceDescription* description) const {
  DCHECK(IsValid());
  base::Value* options_list = description->CreateItem(
      Traits::GetCapabilityPath(), base::Value::Type::LIST);
  for (const Option& option : options_) {
    base::Value option_value(base::Value::Type::DICTIONARY);
    Traits::Save(option, &option_value);
    options_list->Append(std::move(option_value));
  }
}

template <class Option, class Traits>
SelectionCapability<Option, Traits>::SelectionCapability() {
  Reset();
}

template <class Option, class Traits>
SelectionCapability<Option, Traits>::SelectionCapability(
    SelectionCapability&& other) = default;

template <class Option, class Traits>
SelectionCapability<Option, Traits>::~SelectionCapability() {
}

template <class Option, class Traits>
SelectionCapability<Option, Traits>& SelectionCapability<Option, Traits>::
operator=(SelectionCapability&& other) = default;

template <class Option, class Traits>
bool SelectionCapability<Option, Traits>::operator==(
    const SelectionCapability<Option, Traits>& other) const {
  return options_ == other.options_ && default_idx_ == other.default_idx_;
}

template <class Option, class Traits>
bool SelectionCapability<Option, Traits>::IsValid() const {
  if (empty())
    return false;  // This type of capabilities can't be empty
  for (size_t i = 0; i < options_.size(); ++i) {
    if (!Traits::IsValid(options_[i]))
      return false;
  }
  return default_idx_ >= 0 && default_idx_ < base::checked_cast<int>(size());
}

template <class Option, class Traits>
bool SelectionCapability<Option, Traits>::LoadFrom(
    const CloudDeviceDescription& description) {
  const base::Value* item = description.GetItem(Traits::GetCapabilityPath(),
                                                base::Value::Type::DICTIONARY);
  if (!item) {
    Reset();
    return false;
  }
  return LoadFrom(*item);
}

template <class Option, class Traits>
void SelectionCapability<Option, Traits>::SaveTo(
    CloudDeviceDescription* description) const {
  DCHECK(IsValid());
  SaveTo(description->CreateItem(Traits::GetCapabilityPath(),
                                 base::Value::Type::DICTIONARY));
}

template <class Option, class Traits>
bool SelectionCapability<Option, Traits>::LoadFrom(const base::Value& dict) {
  Reset();
  const base::Value* options_value =
      dict.FindKeyOfType(json::kKeyOption, base::Value::Type::LIST);
  if (!options_value)
    return false;
  base::span<const base::Value> options = options_value->GetList();
  for (const base::Value& option_value : options) {
    Option option;
    if (!option_value.is_dict() || !Traits::Load(option_value, &option))
      return false;
    bool is_default =
        option_value.FindBoolKey(json::kKeyIsDefault).value_or(false);
    if (is_default && default_idx_ >= 0) {
      return false;  // Multiple defaults.
    }
    AddDefaultOption(option, is_default);
  }
  return IsValid();
}

template <class Option, class Traits>
void SelectionCapability<Option, Traits>::SaveTo(base::Value* dict) const {
  DCHECK(IsValid());
  base::Value options_list(base::Value::Type::LIST);
  for (size_t i = 0; i < options_.size(); ++i) {
    base::Value option_value(base::Value::Type::DICTIONARY);
    if (base::checked_cast<int>(i) == default_idx_)
      option_value.SetKey(json::kKeyIsDefault, base::Value(true));
    Traits::Save(options_[i], &option_value);
    options_list.Append(std::move(option_value));
  }
  dict->SetKey(json::kKeyOption, std::move(options_list));
}

template <class Traits>
BooleanCapability<Traits>::BooleanCapability() {
  Reset();
}

template <class Traits>
BooleanCapability<Traits>::~BooleanCapability() {
}

template <class Traits>
bool BooleanCapability<Traits>::LoadFrom(
    const CloudDeviceDescription& description) {
  Reset();
  const base::Value* dict = description.GetItem(Traits::GetCapabilityPath(),
                                                base::Value::Type::DICTIONARY);
  if (!dict)
    return false;
  default_value_ = dict->FindBoolKey(json::kKeyDefault)
                       .value_or(static_cast<bool>(Traits::kDefault));
  return true;
}

template <class Traits>
void BooleanCapability<Traits>::SaveTo(
    CloudDeviceDescription* description) const {
  base::Value* dict = description->CreateItem(Traits::GetCapabilityPath(),
                                              base::Value::Type::DICTIONARY);
  if (default_value_ != Traits::kDefault)
    dict->SetKey(json::kKeyDefault, base::Value(default_value_));
}

template <class Traits>
bool EmptyCapability<Traits>::LoadFrom(
    const CloudDeviceDescription& description) {
  return description.GetItem(Traits::GetCapabilityPath(),
                             base::Value::Type::DICTIONARY) != nullptr;
}

template <class Traits>
void EmptyCapability<Traits>::SaveTo(
    CloudDeviceDescription* description) const {
  description->CreateItem(Traits::GetCapabilityPath(),
                          base::Value::Type::DICTIONARY);
}

template <class Option, class Traits>
ValueCapability<Option, Traits>::ValueCapability() {
  Reset();
}

template <class Option, class Traits>
ValueCapability<Option, Traits>::~ValueCapability() {
}

template <class Option, class Traits>
bool ValueCapability<Option, Traits>::IsValid() const {
  return Traits::IsValid(value());
}

template <class Option, class Traits>
bool ValueCapability<Option, Traits>::LoadFrom(
    const CloudDeviceDescription& description) {
  Reset();
  const base::Value* option_value = description.GetItem(
      Traits::GetCapabilityPath(), base::Value::Type::DICTIONARY);
  if (!option_value)
    return false;
  Option option;
  if (!Traits::Load(*option_value, &option))
    return false;
  set_value(option);
  return IsValid();
}

template <class Option, class Traits>
void ValueCapability<Option, Traits>::SaveTo(
    CloudDeviceDescription* description) const {
  DCHECK(IsValid());
  Traits::Save(value(), description->CreateItem(Traits::GetCapabilityPath(),
                                                base::Value::Type::DICTIONARY));
}

template <class Option, class Traits>
TicketItem<Option, Traits>::TicketItem() {
  Reset();
}

template <class Option, class Traits>
TicketItem<Option, Traits>::~TicketItem() {
}

template <class Option, class Traits>
bool TicketItem<Option, Traits>::IsValid() const {
  return Traits::IsValid(value());
}

template <class Option, class Traits>
bool TicketItem<Option, Traits>::LoadFrom(
    const CloudDeviceDescription& description) {
  Reset();
  const base::Value* option_value = description.GetItem(
      Traits::GetTicketItemPath(), base::Value::Type::DICTIONARY);
  if (!option_value)
    return false;
  Option option;
  if (!Traits::Load(*option_value, &option))
    return false;
  set_value(option);
  return IsValid();
}

template <class Option, class Traits>
void TicketItem<Option, Traits>::SaveTo(
    CloudDeviceDescription* description) const {
  DCHECK(IsValid());
  Traits::Save(value(), description->CreateItem(Traits::GetTicketItemPath(),
                                                base::Value::Type::DICTIONARY));
}

}  // namespace cloud_devices

#endif  // COMPONENTS_CLOUD_DEVICES_COMMON_DESCRIPTION_DESCRIPTION_ITEMS_INL_H_
