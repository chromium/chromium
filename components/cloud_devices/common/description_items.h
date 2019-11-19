// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLOUD_DEVICES_COMMON_CAPABILITY_INTERFACES_H_
#define COMPONENTS_CLOUD_DEVICES_COMMON_CAPABILITY_INTERFACES_H_

// Defines common templates that could be used to create device specific
// capabilities and print tickets.

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "components/cloud_devices/common/cloud_device_description.h"

namespace cloud_devices {

// All traits below specify how to serialize and validate capabilities and
// ticket items.
// Traits should have following methods:
//   // Returns true if capability semantically valid.
//   static bool IsValid(const Option&);
//
//   // Returns json path relative to the root of CDD/CJT.
//   static std::string GetItemPath();
//
//   // Loads ticket item. Returns false if failed.
//   static bool Load(const base::Value& dict, ContentType* option);
//
//   // Saves ticket item.
//   static void Save(ContentType option, base::Value* dict);

// Represents a CDD capability that is stored as a JSON list
// Ex: "<CAPABILITY_NAME>": [ {<VALUE>}, {<VALUE>}, {<VALUE>} ]
// Option specifies data type for <VALUE>.
// Traits specifies how <VALUE> is stored in JSON and semantic validation.
template <class Option, class Traits>
class ListCapability {
 public:
  ListCapability();
  ~ListCapability();

  bool LoadFrom(const CloudDeviceDescription& description);
  void SaveTo(CloudDeviceDescription* description) const;

  void Reset() { options_.clear(); }

  bool IsValid() const;

  bool empty() const { return options_.empty(); }

  size_t size() const { return options_.size(); }

  const Option& operator[](size_t i) const { return options_[i]; }

  bool Contains(const Option& option) const {
    return base::Contains(options_, option);
  }

  void AddOption(Option&& option) { options_.emplace_back(std::move(option)); }

 private:
  typedef std::vector<Option> OptionVector;
  OptionVector options_;

  DISALLOW_COPY_AND_ASSIGN(ListCapability);
};

// Represents CDD capability stored as JSON list with default_value value.
// Ex: "<CAPABILITY_NAME>": { "option": [{ "is_default": true, <VALUE>},
//                                       {<VALUE>} ]}
// Option specifies data type for <VALUE>.
// Traits specifies how <VALUE> is stored in JSON and semantic validation.
template <class Option, class Traits>
class SelectionCapability {
 public:
  SelectionCapability();
  SelectionCapability(SelectionCapability&& other);
  ~SelectionCapability();

  SelectionCapability& operator=(SelectionCapability&& other);

  bool operator==(const SelectionCapability& other) const;

  bool LoadFrom(const CloudDeviceDescription& description);
  void SaveTo(CloudDeviceDescription* description) const;

  bool LoadFrom(const base::Value& dict);
  void SaveTo(base::Value* dict) const;

  void Reset() {
    options_.clear();
    default_idx_ = -1;
  }

  bool IsValid() const;

  bool empty() const { return options_.empty(); }

  size_t size() const { return options_.size(); }

  const Option& operator[](size_t i) const { return options_[i]; }

  bool Contains(const Option& option) const {
    return base::Contains(options_, option);
  }

  const Option& GetDefault() const {
    CHECK_GE(default_idx_, 0);
    return options_[default_idx_];
  }

  void AddOption(const Option& option) { AddDefaultOption(option, false); }

  void AddDefaultOption(const Option& option, bool is_default) {
    if (is_default) {
      DCHECK_EQ(default_idx_, -1);
      // Point to the last element.
      default_idx_ = base::checked_cast<int>(size());
    }
    options_.push_back(option);
  }

 private:
  typedef std::vector<Option> OptionVector;

  OptionVector options_;
  int default_idx_;

  DISALLOW_COPY_AND_ASSIGN(SelectionCapability);
};

// Represents CDD capability that can be true or false.
// Ex: "<CAPABILITY_NAME>": { "default_value": true }
// Traits specifies how <VALUE> is stored in JSON and semantic validation.
template <class Traits>
class BooleanCapability {
 public:
  BooleanCapability();
  ~BooleanCapability();

  bool LoadFrom(const CloudDeviceDescription& description);
  void SaveTo(CloudDeviceDescription* description) const;

  void Reset() { default_value_ = false; }

  void set_default_value(bool value) { default_value_ = value; }

  bool default_value() const { return default_value_; }

 private:
  bool default_value_;

  DISALLOW_COPY_AND_ASSIGN(BooleanCapability);
};

// Represents CDD capability for which existence is only important.
// Ex: "<CAPABILITY_NAME>": { }
// Traits specifies how <VALUE> is stored in JSON and semantic validation.
template <class Traits>
class EmptyCapability {
 public:
  EmptyCapability() {}
  ~EmptyCapability() {}

  bool LoadFrom(const CloudDeviceDescription& description);
  void SaveTo(CloudDeviceDescription* description) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyCapability);
};

// Represents an item that is of a specific value type.
// Ex: "<CAPABILITY_NAME>": {<VALUE>}
// Option specifies data type for <VALUE>.
// Traits specifies how <VALUE> is stored in JSON and semantic validation.
template <class Option, class Traits>
class ValueCapability {
 public:
  ValueCapability();
  ~ValueCapability();

  bool LoadFrom(const CloudDeviceDescription& description);
  void SaveTo(CloudDeviceDescription* description) const;

  void Reset() { value_ = Option(); }

  bool IsValid() const;

  const Option& value() const { return value_; }

  void set_value(const Option& value) { value_ = value; }

 private:
  Option value_;

  DISALLOW_COPY_AND_ASSIGN(ValueCapability);
};

// Represents CJT items.
// Ex: "<CAPABILITY_NAME>": {<VALUE>}
// Option specifies data type for <VALUE>.
// Traits specifies how <VALUE> is stored in JSON and semantic validation.
template <class Option, class Traits>
class TicketItem {
 public:
  TicketItem();
  ~TicketItem();

  bool LoadFrom(const CloudDeviceDescription& description);
  void SaveTo(CloudDeviceDescription* description) const;

  void Reset() { value_ = Option(); }

  bool IsValid() const;

  const Option& value() const { return value_; }

  void set_value(const Option& value) { value_ = value; }

 private:
  Option value_;

  DISALLOW_COPY_AND_ASSIGN(TicketItem);
};

}  // namespace cloud_devices

#endif  // COMPONENTS_CLOUD_DEVICES_COMMON_CAPABILITY_INTERFACES_H_
