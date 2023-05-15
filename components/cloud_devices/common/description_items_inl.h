// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLOUD_DEVICES_COMMON_DESCRIPTION_ITEMS_INL_H_
#define COMPONENTS_CLOUD_DEVICES_COMMON_DESCRIPTION_ITEMS_INL_H_

#include <stddef.h>

#include <memory>
#include <utility>

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
ListCapability<Option, Traits>::ListCapability(ListCapability&& other) =
    default;

template <class Option, class Traits>
ListCapability<Option, Traits>::~ListCapability() = default;

template <class Option, class Traits>
bool ListCapability<Option, Traits>::IsValid() const {
  if (empty())
    return false;  // This type of capabilities can't be empty.
  for (const auto& item : options_) {
    if (!Traits::IsValid(item))
      return false;
  }
  return true;
}

template <class Option, class Traits>
std::string ListCapability<Option, Traits>::GetPath() const {
  return Traits::GetCapabilityPath();
}

template <class Option, class Traits>
std::string ListTicketItem<Option, Traits>::GetPath() const {
  return Traits::GetTicketItemPath();
}

template <class Option, class Traits>
bool ListCapability<Option, Traits>::LoadFrom(
    const CloudDeviceDescription& description) {
  Reset();
  const base::Value::List* options_value = description.GetListItem(GetPath());
  if (!options_value)
    return false;
  for (const base::Value& option_value : *options_value) {
    Option option;
    if (!option_value.is_dict() ||
        !Traits::Load(option_value.GetDict(), &option)) {
      return false;
    }
    AddOption(std::move(option));
  }
  return IsValid();
}

template <class Option, class Traits>
void ListCapability<Option, Traits>::SaveTo(
    CloudDeviceDescription* description) const {
  DCHECK(IsValid());
  base::Value::List options_list;
  for (const Option& option : options_) {
    base::Value::Dict option_value;
    Traits::Save(option, &option_value);
    options_list.Append(std::move(option_value));
  }
  description->SetListItem(GetPath(), std::move(options_list));
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
    return false;  // This type of capabilities can't be empty.
  for (const auto& item : options_) {
    if (!Traits::IsValid(item))
      return false;
  }
  // This type of capability does not need a default value.
  return default_idx_.value_or(0) < size();
}

template <class Option, class Traits>
bool SelectionCapability<Option, Traits>::LoadFrom(
    const CloudDeviceDescription& description) {
  const base::Value::Dict* item =
      description.GetDictItem(Traits::GetCapabilityPath());
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
  base::Value::Dict dict;
  SaveTo(&dict);
  description->SetDictItem(Traits::GetCapabilityPath(), std::move(dict));
}

template <class Option, class Traits>
bool SelectionCapability<Option, Traits>::LoadFrom(
    const base::Value::Dict& dict) {
  Reset();
  const base::Value::List* options_value = dict.FindList(json::kKeyOption);
  if (!options_value)
    return false;
  for (const base::Value& option_value : *options_value) {
    Option option;
    if (!option_value.is_dict() ||
        !Traits::Load(option_value.GetDict(), &option)) {
      return false;
    }
    bool is_default =
        option_value.GetDict().FindBool(json::kKeyIsDefault).value_or(false);
    if (is_default && default_idx_.has_value())
      return false;  // Multiple defaults.

    AddDefaultOption(option, is_default);
  }
  return IsValid();
}

template <class Option, class Traits>
void SelectionCapability<Option, Traits>::SaveTo(
    base::Value::Dict* dict) const {
  DCHECK(IsValid());
  base::Value::List options_list;
  for (size_t i = 0; i < options_.size(); ++i) {
    base::Value::Dict option_value;
    if (default_idx_.has_value() && default_idx_.value() == i)
      option_value.Set(json::kKeyIsDefault, true);
    Traits::Save(options_[i], &option_value);
    options_list.Append(std::move(option_value));
  }
  dict->Set(json::kKeyOption, std::move(options_list));
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
  const base::Value::Dict* dict =
      description.GetDictItem(Traits::GetCapabilityPath());
  if (!dict)
    return false;
  default_value_ = dict->FindBool(json::kKeyDefault)
                       .value_or(static_cast<bool>(Traits::kDefault));
  return true;
}

template <class Traits>
void BooleanCapability<Traits>::SaveTo(
    CloudDeviceDescription* description) const {
  base::Value::Dict dict;
  if (default_value_ != Traits::kDefault) {
    dict.Set(json::kKeyDefault, default_value_);
  }
  description->SetDictItem(Traits::GetCapabilityPath(), std::move(dict));
}

template <class Traits>
bool EmptyCapability<Traits>::LoadFrom(
    const CloudDeviceDescription& description) {
  return description.GetDictItem(Traits::GetCapabilityPath()) != nullptr;
}

template <class Traits>
void EmptyCapability<Traits>::SaveTo(
    CloudDeviceDescription* description) const {
  description->SetDictItem(Traits::GetCapabilityPath(), base::Value::Dict());
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
  const base::Value::Dict* option_value =
      description.GetDictItem(Traits::GetCapabilityPath());
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
  base::Value::Dict dict;
  Traits::Save(value(), &dict);
  description->SetDictItem(Traits::GetCapabilityPath(), std::move(dict));
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
  const base::Value::Dict* option_value =
      description.GetDictItem(Traits::GetTicketItemPath());
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
  base::Value::Dict dict;
  Traits::Save(value(), &dict);
  description->SetDictItem(Traits::GetTicketItemPath(), std::move(dict));
}

}  // namespace cloud_devices

#endif  // COMPONENTS_CLOUD_DEVICES_COMMON_DESCRIPTION_ITEMS_INL_H_
