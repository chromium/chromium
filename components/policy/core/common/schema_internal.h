// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_SCHEMA_INTERNAL_H_
#define COMPONENTS_POLICY_CORE_COMMON_SCHEMA_INTERNAL_H_

#include "base/values.h"
#include "components/policy/policy_export.h"

namespace policy {
namespace internal {

// These types are used internally by the SchemaOwner parser, and by the
// compile-time code generator. They shouldn't be used directly.

// Represents the type of one policy, or an item of a list policy, or a
// property of a map policy.
struct POLICY_EXPORT SchemaNode {
  // The policy type.
  base::Value::Type type;

  // If |type| is Type::DICTIONARY then |extra| is an offset into
  // SchemaData::properties_nodes that indexes the PropertiesNode describing
  // the entries of this dictionary.
  //
  // If |type| is Type::LIST then |extra| is an offset into
  // SchemaData::schema_nodes that indexes the SchemaNode describing the items
  // of this list.
  //
  // If |type| is Type::INTEGER or Type::STRING, and contains corresponding
  // restriction (enumeration of possible values, or range for integer), then
  // |extra| is an offset into SchemaData::restriction_nodes that indexes the
  // RestrictionNode describing the restriction on the value.
  //
  // Otherwise extra is -1 and is invalid.
  short extra;

  // True if this value is sensitive and should be masked before displaying it
  // to the user.
  bool is_sensitive_value;

  // True if any of its children has |is_sensitive_value|==true.
  bool has_sensitive_children;
};

// Represents an entry of a map policy.
struct POLICY_EXPORT PropertyNode {
  // The entry key.
  const char* key;

  // An offset into SchemaData::schema_nodes that indexes the SchemaNode
  // describing the structure of this key.
  short schema;
};

// Represents the list of keys of a map policy.
struct POLICY_EXPORT PropertiesNode {
  // An offset into SchemaData::property_nodes that indexes the PropertyNode
  // describing the first known property of this map policy.
  short begin;

  // An offset into SchemaData::property_nodes that indexes the PropertyNode
  // right beyond the last known property of this map policy.
  //
  // If |begin == end| then the map policy that this PropertiesNode corresponds
  // to does not have known properties.
  //
  // Note that the range [begin, end) is sorted by PropertyNode::key, so that
  // properties can be looked up by binary searching in the range.
  short end;

  // An offset into SchemaData::property_nodes that indexes the PropertyNode
  // right beyond the last known pattern property.
  //
  // [end, pattern_end) is the range that covers all pattern properties
  // defined. It's not required to be sorted.
  short pattern_end;

  // An offset into SchemaData::required_properties that indexes the first
  // required property of this map policy.
  short required_begin;

  // An offset into SchemaData::required_properties that indexes the property
  // right beyond the last required property.
  //
  // If |required_begin == required_end|, then the map policy that this
  // PropertiesNode corresponds to does not have any required properties.
  //
  // Note that the range [required_begin, required_end) is not sorted.
  short required_end;

  // If this map policy supports keys with any value (besides the well-known
  // values described in the range [begin, end)) then |additional| is an offset
  // into SchemaData::schema_nodes that indexes the SchemaNode describing the
  // structure of the values for those keys. Otherwise |additional| is -1 and
  // is invalid.
  short additional;
};

// Represents the restriction on Type::INTEGER or Type::STRING instance of
// base::Value.
union POLICY_EXPORT RestrictionNode {
  // Offsets into SchemaData::int_enums or SchemaData::string_enums, the
  // entry of which describes the enumeration of all possible values of
  // corresponding integer or string value. |offset_begin| being strictly less
  // than |offset_end| is assumed.
  struct EnumerationRestriction {
    int offset_begin;
    int offset_end;
  } enumeration_restriction;

  // For integer type only, represents that all values between |min_value|
  // and |max_value| can be choosen. Note that integer type in base::Value
  // is bounded, so this can also be used if only one of |min_value| and
  // |max_value| is stated. |max_value| being greater or equal to |min_value|
  // is assumed.
  struct RangedRestriction {
    int max_value;
    int min_value;
  } ranged_restriction;

  // For string type only, requires |pattern_index| and |pattern_index_backup|
  // to be exactly the same. And it's an offset into SchemaData::string_enums
  // which contains the regular expression that the target string must follow.
  struct StringPatternRestriction {
    int pattern_index;
    int pattern_index_backup;
  } string_pattern_restriction;
};

// Contains arrays of related nodes. All of the offsets in these nodes reference
// other nodes in these arrays.
struct POLICY_EXPORT SchemaData {
  const SchemaNode* schema_nodes;
  const PropertyNode* property_nodes;
  const PropertiesNode* properties_nodes;
  const RestrictionNode* restriction_nodes;
  const char* const* required_properties;

  const int* int_enums;
  const char* const* string_enums;
  int validation_schema_root_index;
};

}  // namespace internal
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_SCHEMA_INTERNAL_H_
