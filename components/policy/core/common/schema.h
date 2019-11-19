// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_SCHEMA_H_
#define COMPONENTS_POLICY_CORE_COMMON_SCHEMA_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "components/policy/policy_export.h"

namespace policy {
namespace internal {

struct POLICY_EXPORT SchemaData;
struct POLICY_EXPORT SchemaNode;
struct POLICY_EXPORT PropertyNode;
struct POLICY_EXPORT PropertiesNode;

}  // namespace internal

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
  // don't know newField). Prefer to use |SCHEMA_ALLOW_UNKNOWN| for policies
  // instead.
  SCHEMA_STRICT = 0,
  // Unknown properties in any dictionary will be ignored.
  SCHEMA_ALLOW_UNKNOWN,
  // Mismatched values will be ignored.
  SCHEMA_ALLOW_INVALID,
};

// Schema validation options for Schema::ParseToDictAndValidate().
constexpr int kSchemaOptionsNone = 0;
constexpr int kSchemaOptionsIgnoreUnknownAttributes = 1 << 0;

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

  // Parses the JSON schema in |schema| and returns a Schema that owns
  // the internal representation. If |schema| is invalid then an invalid Schema
  // is returned and |error| contains a reason for the failure.
  static Schema Parse(const std::string& schema, std::string* error);

  // Verifies if |schema| is a valid JSON v3 schema. When this validation passes
  // then |schema| is valid JSON that can be parsed into a Value, and that Value
  // can be used to build a |Schema|. Returns the parsed Value when |schema|
  // validated, otherwise returns nullptr. In that case, |error| contains an
  // error description. For performance reasons, currently IsValidSchema() won't
  // check the correctness of regular expressions used in "pattern" and
  // "patternProperties" and in Validate() invalid regular expression don't
  // accept any strings.
  // |options| is a bitwise-OR combination of the options above (see
  // |kSchemaOptions*| above).
  static std::unique_ptr<base::Value> ParseToDictAndValidate(
      const std::string& schema,
      int options,
      std::string* error);

  // Returns true if this Schema is valid. Schemas returned by the methods below
  // may be invalid, and in those cases the other methods must not be used.
  bool valid() const { return node_ != NULL; }

  base::Value::Type type() const;

  // Validate |value| against current schema, |strategy| is the strategy to
  // handle unknown properties or invalid values. Allowed errors will be
  // ignored. |error_path| and |error| will contain the last error location and
  // detailed message if |value| doesn't strictly conform to the schema. If
  // |value| doesn't conform to the schema even within the allowance of
  // |strategy|, false will be returned and |error_path| and |error| will
  // contain the corresponding error that caused the failure. |error_path| can
  // be NULL and in that case no error path will be returned.
  bool Validate(const base::Value& value,
                SchemaOnErrorStrategy strategy,
                std::string* error_path,
                std::string* error) const;

  // Similar to Validate() but drop values with errors instead of ignoring them.
  // |changed| is a pointer to a boolean value, and indicate whether |value|
  // is changed or not (probably dropped properties or items). Be sure to set
  // the bool that |changed| pointed to to false before calling Normalize().
  // |changed| can be NULL and in that case no boolean will be set.
  // This function will also take the ownership of dropped base::Value and
  // destroy them.
  bool Normalize(base::Value* value,
                 SchemaOnErrorStrategy strategy,
                 std::string* error_path,
                 std::string* error,
                 bool* changed) const;

  // Modifies |value| in place - masks values that have been marked as sensitive
  // ("sensitiveValue": true) in this Schema. Note that |value| may not be
  // schema-valid according to this Schema after this function returns - the
  // masking is performed by replacing values with string values, so the value
  // types may not correspond to this Schema anymore.
  void MaskSensitiveValues(base::Value* value) const;

  // Used to iterate over the known properties of Type::DICTIONARY schemas.
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
    const internal::PropertyNode* it_;
    const internal::PropertyNode* end_;
  };

  // These methods should be called only if type() == Type::DICTIONARY,
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
  const internal::SchemaNode* node_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_SCHEMA_H_
