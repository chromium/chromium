// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/policy/core/common/schema.h"

#include <limits.h>
#include <stddef.h>

#include <algorithm>
#include <climits>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/policy/core/common/json_schema_constants.h"
#include "components/policy/core/common/schema_internal.h"
#include "third_party/re2/src/re2/re2.h"

namespace schema = json_schema_constants;

namespace policy {

using internal::PropertiesNode;
using internal::PropertyNode;
using internal::RestrictionNode;
using internal::SchemaData;
using internal::SchemaNode;

std::string ErrorPathToString(const std::string& policy_name,
                              PolicyErrorPath error_path) {
  if (error_path.empty())
    return std::string();

  std::stringstream error_path_string{policy_name};
  error_path_string << policy_name;
  for (auto& entry : error_path) {
    if (absl::holds_alternative<int>(entry)) {
      error_path_string << "[" << absl::get<int>(entry) << "]";
    } else if (absl::holds_alternative<std::string>(entry)) {
      error_path_string << "." << absl::get<std::string>(entry);
    }
  }
  return error_path_string.str();
}

const char kSensitiveValueMask[] = "********";

namespace {

struct ReferencesAndIDs {
  // Maps schema "id" attributes to the corresponding SchemaNode index.
  std::map<std::string, short> id_map;

  // List of pairs of references to be assigned later. The string is the "id"
  // whose corresponding index should be stored in the pointer, once all the IDs
  // are available.
  std::vector<std::pair<std::string, short*>> reference_list;
};

// Sizes for the storage arrays. These are calculated in advance so that the
// arrays don't have to be resized during parsing, which would invalidate
// pointers into their contents (i.e. string's c_str() and address of indices
// for "$ref" attributes).
struct StorageSizes {
  size_t strings = 0;
  size_t schema_nodes = 0;
  size_t property_nodes = 0;
  size_t properties_nodes = 0;
  size_t restriction_nodes = 0;
  size_t required_properties = 0;
  size_t int_enums = 0;
  size_t string_enums = 0;
};

// An invalid index, indicating that a node is not present; similar to a NULL
// pointer.
const short kInvalid = -1;

// Maps a schema key to the corresponding base::Value::Type
struct SchemaKeyToValueType {
  const char* key;
  base::Value::Type type;
};

// Allowed types and their base::Value::Type equivalent. These are ordered
// alphabetically to perform binary search.
const SchemaKeyToValueType kSchemaTypesToValueTypes[] = {
    {schema::kArray, base::Value::Type::LIST},
    {schema::kBoolean, base::Value::Type::BOOLEAN},
    {schema::kInteger, base::Value::Type::INTEGER},
    {schema::kNumber, base::Value::Type::DOUBLE},
    {schema::kObject, base::Value::Type::DICT},
    {schema::kString, base::Value::Type::STRING},
};
const SchemaKeyToValueType* kSchemaTypesToValueTypesEnd =
    kSchemaTypesToValueTypes + std::size(kSchemaTypesToValueTypes);

// Allowed attributes and types for type 'array'. These are ordered
// alphabetically to perform binary search.
const SchemaKeyToValueType kAttributesAndTypesForArray[] = {
    {schema::kDescription, base::Value::Type::STRING},
    {schema::kId, base::Value::Type::STRING},
    {schema::kItems, base::Value::Type::DICT},
    {schema::kSensitiveValue, base::Value::Type::BOOLEAN},
    {schema::kTitle, base::Value::Type::STRING},
    {schema::kType, base::Value::Type::STRING},
};
const SchemaKeyToValueType* kAttributesAndTypesForArrayEnd =
    kAttributesAndTypesForArray + std::size(kAttributesAndTypesForArray);

// Allowed attributes and types for type 'boolean'. These are ordered
// alphabetically to perform binary search.
const SchemaKeyToValueType kAttributesAndTypesForBoolean[] = {
    {schema::kDescription, base::Value::Type::STRING},
    {schema::kId, base::Value::Type::STRING},
    {schema::kSensitiveValue, base::Value::Type::BOOLEAN},
    {schema::kTitle, base::Value::Type::STRING},
    {schema::kType, base::Value::Type::STRING},
};
const SchemaKeyToValueType* kAttributesAndTypesForBooleanEnd =
    kAttributesAndTypesForBoolean + std::size(kAttributesAndTypesForBoolean);

// Allowed attributes and types for type 'integer'. These are ordered
// alphabetically to perform binary search.
const SchemaKeyToValueType kAttributesAndTypesForInteger[] = {
    {schema::kDescription, base::Value::Type::STRING},
    {schema::kEnum, base::Value::Type::LIST},
    {schema::kId, base::Value::Type::STRING},
    {schema::kMaximum, base::Value::Type::DOUBLE},
    {schema::kMinimum, base::Value::Type::DOUBLE},
    {schema::kSensitiveValue, base::Value::Type::BOOLEAN},
    {schema::kTitle, base::Value::Type::STRING},
    {schema::kType, base::Value::Type::STRING},
};
const SchemaKeyToValueType* kAttributesAndTypesForIntegerEnd =
    kAttributesAndTypesForInteger + std::size(kAttributesAndTypesForInteger);

// Allowed attributes and types for type 'number'. These are ordered
// alphabetically to perform binary search.
const SchemaKeyToValueType kAttributesAndTypesForNumber[] = {
    {schema::kDescription, base::Value::Type::STRING},
    {schema::kId, base::Value::Type::STRING},
    {schema::kSensitiveValue, base::Value::Type::BOOLEAN},
    {schema::kTitle, base::Value::Type::STRING},
    {schema::kType, base::Value::Type::STRING},
};
const SchemaKeyToValueType* kAttributesAndTypesForNumberEnd =
    kAttributesAndTypesForNumber + std::size(kAttributesAndTypesForNumber);

// Allowed attributes and types for type 'object'. These are ordered
// alphabetically to perform binary search.
const SchemaKeyToValueType kAttributesAndTypesForObject[] = {
    {schema::kAdditionalProperties, base::Value::Type::DICT},
    {schema::kDescription, base::Value::Type::STRING},
    {schema::kId, base::Value::Type::STRING},
    {schema::kPatternProperties, base::Value::Type::DICT},
    {schema::kProperties, base::Value::Type::DICT},
    {schema::kRequired, base::Value::Type::LIST},
    {schema::kSensitiveValue, base::Value::Type::BOOLEAN},
    {schema::kTitle, base::Value::Type::STRING},
    {schema::kType, base::Value::Type::STRING},
};
const SchemaKeyToValueType* kAttributesAndTypesForObjectEnd =
    kAttributesAndTypesForObject + std::size(kAttributesAndTypesForObject);

// Allowed attributes and types for $ref. These are ordered alphabetically to
// perform binary search.
const SchemaKeyToValueType kAttributesAndTypesForRef[] = {
    {schema::kDescription, base::Value::Type::STRING},
    {schema::kRef, base::Value::Type::STRING},
    {schema::kTitle, base::Value::Type::STRING},
};
const SchemaKeyToValueType* kAttributesAndTypesForRefEnd =
    kAttributesAndTypesForRef + std::size(kAttributesAndTypesForRef);

// Allowed attributes and types for type 'string'. These are ordered
// alphabetically to perform binary search.
const SchemaKeyToValueType kAttributesAndTypesForString[] = {
    {schema::kDescription, base::Value::Type::STRING},
    {schema::kEnum, base::Value::Type::LIST},
    {schema::kId, base::Value::Type::STRING},
    {schema::kPattern, base::Value::Type::STRING},
    {schema::kSensitiveValue, base::Value::Type::BOOLEAN},
    {schema::kTitle, base::Value::Type::STRING},
    {schema::kType, base::Value::Type::STRING},
};
const SchemaKeyToValueType* kAttributesAndTypesForStringEnd =
    kAttributesAndTypesForString + std::size(kAttributesAndTypesForString);

// Helper for std::lower_bound.
bool CompareToString(const SchemaKeyToValueType& entry,
                     const std::string& key) {
  return entry.key < key;
}

// Returns true if a SchemaKeyToValueType with key==|schema_key| can be found in
// the array represented by |begin| and |end|. If so, |value_type| will be set
// to the SchemaKeyToValueType value type.
bool MapSchemaKeyToValueType(const std::string& schema_key,
                             const SchemaKeyToValueType* begin,
                             const SchemaKeyToValueType* end,
                             base::Value::Type* value_type) {
  const SchemaKeyToValueType* entry =
      std::lower_bound(begin, end, schema_key, CompareToString);
  if (entry == end || entry->key != schema_key)
    return false;
  if (value_type)
    *value_type = entry->type;
  return true;
}

// Shorthand method for |SchemaTypeToValueType()| with
// |kSchemaTypesToValueTypes|.
bool SchemaTypeToValueType(const std::string& schema_type,
                           base::Value::Type* value_type) {
  return MapSchemaKeyToValueType(schema_type, kSchemaTypesToValueTypes,
                                 kSchemaTypesToValueTypesEnd, value_type);
}

bool StrategyAllowUnknown(SchemaOnErrorStrategy strategy) {
  return strategy != SCHEMA_STRICT;
}

bool StrategyAllowInvalidListEntry(SchemaOnErrorStrategy strategy) {
  return strategy == SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY;
}

bool StrategyAllowUnknownWithoutWarning(SchemaOnErrorStrategy strategy) {
  return strategy == SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING;
}

void SchemaErrorFound(PolicyErrorPath* out_error_path,
                      std::string* out_error,
                      const std::string& msg) {
  if (out_error_path)
    *out_error_path = {};
  if (out_error)
    *out_error = msg;
}

void AddListIndexPrefixToPath(int index, PolicyErrorPath* path) {
  if (path) {
    path->emplace(path->begin(), index);
  }
}

void AddDictKeyPrefixToPath(const std::string& key, PolicyErrorPath* path) {
  if (path) {
    path->emplace(path->begin(), key);
  }
}

bool IgnoreUnknownAttributes(int options) {
  return (options & kSchemaOptionsIgnoreUnknownAttributes);
}

// Check that the value's type and the expected type are equal. We also allow
// integers when expecting doubles.
bool CheckType(const base::Value* value, base::Value::Type expected_type) {
  return value->type() == expected_type ||
         (value->is_int() && expected_type == base::Value::Type::DOUBLE);
}

// Returns true if |type| is supported as schema's 'type' value.
bool IsValidType(const std::string& type) {
  return MapSchemaKeyToValueType(type, kSchemaTypesToValueTypes,
                                 kSchemaTypesToValueTypesEnd, nullptr);
}

// Validate that |dict| only contains attributes that are allowed for the
// corresponding value of 'type'. Also ensure that all of those attributes are
// of the expected type. |options| can be used to ignore unknown attributes.
base::expected<void, std::string> ValidateAttributesAndTypes(
    const base::Value::Dict& dict,
    const std::string& type,
    int options) {
  const SchemaKeyToValueType* begin = nullptr;
  const SchemaKeyToValueType* end = nullptr;
  if (type == schema::kArray) {
    begin = kAttributesAndTypesForArray;
    end = kAttributesAndTypesForArrayEnd;
  } else if (type == schema::kBoolean) {
    begin = kAttributesAndTypesForBoolean;
    end = kAttributesAndTypesForBooleanEnd;
  } else if (type == schema::kInteger) {
    begin = kAttributesAndTypesForInteger;
    end = kAttributesAndTypesForIntegerEnd;
  } else if (type == schema::kNumber) {
    begin = kAttributesAndTypesForNumber;
    end = kAttributesAndTypesForNumberEnd;
  } else if (type == schema::kObject) {
    begin = kAttributesAndTypesForObject;
    end = kAttributesAndTypesForObjectEnd;
  } else if (type == schema::kRef) {
    begin = kAttributesAndTypesForRef;
    end = kAttributesAndTypesForRefEnd;
  } else if (type == schema::kString) {
    begin = kAttributesAndTypesForString;
    end = kAttributesAndTypesForStringEnd;
  } else {
    NOTREACHED_IN_MIGRATION()
        << "Type should be a valid schema type or '$ref'.";
  }

  base::Value::Type expected_type = base::Value::Type::NONE;
  for (auto it : dict) {
    if (MapSchemaKeyToValueType(it.first, begin, end, &expected_type)) {
      if (!CheckType(&it.second, expected_type)) {
        return base::unexpected(base::StringPrintf(
            "Invalid type for attribute '%s'", it.first.c_str()));
      }
    } else if (!IgnoreUnknownAttributes(options)) {
      return base::unexpected(
          base::StringPrintf("Unknown attribute '%s'", it.first.c_str()));
    }
  }
  return base::ok();
}

// Validates that |enum_list| is a list and its items are all of type |type|.
base::expected<void, std::string> ValidateEnum(const base::Value* enum_list,
                                               const std::string& type) {
  if (!enum_list->is_list() || enum_list->GetList().empty()) {
    return base::unexpected("Attribute 'enum' must be a non-empty list.");
  }
  base::Value::Type expected_item_type = base::Value::Type::NONE;
  MapSchemaKeyToValueType(type, kSchemaTypesToValueTypes,
                          kSchemaTypesToValueTypesEnd, &expected_item_type);
  for (const base::Value& item : enum_list->GetList()) {
    if (item.type() != expected_item_type) {
      return base::unexpected(base::StringPrintf(
          "Attribute 'enum' for type '%s' contains items with invalid types",
          type.c_str()));
    }
  }
  return base::ok();
}

// Forward declaration (used in ValidateProperties).
base::expected<void, std::string> IsValidSchema(const base::Value::Dict& dict,
                                                int options);

// Validates that the values in the |properties| dict are valid schemas.
base::expected<void, std::string> ValidateProperties(
    const base::Value::Dict& properties,
    int options) {
  for (auto dict_it : properties) {
    if (!dict_it.second.is_dict()) {
      return base::unexpected(base::StringPrintf(
          "Schema for property '%s' must be a dict.", dict_it.first.c_str()));
    }
    RETURN_IF_ERROR(IsValidSchema(dict_it.second.GetDict(), options));
  }
  return base::ok();
}

base::expected<void, std::string> IsFieldTypeObject(
    const base::Value& field,
    const std::string& field_name) {
  if (!field.is_dict()) {
    return base::unexpected(base::StringPrintf("Field '%s' must be an object.",
                                               field_name.c_str()));
  }
  return base::ok();
}

// Checks whether the passed dict is a valid schema. See
// |kAllowedAttributesAndTypes| for a list of supported types, supported
// attributes and their expected types. Values for 'minimum' and 'maximum' for
// type 'integer' can be of type int or double. Referenced IDs ($ref) are not
// checked for existence and IDs are not checked for duplicates. The 'pattern'
// attribute and keys for 'patternProperties' are not checked for valid regular
// expression syntax. Invalid regular expressions will cause a value validation
// error.
base::expected<void, std::string> IsValidSchema(const base::Value::Dict& dict,
                                                int options) {
  // Validate '$ref'.
  if (dict.contains(schema::kRef)) {
    return ValidateAttributesAndTypes(dict, schema::kRef, options);
  }

  // Validate 'type'.
  if (!dict.contains(schema::kType)) {
    return base::unexpected("Each schema must have a 'type' or '$ref'.");
  }

  const std::string* type = dict.FindString(schema::kType);
  if (!type) {
    return base::unexpected("Attribute 'type' must be a string.");
  }
  const std::string& type_string = *type;
  if (!IsValidType(type_string)) {
    return base::unexpected(
        base::StringPrintf("Unknown type '%s'.", type_string.c_str()));
  }

  // Validate attributes and expected types.
  RETURN_IF_ERROR(ValidateAttributesAndTypes(dict, type_string, options));

  // Validate 'enum' attribute.
  if (type_string == schema::kString || type_string == schema::kInteger) {
    if (const base::Value* enum_list = dict.Find(schema::kEnum)) {
      RETURN_IF_ERROR(ValidateEnum(enum_list, type_string));
    }
  }

  // TODO(b/341873894): Refactor type validation to helper functions.
  if (type_string == schema::kInteger) {
    // Validate 'minimum' > 'maximum'.
    const std::optional<double> minimum_value =
        dict.FindDouble(schema::kMinimum);
    const std::optional<double> maximum_value =
        dict.FindDouble(schema::kMaximum);
    if (minimum_value && maximum_value) {
      if (minimum_value.value() > maximum_value.value()) {
        return base::unexpected(
            base::StringPrintf("Invalid range specified [%f;%f].",
                               minimum_value.value(), maximum_value.value()));
      }
    }
  } else if (type_string == schema::kArray) {
    // Validate type 'array'.
    const base::Value* items = dict.Find(schema::kItems);
    if (!items || !items->is_dict()) {
      return base::unexpected(
          "Schema of type 'array' must have a schema in 'items' of type "
          "dictionary.");
    }
    RETURN_IF_ERROR(IsValidSchema(items->GetDict(), options));
  } else if (type_string == schema::kObject) {
    // Validate type 'object'.
    const base::Value* properties = dict.Find(schema::kProperties);
    if (properties) {
      RETURN_IF_ERROR(IsFieldTypeObject(*properties, schema::kProperties));
      RETURN_IF_ERROR(ValidateProperties(properties->GetDict(), options));
    }

    if (const base::Value* pattern_properties =
            dict.Find(schema::kPatternProperties)) {
      RETURN_IF_ERROR(
          IsFieldTypeObject(*pattern_properties, schema::kPatternProperties));
      RETURN_IF_ERROR(
          ValidateProperties(pattern_properties->GetDict(), options));
    }

    if (const base::Value* additional_properties =
            dict.Find(schema::kAdditionalProperties)) {
      RETURN_IF_ERROR(IsFieldTypeObject(*additional_properties,
                                        schema::kAdditionalProperties));
      RETURN_IF_ERROR(IsValidSchema(additional_properties->GetDict(), options));
    }

    if (const base::Value::List* required = dict.FindList(schema::kRequired)) {
      for (const base::Value& item : *required) {
        if (!item.is_string()) {
          return base::unexpected(
              "Attribute 'required' may only contain strings.");
        }
        const std::string property_name = item.GetString();
        if (!properties || !properties->GetDict().contains(property_name)) {
          return base::unexpected(base::StringPrintf(
              "Attribute 'required' contains unknown property '%s'.",
              property_name.c_str()));
        }
      }
    }
  }

  return base::ok();
}

}  // namespace

// Contains the internal data representation of a Schema. This can either wrap
// a SchemaData owned elsewhere (currently used to wrap the Chrome schema, which
// is generated at compile time), or it can own its own SchemaData.
class Schema::InternalStorage
    : public base::RefCountedThreadSafe<InternalStorage> {
 public:
  InternalStorage(const InternalStorage&) = delete;
  InternalStorage& operator=(const InternalStorage&) = delete;

  static scoped_refptr<const InternalStorage> Wrap(const SchemaData* data);

  static base::expected<scoped_refptr<const InternalStorage>, std::string>
  ParseSchema(const base::Value::Dict& schema);

  const SchemaData* data() const { return &schema_data_; }

  const SchemaNode* root_node() const { return schema(0); }

  // Returns the validation_schema root node if one was generated, or nullptr.
  const SchemaNode* validation_schema_root_node() const {
    return schema_data_.validation_schema_root_index >= 0
               ? schema(schema_data_.validation_schema_root_index)
               : nullptr;
  }

  const SchemaNode* schema(int index) const {
    DCHECK_GE(index, 0);
    return schema_data_.schema_nodes + index;
  }

  const PropertiesNode* properties(int index) const {
    DCHECK_GE(index, 0);
    return schema_data_.properties_nodes + index;
  }

  const PropertyNode* property(int index) const {
    DCHECK_GE(index, 0);
    return schema_data_.property_nodes + index;
  }

  const RestrictionNode* restriction(int index) const {
    DCHECK_GE(index, 0);
    return schema_data_.restriction_nodes + index;
  }

  const char* const* required_property(int index) const {
    DCHECK_GE(index, 0);
    return schema_data_.required_properties + index;
  }

  const int* int_enums(int index) const {
    DCHECK_GE(index, 0);
    return schema_data_.int_enums + index;
  }

  const char* const* string_enums(int index) const {
    DCHECK_GE(index, 0);
    return schema_data_.string_enums + index;
  }

  // Compiles regular expression |pattern|. The result is cached and will be
  // returned directly next time.
  re2::RE2* CompileRegex(const std::string& pattern) const;

 private:
  friend class base::RefCountedThreadSafe<InternalStorage>;

  InternalStorage();
  ~InternalStorage();

  // Determines the expected |sizes| of the storage for the representation
  // of |schema|.
  static void DetermineStorageSizes(const base::Value::Dict& schema,
                                    StorageSizes* sizes);

  // Parses the JSON schema in |schema|.
  //
  // If |schema| has a "$ref" attribute then a pending reference is appended
  // to the |reference_list|, and nothing else is done.
  //
  // Otherwise, |index| gets assigned the index of the corresponding SchemaNode
  // in |schema_nodes_|. If the |schema| contains an "id" then that ID is mapped
  // to the |index| in the |id_map|.
  //
  // If |schema| is invalid, it returns an error reason.
  base::expected<void, std::string> Parse(const base::Value::Dict& schema,
                                          short* index,
                                          ReferencesAndIDs* references_and_ids);

  // Helper for Parse() that gets an already assigned |schema_node| instead of
  // an |index| pointer.
  base::expected<void, std::string> ParseDictionary(
      const base::Value::Dict& schema,
      SchemaNode* schema_node,
      ReferencesAndIDs* references_and_ids);

  // Helper for Parse() that gets an already assigned |schema_node| instead of
  // an |index| pointer.
  base::expected<void, std::string> ParseList(
      const base::Value::Dict& schema,
      SchemaNode* schema_node,
      ReferencesAndIDs* references_and_ids);

  base::expected<void, std::string> ParseEnum(const base::Value::Dict& schema,
                                              base::Value::Type type,
                                              SchemaNode* schema_node);

  base::expected<void, std::string> ParseRangedInt(
      const base::Value::Dict& schema,
      SchemaNode* schema_node);

  base::expected<void, std::string> ParseStringPattern(
      const base::Value::Dict& schema,
      SchemaNode* schema_node);

  // Assigns the IDs in |id_map| to the pending references in the
  // |reference_list|. If an ID is missing then |error| is set and false is
  // returned; otherwise returns true.
  static bool ResolveReferences(const ReferencesAndIDs& references_and_ids,
                                std::string* error);

  // Sets |has_sensitive_children| for all |SchemaNode|s in |schema_nodes_|.
  void FindSensitiveChildren();

  // Returns true iff the node at |index| has sensitive child elements or
  // contains a sensitive value itself.
  bool FindSensitiveChildrenRecursive(int index,
                                      std::set<int>* handled_schema_nodes);

  // Cache for CompileRegex(), will memorize return value of every call to
  // CompileRegex() and return results directly next time.
  mutable std::map<std::string, std::unique_ptr<re2::RE2>> regex_cache_;

  SchemaData schema_data_;
  std::vector<std::string> strings_;
  std::vector<SchemaNode> schema_nodes_;
  std::vector<PropertyNode> property_nodes_;
  std::vector<PropertiesNode> properties_nodes_;
  std::vector<RestrictionNode> restriction_nodes_;
  std::vector<const char*> required_properties_;
  std::vector<int> int_enums_;
  std::vector<const char*> string_enums_;
};

Schema::InternalStorage::InternalStorage() = default;

Schema::InternalStorage::~InternalStorage() = default;

// static
scoped_refptr<const Schema::InternalStorage> Schema::InternalStorage::Wrap(
    const SchemaData* data) {
  InternalStorage* storage = new InternalStorage();
  storage->schema_data_ = *data;
  return storage;
}

// static
base::expected<scoped_refptr<const Schema::InternalStorage>, std::string>
Schema::InternalStorage::ParseSchema(const base::Value::Dict& schema) {
  // Determine the sizes of the storage arrays and reserve the capacity before
  // starting to append nodes and strings. This is important to prevent the
  // arrays from being reallocated, which would invalidate the c_str() pointers
  // and the addresses of indices to fix.
  StorageSizes sizes;
  DetermineStorageSizes(schema, &sizes);

  scoped_refptr<InternalStorage> storage = new InternalStorage();
  storage->strings_.reserve(sizes.strings);
  storage->schema_nodes_.reserve(sizes.schema_nodes);
  storage->property_nodes_.reserve(sizes.property_nodes);
  storage->properties_nodes_.reserve(sizes.properties_nodes);
  storage->restriction_nodes_.reserve(sizes.restriction_nodes);
  storage->required_properties_.reserve(sizes.required_properties);
  storage->int_enums_.reserve(sizes.int_enums);
  storage->string_enums_.reserve(sizes.string_enums);

  short root_index = kInvalid;
  ReferencesAndIDs references_and_ids;

  RETURN_IF_ERROR(storage->Parse(schema, &root_index, &references_and_ids));

  if (root_index == kInvalid) {
    return base::unexpected("The main schema can't have a $ref");
  }

  // None of this should ever happen without having been already detected.
  // But, if it does happen, then it will lead to corrupted memory; drop
  // everything in that case.
  if (root_index != 0 || sizes.strings != storage->strings_.size() ||
      sizes.schema_nodes != storage->schema_nodes_.size() ||
      sizes.property_nodes != storage->property_nodes_.size() ||
      sizes.properties_nodes != storage->properties_nodes_.size() ||
      sizes.restriction_nodes != storage->restriction_nodes_.size() ||
      sizes.required_properties != storage->required_properties_.size() ||
      sizes.int_enums != storage->int_enums_.size() ||
      sizes.string_enums != storage->string_enums_.size()) {
    return base::unexpected(
        "Failed to parse the schema due to a Chrome bug. Please file a "
        "new issue at http://crbug.com");
  }

  std::string error;
  if (!ResolveReferences(references_and_ids, &error)) {
    return base::unexpected(error);
  }

  storage->FindSensitiveChildren();

  SchemaData* data = &storage->schema_data_;
  data->schema_nodes = storage->schema_nodes_.data();
  data->property_nodes = storage->property_nodes_.data();
  data->properties_nodes = storage->properties_nodes_.data();
  data->restriction_nodes = storage->restriction_nodes_.data();
  data->required_properties = storage->required_properties_.data();
  data->int_enums = storage->int_enums_.data();
  data->string_enums = storage->string_enums_.data();
  data->validation_schema_root_index = -1;

  return base::ok(std::move(storage));
}

re2::RE2* Schema::InternalStorage::CompileRegex(
    const std::string& pattern) const {
  auto it = regex_cache_.find(pattern);
  if (it == regex_cache_.end()) {
    std::unique_ptr<re2::RE2> compiled(new re2::RE2(pattern));
    re2::RE2* compiled_ptr = compiled.get();
    regex_cache_.insert(std::make_pair(pattern, std::move(compiled)));
    return compiled_ptr;
  }
  return it->second.get();
}

// static
void Schema::InternalStorage::DetermineStorageSizes(
    const base::Value::Dict& schema,
    StorageSizes* sizes) {
  if (schema.FindString(schema::kRef)) {
    // Schemas with a "$ref" attribute don't take additional storage.
    return;
  }

  base::Value::Type type = base::Value::Type::NONE;
  const std::string* type_string = schema.FindString(schema::kType);
  if (!type_string || !SchemaTypeToValueType(*type_string, &type)) {
    // This schema is invalid.
    return;
  }

  sizes->schema_nodes++;

  if (type == base::Value::Type::LIST) {
    const base::Value* items = schema.Find(schema::kItems);
    if (items && items->is_dict()) {
      DetermineStorageSizes(items->GetDict(), sizes);
    }
  } else if (type == base::Value::Type::DICT) {
    sizes->properties_nodes++;

    const base::Value* additional_properties =
        schema.Find(schema::kAdditionalProperties);
    if (additional_properties && additional_properties->is_dict()) {
      DetermineStorageSizes(additional_properties->GetDict(), sizes);
    }

    const base::Value::Dict* properties = schema.FindDict(schema::kProperties);
    if (properties) {
      for (auto property : *properties) {
        if (property.second.is_dict()) {
          DetermineStorageSizes(property.second.GetDict(), sizes);
        }
        sizes->strings++;
        sizes->property_nodes++;
      }
    }

    const base::Value::Dict* pattern_properties =
        schema.FindDict(schema::kPatternProperties);
    if (pattern_properties) {
      for (auto pattern_property : *pattern_properties) {
        if (pattern_property.second.is_dict()) {
          DetermineStorageSizes(pattern_property.second.GetDict(), sizes);
        }
        sizes->strings++;
        sizes->property_nodes++;
      }
    }

    const base::Value::List* required_properties =
        schema.FindList(schema::kRequired);
    if (required_properties) {
      sizes->strings += required_properties->size();
      sizes->required_properties += required_properties->size();
    }
  } else if (schema.FindList(schema::kEnum)) {
    const base::Value::List* possible_values = schema.FindList(schema::kEnum);
    if (possible_values) {
      size_t num_possible_values = possible_values->size();
      if (type == base::Value::Type::INTEGER) {
        sizes->int_enums += num_possible_values;
      } else if (type == base::Value::Type::STRING) {
        sizes->string_enums += num_possible_values;
        sizes->strings += num_possible_values;
      }
      sizes->restriction_nodes++;
    }
  } else if (type == base::Value::Type::INTEGER) {
    if (schema.contains(schema::kMinimum) ||
        schema.contains(schema::kMaximum)) {
      sizes->restriction_nodes++;
    }
  } else if (type == base::Value::Type::STRING) {
    if (schema.contains(schema::kPattern)) {
      sizes->strings++;
      sizes->string_enums++;
      sizes->restriction_nodes++;
    }
  }
}

base::expected<void, std::string> Schema::InternalStorage::Parse(
    const base::Value::Dict& schema,
    short* index,
    ReferencesAndIDs* references_and_ids) {
  const std::string* ref = schema.FindString(schema::kRef);
  if (ref) {
    if (schema.FindString(schema::kId)) {
      return base::unexpected("Schemas with a $ref can't have an id");
    }
    references_and_ids->reference_list.emplace_back(*ref, index);
    return base::ok();
  }

  const std::string* type_string = schema.FindString(schema::kType);
  if (!type_string) {
    return base::unexpected("The schema type must be declared.");
  }

  base::Value::Type type = base::Value::Type::NONE;
  if (!SchemaTypeToValueType(*type_string, &type)) {
    return base::unexpected("Type not supported: " + *type_string);
  }

  if (schema_nodes_.size() > std::numeric_limits<short>::max()) {
    return base::unexpected(
        "Can't have more than " +
        base::NumberToString(std::numeric_limits<short>::max()) +
        " schema nodes.");
  }
  *index = static_cast<short>(schema_nodes_.size());
  schema_nodes_.push_back(
      {.type = type,
       .extra = kInvalid,
       .is_sensitive_value =
           schema.FindBool(schema::kSensitiveValue).value_or(false)});
  SchemaNode* schema_node = &schema_nodes_.back();

  if (type == base::Value::Type::DICT) {
    RETURN_IF_ERROR(ParseDictionary(schema, schema_node, references_and_ids));
  } else if (type == base::Value::Type::LIST) {
    RETURN_IF_ERROR(ParseList(schema, schema_node, references_and_ids));
  } else if (schema.contains(schema::kEnum)) {
    RETURN_IF_ERROR(ParseEnum(schema, type, schema_node));
  } else if (schema.contains(schema::kPattern)) {
    RETURN_IF_ERROR(ParseStringPattern(schema, schema_node));
  } else if (schema.contains(schema::kMinimum) ||
             schema.contains(schema::kMaximum)) {
    if (type != base::Value::Type::INTEGER) {
      return base::unexpected("Only integers can have minimum and maximum");
    }
    RETURN_IF_ERROR(ParseRangedInt(schema, schema_node));
  }
  const std::string* id = schema.FindString(schema::kId);
  if (id) {
    auto& id_map = references_and_ids->id_map;
    if (base::Contains(id_map, *id)) {
      return base::unexpected("Duplicated id: " + *id);
    }
    id_map[*id] = *index;
  }

  return base::ok();
}

base::expected<void, std::string> Schema::InternalStorage::ParseDictionary(
    const base::Value::Dict& schema,
    SchemaNode* schema_node,
    ReferencesAndIDs* references_and_ids) {
  int extra = static_cast<int>(properties_nodes_.size());
  properties_nodes_.push_back({.additional = kInvalid});
  schema_node->extra = extra;

  const base::Value::Dict* additional_properties =
      schema.FindDict(schema::kAdditionalProperties);
  if (additional_properties) {
    RETURN_IF_ERROR(Parse(*additional_properties,
                          &properties_nodes_[extra].additional,
                          references_and_ids));
  }

  properties_nodes_[extra].begin = static_cast<int>(property_nodes_.size());

  const base::Value::Dict* properties = schema.FindDict(schema::kProperties);
  if (properties) {
    // This and below reserves nodes for all of the |properties|, and makes sure
    // they are contiguous. Recursive calls to Parse() will append after these
    // elements.
    property_nodes_.resize(property_nodes_.size() + properties->size());
  }

  properties_nodes_[extra].end = static_cast<int>(property_nodes_.size());

  const base::Value::Dict* pattern_properties =
      schema.FindDict(schema::kPatternProperties);
  if (pattern_properties) {
    property_nodes_.resize(property_nodes_.size() + pattern_properties->size());
  }

  properties_nodes_[extra].pattern_end =
      static_cast<int>(property_nodes_.size());

  if (properties != nullptr) {
    int base_index = properties_nodes_[extra].begin;
    int index = base_index;

    for (auto property : *properties) {
      strings_.push_back(property.first);
      property_nodes_[index].key = strings_.back().c_str();
      if (!property.second.is_dict()) {
        return base::unexpected(std::string());
      }
      RETURN_IF_ERROR(Parse(property.second.GetDict(),
                            &property_nodes_[index].schema,
                            references_and_ids));
      ++index;
    }
    CHECK_EQ(static_cast<int>(properties->size()), index - base_index);
  }

  if (pattern_properties != nullptr) {
    int base_index = properties_nodes_[extra].end;
    int index = base_index;

    for (auto pattern_property : *pattern_properties) {
      re2::RE2* compiled_regex = CompileRegex(pattern_property.first);
      if (!compiled_regex->ok()) {
        return base::unexpected(
            "/" + pattern_property.first +
            "/ is a invalid regex: " + compiled_regex->error());
      }
      strings_.push_back(pattern_property.first);
      property_nodes_[index].key = strings_.back().c_str();
      if (!pattern_property.second.is_dict()) {
        return base::unexpected(std::string());
      }
      RETURN_IF_ERROR(Parse(pattern_property.second.GetDict(),
                            &property_nodes_[index].schema,
                            references_and_ids));
      ++index;
    }
    CHECK_EQ(static_cast<int>(pattern_properties->size()), index - base_index);
  }

  properties_nodes_[extra].required_begin = required_properties_.size();
  const base::Value::List* required_properties =
      schema.FindList(schema::kRequired);
  if (required_properties) {
    for (const base::Value& val : *required_properties) {
      strings_.push_back(val.GetString());
      required_properties_.push_back(strings_.back().c_str());
    }
  }
  properties_nodes_[extra].required_end = required_properties_.size();

  if (properties_nodes_[extra].begin == properties_nodes_[extra].pattern_end) {
    properties_nodes_[extra].begin = kInvalid;
    properties_nodes_[extra].end = kInvalid;
    properties_nodes_[extra].pattern_end = kInvalid;
    properties_nodes_[extra].required_begin = kInvalid;
    properties_nodes_[extra].required_end = kInvalid;
  }

  return base::ok();
}

base::expected<void, std::string> Schema::InternalStorage::ParseList(
    const base::Value::Dict& schema,
    SchemaNode* schema_node,
    ReferencesAndIDs* references_and_ids) {
  const base::Value::Dict* items = schema.FindDict(schema::kItems);
  if (!items) {
    return base::unexpected(
        "Arrays must declare a single schema for their items.");
  }
  return Parse(*items, &schema_node->extra, references_and_ids);
}

base::expected<void, std::string> Schema::InternalStorage::ParseEnum(
    const base::Value::Dict& schema,
    base::Value::Type type,
    SchemaNode* schema_node) {
  const base::Value::List* possible_values = schema.FindList(schema::kEnum);
  if (!possible_values) {
    return base::unexpected("Enum attribute must be a list value");
  }
  if (possible_values->empty()) {
    return base::unexpected("Enum attribute must be non-empty");
  }
  int offset_begin;
  int offset_end;
  if (type == base::Value::Type::INTEGER) {
    offset_begin = static_cast<int>(int_enums_.size());
    for (const auto& possible_value : *possible_values) {
      if (!possible_value.is_int()) {
        return base::unexpected("Invalid enumeration member type");
      }
      int_enums_.push_back(possible_value.GetInt());
    }
    offset_end = static_cast<int>(int_enums_.size());
  } else if (type == base::Value::Type::STRING) {
    offset_begin = static_cast<int>(string_enums_.size());
    for (const auto& possible_value : *possible_values) {
      if (!possible_value.is_string()) {
        return base::unexpected("Invalid enumeration member type");
      }
      strings_.push_back(possible_value.GetString());
      string_enums_.push_back(strings_.back().c_str());
    }
    offset_end = static_cast<int>(string_enums_.size());
  } else {
    return base::unexpected(
        "Enumeration is only supported for integer and string.");
  }
  schema_node->extra = static_cast<int>(restriction_nodes_.size());
  restriction_nodes_.push_back(RestrictionNode{
      .enumeration_restriction = RestrictionNode::EnumerationRestriction{
          .offset_begin = offset_begin, .offset_end = offset_end}});
  return base::ok();
}

base::expected<void, std::string> Schema::InternalStorage::ParseRangedInt(
    const base::Value::Dict& schema,
    SchemaNode* schema_node) {
  int min_value = schema.FindInt(schema::kMinimum).value_or(INT_MIN);
  int max_value = schema.FindInt(schema::kMaximum).value_or(INT_MAX);
  if (min_value > max_value) {
    return base::unexpected("Invalid range restriction for int type.");
  }
  schema_node->extra = static_cast<int>(restriction_nodes_.size());
  restriction_nodes_.push_back(
      RestrictionNode{.ranged_restriction = RestrictionNode::RangedRestriction{
                          .max_value = max_value, .min_value = min_value}});
  return base::ok();
}

base::expected<void, std::string> Schema::InternalStorage::ParseStringPattern(
    const base::Value::Dict& schema,
    SchemaNode* schema_node) {
  const std::string* pattern = schema.FindString(schema::kPattern);
  if (!pattern) {
    return base::unexpected("Schema pattern must be a string.");
  }
  re2::RE2* compiled_regex = CompileRegex(*pattern);
  if (!compiled_regex->ok()) {
    return base::unexpected("/" + *pattern +
                            "/ is invalid regex: " + compiled_regex->error());
  }
  int index = static_cast<int>(string_enums_.size());
  strings_.push_back(*pattern);
  string_enums_.push_back(strings_.back().c_str());
  schema_node->extra = static_cast<int>(restriction_nodes_.size());
  restriction_nodes_.push_back(RestrictionNode{
      .string_pattern_restriction = RestrictionNode::StringPatternRestriction{
          .pattern_index = index, .pattern_index_backup = index}});
  return base::ok();
}

// static
bool Schema::InternalStorage::ResolveReferences(
    const ReferencesAndIDs& references_and_ids,
    std::string* error) {
  const auto& reference_list = references_and_ids.reference_list;
  const auto& id_map = references_and_ids.id_map;
  for (auto& ref : reference_list) {
    auto id = id_map.find(ref.first);
    if (id == id_map.end()) {
      *error = "Invalid $ref: " + ref.first;
      return false;
    }
    *ref.second = id->second;
  }
  return true;
}

void Schema::InternalStorage::FindSensitiveChildren() {
  if (schema_nodes_.empty())
    return;

  std::set<int> handled_schema_nodes;
  FindSensitiveChildrenRecursive(0, &handled_schema_nodes);
}

bool Schema::InternalStorage::FindSensitiveChildrenRecursive(
    int index,
    std::set<int>* handled_schema_nodes) {
  DCHECK(static_cast<unsigned long>(index) < schema_nodes_.size());
  SchemaNode& schema_node = schema_nodes_[index];
  if (handled_schema_nodes->find(index) != handled_schema_nodes->end())
    return schema_node.has_sensitive_children || schema_node.is_sensitive_value;

  handled_schema_nodes->insert(index);
  bool has_sensitive_children = false;
  if (schema_node.type == base::Value::Type::DICT) {
    const PropertiesNode& properties_node =
        properties_nodes_[schema_node.extra];
    // Iterate through properties and patternProperties.
    for (int i = properties_node.begin; i < properties_node.pattern_end; ++i) {
      int sub_index = property_nodes_[i].schema;
      has_sensitive_children |=
          FindSensitiveChildrenRecursive(sub_index, handled_schema_nodes);
    }
    if (properties_node.additional != kInvalid) {
      has_sensitive_children |= FindSensitiveChildrenRecursive(
          properties_node.additional, handled_schema_nodes);
    }
  } else if (schema_node.type == base::Value::Type::LIST) {
    int sub_index = schema_node.extra;
    has_sensitive_children |=
        FindSensitiveChildrenRecursive(sub_index, handled_schema_nodes);
  }
  schema_node.has_sensitive_children = has_sensitive_children;

  return schema_node.has_sensitive_children || schema_node.is_sensitive_value;
}

Schema::Iterator::Iterator(const scoped_refptr<const InternalStorage>& storage,
                           const PropertiesNode* node) {
  if (node->begin == kInvalid || node->end == kInvalid) {
    it_ = nullptr;
    end_ = nullptr;
  } else {
    storage_ = storage;
    it_ = storage->property(node->begin);
    end_ = storage->property(node->end);
  }
}

Schema::Iterator::Iterator(const Iterator& iterator)
    : storage_(iterator.storage_), it_(iterator.it_), end_(iterator.end_) {}

Schema::Iterator::~Iterator() = default;

Schema::Iterator& Schema::Iterator::operator=(const Iterator& iterator) {
  storage_ = iterator.storage_;
  it_ = iterator.it_;
  end_ = iterator.end_;
  return *this;
}

bool Schema::Iterator::IsAtEnd() const {
  return it_ == end_;
}

void Schema::Iterator::Advance() {
  DCHECK(it_);
  ++it_;
}

const char* Schema::Iterator::key() const {
  return it_->key;
}

Schema Schema::Iterator::schema() const {
  return Schema(storage_, storage_->schema(it_->schema));
}

Schema::Schema() : node_(nullptr) {}

Schema::Schema(const scoped_refptr<const InternalStorage>& storage,
               const SchemaNode* node)
    : storage_(storage), node_(node) {}

Schema::Schema(const Schema& schema)
    : storage_(schema.storage_), node_(schema.node_) {}

Schema::~Schema() = default;

Schema& Schema::operator=(const Schema& schema) {
  storage_ = schema.storage_;
  node_ = schema.node_;
  return *this;
}

// static
Schema Schema::Wrap(const SchemaData* data) {
  scoped_refptr<const InternalStorage> storage = InternalStorage::Wrap(data);
  return Schema(storage, storage->root_node());
}

bool Schema::Validate(const base::Value& value,
                      SchemaOnErrorStrategy strategy,
                      PolicyErrorPath* out_error_path,
                      std::string* out_error) const {
  if (!valid()) {
    SchemaErrorFound(out_error_path, out_error, "The schema is invalid.");
    return false;
  }

  if (value.type() != type()) {
    // Allow the integer to double promotion. Note that range restriction on
    // double is not supported now.
    if (value.is_int() && type() == base::Value::Type::DOUBLE) {
      return true;
    }

    SchemaErrorFound(
        out_error_path, out_error,
        base::StringPrintf(
            "Policy type mismatch: expected: \"%s\", actual: \"%s\".",
            base::Value::GetTypeName(type()),
            base::Value::GetTypeName(value.type())));
    return false;
  }

  if (value.is_dict()) {
    base::flat_set<std::string> present_properties;
    for (auto dict_item : value.GetDict()) {
      SchemaList schema_list = GetMatchingProperties(dict_item.first);
      if (schema_list.empty()) {
        // Unknown property was detected.
        if (!StrategyAllowUnknownWithoutWarning(strategy)) {
          SchemaErrorFound(out_error_path, out_error,
                           "Unknown property: " + dict_item.first);
        }
        if (!StrategyAllowUnknown(strategy))
          return false;
      } else {
        for (const auto& subschema : schema_list) {
          std::string new_error;
          const bool validation_result = subschema.Validate(
              dict_item.second, strategy, out_error_path, &new_error);
          if (!new_error.empty()) {
            AddDictKeyPrefixToPath(dict_item.first, out_error_path);
            if (out_error)
              *out_error = std::move(new_error);
          }
          if (!validation_result) {
            // Invalid property was detected.
            return false;
          }
        }
        present_properties.insert(dict_item.first);
      }
    }

    for (const auto& required_property : GetRequiredProperties()) {
      if (base::Contains(present_properties, required_property))
        continue;

      SchemaErrorFound(
          out_error_path, out_error,
          "Missing or invalid required property: " + required_property);
      return false;
    }
  } else if (value.is_list()) {
    for (size_t index = 0; index < value.GetList().size(); ++index) {
      const base::Value& list_item = value.GetList()[index];
      std::string new_error;
      const bool validation_result =
          GetItems().Validate(list_item, strategy, out_error_path, &new_error);
      if (!new_error.empty()) {
        AddListIndexPrefixToPath(index, out_error_path);
        if (out_error)
          *out_error = std::move(new_error);
      }
      if (!validation_result && !StrategyAllowInvalidListEntry(strategy))
        return false;  // Invalid list item was detected.
    }
  } else if (value.is_int()) {
    if (node_->extra != kInvalid &&
        !ValidateIntegerRestriction(node_->extra, value.GetInt())) {
      SchemaErrorFound(out_error_path, out_error, "Invalid value for integer");
      return false;
    }
  } else if (value.is_string()) {
    if (node_->extra != kInvalid &&
        !ValidateStringRestriction(node_->extra, value.GetString().c_str())) {
      SchemaErrorFound(out_error_path, out_error, "Invalid value for string");
      return false;
    }
  }

  return true;
}

bool Schema::Normalize(base::Value* value,
                       SchemaOnErrorStrategy strategy,
                       PolicyErrorPath* out_error_path,
                       std::string* out_error,
                       bool* out_changed) const {
  if (!valid()) {
    SchemaErrorFound(out_error_path, out_error, "The schema is invalid.");
    return false;
  }

  if (value->type() != type()) {
    // Allow the integer to double promotion. Note that range restriction on
    // double is not supported now.
    if (value->is_int() && type() == base::Value::Type::DOUBLE) {
      return true;
    }

    SchemaErrorFound(
        out_error_path, out_error,
        base::StringPrintf(
            "Policy type mismatch: expected: \"%s\", actual: \"%s\".",
            base::Value::GetTypeName(type()),
            base::Value::GetTypeName(value->type())));
    return false;
  }

  if (value->is_dict()) {
    base::flat_set<std::string> present_properties;
    std::vector<std::string> drop_list;  // Contains the keys to drop.
    for (auto dict_item : value->GetDict()) {
      SchemaList schema_list = GetMatchingProperties(dict_item.first);
      if (schema_list.empty()) {
        // Unknown property was detected.
        if (!StrategyAllowUnknownWithoutWarning(strategy)) {
          SchemaErrorFound(out_error_path, out_error,
                           "Unknown property: " + dict_item.first);
        }
        if (!StrategyAllowUnknown(strategy))
          return false;
        if (!StrategyAllowUnknownWithoutWarning(strategy)) {
          drop_list.push_back(dict_item.first);
        }
      } else {
        for (const auto& subschema : schema_list) {
          std::string new_error;
          const bool normalization_result =
              subschema.Normalize(&dict_item.second, strategy, out_error_path,
                                  &new_error, out_changed);
          if (!new_error.empty()) {
            AddDictKeyPrefixToPath(dict_item.first, out_error_path);
            if (out_error)
              *out_error = std::move(new_error);
          }
          if (!normalization_result) {
            // Invalid property was detected.
            return false;
          }
        }
        present_properties.insert(dict_item.first);
      }
    }

    for (const auto& required_property : GetRequiredProperties()) {
      if (base::Contains(present_properties, required_property))
        continue;

      SchemaErrorFound(
          out_error_path, out_error,
          "Missing or invalid required property: " + required_property);
      return false;
    }

    if (out_changed && !drop_list.empty())
      *out_changed = true;
    for (const auto& drop_key : drop_list)
      value->GetDict().Remove(drop_key);
    return true;
  } else if (value->is_list()) {
    base::Value::List& list = value->GetList();

    // Instead of removing invalid list items afterwards, we push valid items
    // forward in the list by overriding invalid items. The next free position
    // is indicated by |write_index|, which gets increased for every valid item.
    // At the end |list| is resized to |write_index|'s size.
    size_t write_index = 0;
    for (size_t index = 0; index < list.size(); ++index) {
      base::Value& list_item = list[index];
      std::string new_error;
      const bool normalization_result = GetItems().Normalize(
          &list_item, strategy, out_error_path, &new_error, out_changed);
      if (!new_error.empty()) {
        AddListIndexPrefixToPath(index, out_error_path);
        if (out_error)
          *out_error = new_error;
      }
      if (!normalization_result) {
        // Invalid list item was detected.
        if (!StrategyAllowInvalidListEntry(strategy))
          return false;
      } else {
        if (write_index != index)
          list[write_index] = std::move(list_item);
        ++write_index;
      }
    }
    if (out_changed && write_index < list.size())
      *out_changed = true;
    while (write_index < list.size()) {
      list.erase(list.end() - 1);
    }
    return true;
  }

  return Validate(*value, strategy, out_error_path, out_error);
}

void Schema::MaskSensitiveValues(base::Value* value) const {
  if (!valid())
    return;

  MaskSensitiveValuesRecursive(value);
}

// static
base::expected<Schema, std::string> Schema::Parse(const std::string& content) {
  // Validate as a generic JSON schema, and ignore unknown attributes; they
  // may become used in a future version of the schema format.
  ASSIGN_OR_RETURN(auto dict,
                   Schema::ParseToDictAndValidate(
                       content, kSchemaOptionsIgnoreUnknownAttributes));

  // Validate the main type.
  const std::string* type = dict.FindString(schema::kType);
  if (!type || *type != schema::kObject) {
    return base::unexpected(
        "The main schema must have a type attribute with \"object\" value.");
  }

  // Checks for invalid attributes at the top-level.
  if (dict.contains(schema::kAdditionalProperties) ||
      dict.contains(schema::kPatternProperties)) {
    return base::unexpected(
        "\"additionalProperties\" and \"patternProperties\" are not "
        "supported at the main schema.");
  }

  ASSIGN_OR_RETURN(auto storage, InternalStorage::ParseSchema(dict));
  return base::ok(Schema(storage, storage->root_node()));
}

// static
base::expected<base::Value::Dict, std::string> Schema::ParseToDictAndValidate(
    const std::string& schema,
    int validator_options) {
  ASSIGN_OR_RETURN(
      auto json,
      base::JSONReader::ReadAndReturnValueWithError(
          schema, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS |
                      base::JSONParserOptions::JSON_PARSE_CHROMIUM_EXTENSIONS),
      [](auto e) { return std::move(e.ToString()); });

  if (!json.is_dict()) {
    return base::unexpected("Schema must be a JSON object");
  }
  RETURN_IF_ERROR(IsValidSchema(json.GetDict(), validator_options));

  return base::ok(std::move(json).TakeDict());
}

base::Value::Type Schema::type() const {
  CHECK(valid());
  return node_->type;
}

Schema::Iterator Schema::GetPropertiesIterator() const {
  CHECK(valid());
  CHECK_EQ(base::Value::Type::DICT, type());
  return Iterator(storage_, storage_->properties(node_->extra));
}

namespace {

bool CompareKeys(const PropertyNode& node, const std::string& key) {
  return node.key < key;
}

}  // namespace

Schema Schema::GetKnownProperty(const std::string& key) const {
  CHECK(valid());
  CHECK_EQ(base::Value::Type::DICT, type());
  const PropertiesNode* node = storage_->properties(node_->extra);
  if (node->begin == kInvalid || node->end == kInvalid)
    return Schema();
  const PropertyNode* begin = storage_->property(node->begin);
  const PropertyNode* end = storage_->property(node->end);
  const PropertyNode* it = std::lower_bound(begin, end, key, CompareKeys);
  if (it != end && it->key == key)
    return Schema(storage_, storage_->schema(it->schema));
  return Schema();
}

Schema Schema::GetAdditionalProperties() const {
  CHECK(valid());
  CHECK_EQ(base::Value::Type::DICT, type());
  const PropertiesNode* node = storage_->properties(node_->extra);
  if (node->additional == kInvalid)
    return Schema();
  return Schema(storage_, storage_->schema(node->additional));
}

SchemaList Schema::GetPatternProperties(const std::string& key) const {
  CHECK(valid());
  CHECK_EQ(base::Value::Type::DICT, type());
  const PropertiesNode* node = storage_->properties(node_->extra);
  if (node->end == kInvalid || node->pattern_end == kInvalid)
    return {};
  const PropertyNode* begin = storage_->property(node->end);
  const PropertyNode* end = storage_->property(node->pattern_end);
  SchemaList matching_properties;
  for (const PropertyNode* it = begin; it != end; ++it) {
    if (re2::RE2::PartialMatch(key, *storage_->CompileRegex(it->key))) {
      matching_properties.push_back(
          Schema(storage_, storage_->schema(it->schema)));
    }
  }
  return matching_properties;
}

std::vector<std::string> Schema::GetRequiredProperties() const {
  CHECK(valid());
  CHECK_EQ(base::Value::Type::DICT, type());
  const PropertiesNode* node = storage_->properties(node_->extra);
  if (node->required_begin == kInvalid || node->required_end == kInvalid)
    return {};
  const size_t begin = node->required_begin;
  const size_t end = node->required_end;

  return std::vector<std::string>(storage_->required_property(begin),
                                  storage_->required_property(end));
}

Schema Schema::GetProperty(const std::string& key) const {
  Schema schema = GetKnownProperty(key);
  if (schema.valid())
    return schema;
  return GetAdditionalProperties();
}

SchemaList Schema::GetMatchingProperties(const std::string& key) const {
  SchemaList schema_list;

  Schema known_property = GetKnownProperty(key);
  if (known_property.valid())
    schema_list.push_back(known_property);

  SchemaList pattern_properties = GetPatternProperties(key);
  schema_list.insert(schema_list.end(), pattern_properties.begin(),
                     pattern_properties.end());

  if (schema_list.empty()) {
    Schema additional_property = GetAdditionalProperties();
    if (additional_property.valid())
      schema_list.push_back(additional_property);
  }

  return schema_list;
}

Schema Schema::GetItems() const {
  CHECK(valid());
  CHECK_EQ(base::Value::Type::LIST, type());
  if (node_->extra == kInvalid)
    return Schema();
  return Schema(storage_, storage_->schema(node_->extra));
}

bool Schema::ValidateIntegerRestriction(int index, int value) const {
  const RestrictionNode* rnode = storage_->restriction(index);
  if (rnode->ranged_restriction.min_value <=
      rnode->ranged_restriction.max_value) {
    return rnode->ranged_restriction.min_value <= value &&
           rnode->ranged_restriction.max_value >= value;
  } else {
    for (int i = rnode->enumeration_restriction.offset_begin;
         i < rnode->enumeration_restriction.offset_end; ++i) {
      if (*storage_->int_enums(i) == value)
        return true;
    }
    return false;
  }
}

bool Schema::ValidateStringRestriction(int index, const char* str) const {
  const RestrictionNode* rnode = storage_->restriction(index);
  if (rnode->enumeration_restriction.offset_begin <
      rnode->enumeration_restriction.offset_end) {
    for (int i = rnode->enumeration_restriction.offset_begin;
         i < rnode->enumeration_restriction.offset_end; ++i) {
      if (strcmp(*storage_->string_enums(i), str) == 0)
        return true;
    }
    return false;
  } else {
    int pattern_index = rnode->string_pattern_restriction.pattern_index;
    DCHECK(pattern_index ==
           rnode->string_pattern_restriction.pattern_index_backup);
    re2::RE2* regex =
        storage_->CompileRegex(*storage_->string_enums(pattern_index));
    return re2::RE2::PartialMatch(str, *regex);
  }
}

void Schema::MaskSensitiveValuesRecursive(base::Value* value) const {
  if (IsSensitiveValue()) {
    *value = base::Value(kSensitiveValueMask);
    return;
  }
  if (!HasSensitiveChildren())
    return;
  if (value->type() != type())
    return;

  if (value->is_dict()) {
    for (auto [key, sub_value] : value->GetDict()) {
      SchemaList schema_list = GetMatchingProperties(key);
      for (const auto& schema_item : schema_list)
        schema_item.MaskSensitiveValuesRecursive(&sub_value);
    }
  } else if (value->is_list()) {
    for (auto& list_elem : value->GetList())
      GetItems().MaskSensitiveValuesRecursive(&list_elem);
  }
}

Schema Schema::GetValidationSchema() const {
  CHECK(valid());
  const SchemaNode* validation_schema_root_node =
      storage_->validation_schema_root_node();
  if (!validation_schema_root_node)
    return Schema();
  return Schema(storage_, validation_schema_root_node);
}

bool Schema::IsSensitiveValue() const {
  CHECK(valid());

  // This is safe because |node_| is guaranteed to have been returned from
  // |storage_| and |storage_->root_node()| always returns to the |SchemaNode|
  // with index 0.
  int index = node_ - storage_->root_node();
  const SchemaNode* schema_node = storage_->schema(index);
  if (!schema_node)
    return false;
  return schema_node->is_sensitive_value;
}

bool Schema::HasSensitiveChildren() const {
  CHECK(valid());

  // This is safe because |node_| is guaranteed to have been returned from
  // |storage_| and |storage_->root_node()| always returns to the |SchemaNode|
  // with index 0.
  int index = node_ - storage_->root_node();
  const SchemaNode* schema_node = storage_->schema(index);
  if (!schema_node)
    return false;
  return schema_node->has_sensitive_children;
}

}  // namespace policy
