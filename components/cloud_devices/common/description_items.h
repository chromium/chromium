// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLOUD_DEVICES_COMMON_DESCRIPTION_ITEMS_H_
#define COMPONENTS_CLOUD_DEVICES_COMMON_DESCRIPTION_ITEMS_H_

// Defines common templates that could be used to create device specific
// capabilities and print tickets.

#include <stddef.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
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
//   static bool Load(const base::Value::Dict& dict, ContentType* option);
//
//   // Saves ticket item.
//   static void Save(ContentType option, base::Value::Dict* dict);

// Represents a CDD capability that is stored as a JSON list
// Ex: "<CAPABILITY_NAME>": [ {<VALUE>}, {<VALUE>}, {<VALUE>} ]
// Option specifies data type for <VALUE>.
// Traits specifies how <VALUE> is stored in JSON and semantic validation.
template <class Option, class Traits>
class ListCapability {
 public:
  using OptionVector = std::vector<Option>;

  ListCapability();
  ListCapability(ListCapability&& other);

  ListCapability(const ListCapability&) = delete;
  ListCapability& operator=(const ListCapability&) = delete;

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

  typename OptionVector::iterator begin() { return options_.begin(); }
  typename OptionVector::const_iterator begin() const {
    return options_.begin();
  }

  typename OptionVector::iterator end() { return options_.end(); }
  typename OptionVector::const_iterator end() const { return options_.end(); }

  // Returns JSON path for this item relative to the root of the CDD.
  virtual std::string GetPath() const;

 protected:
  OptionVector options_;
};

// Represents a CJT item that is stored as a JSON list.  This works similarly to
// ListCapability except it's used for ticket items instead of capabilities.
template <class Option, class Traits>
class ListTicketItem : public ListCapability<Option, Traits> {
 public:
  // ListCapability:
  std::string GetPath() const override;
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

  SelectionCapability(const SelectionCapability&) = delete;
  SelectionCapability& operator=(const SelectionCapability&) = delete;

  ~SelectionCapability();

  SelectionCapability& operator=(SelectionCapability&& other);

  bool operator==(const SelectionCapability& other) const;

  bool LoadFrom(const CloudDeviceDescription& description);
  void SaveTo(CloudDeviceDescription* description) const;

  bool LoadFrom(const base::Value::Dict& dict);
  void SaveTo(base::Value::Dict* dict) const;

  void Reset() {
    options_.clear();
    default_idx_.reset();
  }

  bool IsValid() const;

  bool empty() const { return options_.empty(); }

  size_t size() const { return options_.size(); }

  const Option& operator[](size_t i) const { return options_[i]; }

  bool Contains(const Option& option) const {
    return base::Contains(options_, option);
  }

  const Option& GetDefault() const {
    CHECK(!options_.empty());
    return options_[default_idx_.value_or(0)];
  }

  void AddOption(const Option& option) { AddDefaultOption(option, false); }

  void AddDefaultOption(const Option& option, bool is_default) {
    if (is_default) {
      DCHECK(!default_idx_.has_value());
      // Point to the last element.
      default_idx_ = size();
    }
    options_.push_back(option);
  }

 private:
  typedef std::vector<Option> OptionVector;

  OptionVector options_;
  std::optional<size_t> default_idx_;
};

// Represents CDD capability that can be true or false.
// Ex: "<CAPABILITY_NAME>": { "default_value": true }
// Traits specifies how <VALUE> is stored in JSON and semantic validation.
template <class Traits>
class BooleanCapability {
 public:
  BooleanCapability();

  BooleanCapability(const BooleanCapability&) = delete;
  BooleanCapability& operator=(const BooleanCapability&) = delete;

  ~BooleanCapability();

  bool LoadFrom(const CloudDeviceDescription& description);
  void SaveTo(CloudDeviceDescription* description) const;

  void Reset() { default_value_ = false; }

  void set_default_value(bool value) { default_value_ = value; }

  bool default_value() const { return default_value_; }

 private:
  bool default_value_;
};

// Represents CDD capability for which existence is only important.
// Ex: "<CAPABILITY_NAME>": { }
// Traits specifies how <VALUE> is stored in JSON and semantic validation.
template <class Traits>
class EmptyCapability {
 public:
  EmptyCapability() = default;

  EmptyCapability(const EmptyCapability&) = delete;
  EmptyCapability& operator=(const EmptyCapability&) = delete;

  ~EmptyCapability() = default;

  bool LoadFrom(const CloudDeviceDescription& description);
  void SaveTo(CloudDeviceDescription* description) const;
};

// Represents an item that is of a specific value type.
// Ex: "<CAPABILITY_NAME>": {<VALUE>}
// Option specifies data type for <VALUE>.
// Traits specifies how <VALUE> is stored in JSON and semantic validation.
template <class Option, class Traits>
class ValueCapability {
 public:
  ValueCapability();

  ValueCapability(const ValueCapability&) = delete;
  ValueCapability& operator=(const ValueCapability&) = delete;

  ~ValueCapability();

  bool LoadFrom(const CloudDeviceDescription& description);
  void SaveTo(CloudDeviceDescription* description) const;

  void Reset() { value_ = Option(); }

  bool IsValid() const;

  const Option& value() const { return value_; }

  void set_value(const Option& value) { value_ = value; }

 private:
  Option value_;
};

// Represents CJT items.
// Ex: "<CAPABILITY_NAME>": {<VALUE>}
// Option specifies data type for <VALUE>.
// Traits specifies how <VALUE> is stored in JSON and semantic validation.
template <class Option, class Traits>
class TicketItem {
 public:
  TicketItem();

  TicketItem(const TicketItem&) = delete;
  TicketItem& operator=(const TicketItem&) = delete;

  ~TicketItem();

  bool LoadFrom(const CloudDeviceDescription& description);
  void SaveTo(CloudDeviceDescription* description) const;

  void Reset() { value_ = Option(); }

  bool IsValid() const;

  const Option& value() const { return value_; }

  void set_value(const Option& value) { value_ = value; }

 private:
  Option value_;
};

}  // namespace cloud_devices

#endif  // COMPONENTS_CLOUD_DEVICES_COMMON_DESCRIPTION_ITEMS_H_
