// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_SCHEMA_H_
#define COMPONENTS_POLICY_CORE_COMMON_SCHEMA_H_

#include <optional>
#include <string>
#include <vector>

#include "absl/types/variant.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/policy/policy_export.h"

namespace policy {
namespace internal {

struct POLICY_EXPORT SchemaData;
struct POLICY_EXPORT SchemaNode;
struct POLICY_EXPORT PropertyNode;
struct POLICY_EXPORT PropertiesNode;

}  // namespace internal

// The error path, which leads to an error occurred. Members of the
// error path can either be ints in case of list items or strings in case of
// dictionary keys.
using PolicyErrorPath = std::vector<absl::variant<int, std::string>>;

// Returns a formatted string for a given error path |error_path|, consisting
// of list indices and dict keys.
// For example, ErrorPathToString("TestPolicy", {4, "testField"}) will be
// encoded as "TestPolicy[4].testField"
POLICY_EXPORT std::string ErrorPathToString(const std::string& policy_name,
                                            PolicyErrorPath error_path);

// Option flags passed to Schema::Validate() and Schema::Normalize(), describing
// the strategy to handle unknown properties or invalid values for dict type.
// Note that in Schema::Normalize() allowed errors will be dropped and thus
// ignored.
// Unknown error indicates that some value in a dictionary (may or may not be
// the one in root) have unknown property name according to schema.
// Invalid error indicates a validation failure against the schema. As
// validation is done recursively, a validation failure of dict properties or
// list items might be ignored (or dropped in Normalize()) or trigger whole
// dictionary/list validation failure.
enum SchemaOnErrorStrategy {
  // No errors will be allowed. This should not be used for policies, since it
  // basically prevents future changes to the policy (Server sends newField, but
  // clients running older versions of Chrome reject the policy because they
  // don't know newField). Prefer to use |SCHEMA_ALLOW_UNKNOWN| or
  // |SCHEMA_ALLOW_UNKOWN_AND_INVALID_LIST_ENTRY| for policies
  // instead.
  SCHEMA_STRICT = 0,
  // Unknown properties in any dictionary will be ignored.
  SCHEMA_ALLOW_UNKNOWN,
  // In addition to the previous, invalid list entries will be ignored for all
  // lists in the schema. Should only be used in cases where dropping list items
  // is safe. For example, can't be used if an empty list has a special meaning,
  // like allowing everything.
  SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY,
  // Same as |SCHEMA_ALLOW_UNKNOWN|, but unknown properties won't cause errors
  // messages to be added. Used to allow adding extra fields to the policy
  // internally, without adding those fields to the schema. This option should
  // be avoided, since it suppresses the errors.
  SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING,
};

// Schema validation options for Schema::ParseToDictAndValidate().
constexpr int kSchemaOptionsNone = 0;
constexpr int kSchemaOptionsIgnoreUnknownAttributes = 1 << 0;

// String used to hide sensitive policy values.
// It should be consistent with the mask |NetworkConfigurationPolicyHandler|
// uses for network credential fields.
extern const char kSensitiveValueMask[];

class Schema;

typedef std::vector<Schema> SchemaList;

// Describes the expected type of one policy. Also recursively describes the
// types of inner elements, for structured types.
// Objects of this class refer to external, immutable data and are cheap to
// copy.
//
// See components/policy/core/common/json_schema_constants.h for a list of
// supported features and data types. Only these features and data-types are
// supported and enforced. For the full schema proposal see
// https://json-schema.org/understanding-json-schema/index.html.
//
// There are also these departures from the proposal:
//  - "additionalProperties": false is not supported. The value of
//    "additionalProperties" has to be a schema if present. Otherwise, the
//    behavior for unknown attributes is controlled by |SchemaOnErrorStrategy|.
//  - "sensitiveValue" (bool) marks a value to be sensitive. This is used to
//    mask those values in the UI by calling |MaskSensitiveValues()|.
class POLICY_EXPORT Schema {
 public:
  // Used internally to store shared data.
  class InternalStorage;

  // Builds an empty, invalid schema.
  Schema();

  // Makes a copy of |schema| that shares the same internal storage.
  Schema(const Schema& schema);

  ~Schema();

  Schema& operator=(const Schema& schema);

  // Returns a Schema that references static data. This can be used by
  // the embedder to pass structures generated at compile time, which can then
  // be quickly loaded at runtime.
  static Schema Wrap(const internal::SchemaData* data);

  // Parses a JSON schema. If the input `content` represents a valid schema,
  // returns a Schema. Otherwise, returns an error message containing a reason
  // for the failure.
  static base::expected<Schema, std::string> Parse(const std::string& content);

  // Verifies if |schema| is a valid JSON v3 schema. When this validation passes
  // then |schema| is valid JSON that can be parsed into a Value::Dict which can
  // be used to build a |Schema|. Returns the parsed Value::Dict when |schema|
  // validated, otherwise returns an error description. For performance reasons,
  // currently IsValidSchema() won't check the correctness of regular
  // expressions used in "pattern" and "patternProperties" and in Validate()
  // invalid regular expression don't accept any strings. |options| is a
  // bitwise-OR combination of the options above (see |kSchemaOptions*| above).
  static base::expected<base::Value::Dict, std::string> ParseToDictAndValidate(
      const std::string& schema,
      int options);

  // Returns true if this Schema is valid. Schemas returned by the methods below
  // may be invalid, and in those cases the other methods must not be used.
  bool valid() const { return !!node_; }

  base::Value::Type type() const;

  // Validate |value| against current schema, |strategy| is the strategy to
  // handle unknown properties or invalid values. Allowed errors will be
  // ignored. |out_error_path| and |out_error| will contain the last error
  // location and detailed message if |value| doesn't strictly conform to the
  // schema. If |value| doesn't conform to the schema even within the allowance
  // of |strategy|, false will be returned and |out_error_path| and |out_error|
  // will contain the corresponding error that caused the failure.
  // |out_error_path| and |out_error| can be nullptr and in that case no value
  // will be returned.
  bool Validate(const base::Value& value,
                SchemaOnErrorStrategy strategy,
                PolicyErrorPath* out_error_path,
                std::string* out_error) const;

  // Similar to Validate() but drop values with errors instead of ignoring them.
  // |out_changed| is a pointer to a boolean value, and indicate whether |value|
  // is changed or not (probably dropped properties or items). Be sure to set
  // the bool that |out_changed| pointed to false before calling Normalize().
  // |out_error_path|, |out_error| and |out_changed| can be nullptr and in that
  // case no value will be set. This function will also take the ownership of
  // dropped base::Value and destroy them.
  bool Normalize(base::Value* value,
                 SchemaOnErrorStrategy strategy,
                 PolicyErrorPath* out_error_path,
                 std::string* out_error,
                 bool* out_changed) const;

  // Modifies |value| in place - masks values that have been marked as sensitive
  // ("sensitiveValue": true) in this Schema. Note that |value| may not be
  // schema-valid according to this Schema after this function returns - the
  // masking is performed by replacing values with string values, so the value
  // types may not correspond to this Schema anymore.
  void MaskSensitiveValues(base::Value* value) const;

  // Used to iterate over the known properties of Type::DICT schemas.
  class POLICY_EXPORT Iterator {
   public:
    Iterator(const scoped_refptr<const InternalStorage>& storage,
             const internal::PropertiesNode* node);
    Iterator(const Iterator& iterator);
    ~Iterator();

    Iterator& operator=(const Iterator& iterator);

    // The other methods must not be called if the iterator is at the end.
    bool IsAtEnd() const;

    // Advances the iterator to the next property.
    void Advance();

    // Returns the name of the current property.
    const char* key() const;

    // Returns the Schema for the current property. This Schema is always valid.
    Schema schema() const;

   private:
    scoped_refptr<const InternalStorage> storage_;
    raw_ptr<const internal::PropertyNode, AllowPtrArithmetic> it_;
    raw_ptr<const internal::PropertyNode, AllowPtrArithmetic> end_;
  };

  // These methods should be called only if type() == Type::DICT,
  // otherwise invalid memory will be read. A CHECK is currently enforcing this.

  // Returns an iterator that goes over the named properties of this schema.
  // The returned iterator is at the beginning.
  Iterator GetPropertiesIterator() const;

  // Returns the Schema for the property named |key|. If |key| is not a known
  // property name then the returned Schema is not valid.
  Schema GetKnownProperty(const std::string& key) const;

  // Returns all Schemas from pattern properties that match |key|. May be empty.
  SchemaList GetPatternProperties(const std::string& key) const;

  // Returns this Schema's required properties. May be empty if the Schema has
  // no required properties.
  std::vector<std::string> GetRequiredProperties() const;

  // Returns the Schema for additional properties. If additional properties are
  // not allowed for this Schema then the Schema returned is not valid.
  Schema GetAdditionalProperties() const;

  // Returns the Schema for |key| if it is a known property, otherwise returns
  // the Schema for additional properties.
  // DEPRECATED: This function didn't consider patternProperties, use
  // GetMatchingProperties() instead.
  // TODO(binjin): Replace calls to this function with GetKnownProperty() or
  // GetMatchingProperties() and remove this later.
  Schema GetProperty(const std::string& key) const;

  // Returns all Schemas that are supposed to be validated against for |key|.
  // May be empty.
  SchemaList GetMatchingProperties(const std::string& key) const;

  // Returns the Schema for items of an array.
  // This method should be called only if type() == Type::LIST,
  // otherwise invalid memory will be read. A CHECK is currently enforcing this.
  Schema GetItems() const;

  // Gets the validation schema associated with this |schema| - or if there
  // isn't one, returns an empty invalid schema. There are a few policies that
  // contain embedded JSON - these policies have a schema for validating that
  // JSON that is more complicated than the regular schema. For other policies
  // it is not defined. To get the validation schema for a policy, call
  // |chrome_schema.GetValidationSchema().GetKnownProperty(policy_name)|, where
  // |chrome_schema| is the root schema that has all policies as children.
  Schema GetValidationSchema() const;

  // If this returns true, the value described by this schema should not be
  // displayed on the UI.
  bool IsSensitiveValue() const;

  // If this returns true, the schema contains child elements that contain
  // sensitive values.
  bool HasSensitiveChildren() const;

 private:
  // Builds a schema pointing to the inner structure of |storage|,
  // rooted at |node|.
  Schema(const scoped_refptr<const InternalStorage>& storage,
         const internal::SchemaNode* node);

  bool ValidateIntegerRestriction(int index, int value) const;
  bool ValidateStringRestriction(int index, const char* str) const;

  void MaskSensitiveValuesRecursive(base::Value* value) const;

  scoped_refptr<const InternalStorage> storage_;
  raw_ptr<const internal::SchemaNode, DanglingUntriaged | AllowPtrArithmetic>
      node_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_SCHEMA_H_
