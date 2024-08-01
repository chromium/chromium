// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/policy/core/common/schema.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

#define TestSchemaValidation(a, b, c, d) \
    TestSchemaValidationHelper(          \
        base::StringPrintf("%s:%i", __FILE__, __LINE__), a, b, c, d)

const char kTestSchema[] = R"({
  "type": "object",
  "properties": {
    "Boolean": { "type": "boolean" },
    "Integer": { "type": "integer" },
    "Number": { "type": "number" },
    "String": { "type": "string" },
    "Array": {
      "type": "array",
      "items": { "type": "string" }
    },
    "ArrayOfObjects": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "one": { "type": "string" },
          "two": { "type": "integer" }
        }
      }
    },
    "ArrayOfArray": {
      "type": "array",
      "items": {
        "type": "array",
        "items": { "type": "string" }
      }
    },
    "Object": {
      "type": "object",
      "properties": {
        "one": { "type": "boolean" },
        "two": { "type": "integer" }
      },
      "additionalProperties": { "type": "string" }
    },
    "ObjectOfObject": {
      "type": "object",
      "properties": {
        "Object": {
          "type": "object",
          "properties": {
            "one": { "type": "string" },
            "two": { "type": "integer" }
          }
        }
      }
    },
    "IntegerWithEnums": {
      "type": "integer",
      "enum": [1, 2, 3]
    },
    "IntegerWithEnumsGaps": {
      "type": "integer",
      "enum": [10, 20, 30]
    },
    "StringWithEnums": {
      "type": "string",
      "enum": ["one", "two", "three"]
    },
    "IntegerWithRange": {
      "type": "integer",
      "minimum": 1,
      "maximum": 3
    },
    "ObjectOfArray": {
      "type": "object",
      "properties": {
        "List": {
          "type": "array",
          "items": { "type": "integer" }
        }
      }
    },
    "ArrayOfObjectOfArray": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "List": {
            "type": "array",
            "items": { "type": "string" }
          }
        }
      }
    },
    "StringWithPattern": {
      "type": "string",
      "pattern": "^foo+$"
    },
    "ObjectWithPatternProperties": {
      "type": "object",
      "patternProperties": {
        "^foo+$": { "type": "integer" },
        "^bar+$": {
          "type": "string",
          "enum": ["one", "two"]
        }
      },
      "properties": {
        "bar": {
          "type": "string",
          "enum": ["one", "three"]
        }
      }
    },
    "ObjectWithRequiredProperties": {
      "type": "object",
      "properties": {
        "Integer": {
          "type": "integer",
          "enum": [1, 2]
        },
        "String": { "type": "string" },
        "Number": { "type": "number" }
      },
      "patternProperties": {
        "^Integer": {
          "type": "integer",
          "enum": [1, 3]
        }
      },
      "required": [ "Integer", "String" ]
    }
  }
})";

bool ParseFails(const std::string& content) {
  ASSIGN_OR_RETURN(const auto schema, Schema::Parse(content),
                   [](const auto& e) { return true; });
  return false;
}

void TestSchemaValidationHelper(const std::string& source,
                                const Schema& schema,
                                const base::Value& value,
                                SchemaOnErrorStrategy strategy,
                                bool expected_return_value) {
  std::string error;
  static const char kNoErrorReturned[] = "No error returned.";

  // Test that Schema::Validate() works as expected.
  error = kNoErrorReturned;
  bool returned = schema.Validate(value, strategy, nullptr, &error);
  ASSERT_EQ(expected_return_value, returned) << source << ": " << error;

  // Test that Schema::Normalize() will return the same value as
  // Schema::Validate().
  error = kNoErrorReturned;
  base::Value cloned_value(value.Clone());
  bool touched = false;
  returned =
      schema.Normalize(&cloned_value, strategy, nullptr, &error, &touched);
  EXPECT_EQ(expected_return_value, returned) << source << ": " << error;

  if (strategy == SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING)
    return;

  bool strictly_valid = schema.Validate(value, SCHEMA_STRICT, nullptr, &error);
  EXPECT_EQ(touched, !strictly_valid && returned) << source;

  // Test that Schema::Normalize() have actually dropped invalid and unknown
  // properties.
  if (expected_return_value) {
    EXPECT_TRUE(schema.Validate(cloned_value, SCHEMA_STRICT, nullptr, &error))
        << source;
    EXPECT_TRUE(schema.Normalize(&cloned_value, SCHEMA_STRICT, nullptr, &error,
                                 nullptr))
        << source;
  }
}

void TestSchemaValidationWithPath(const Schema& schema,
                                  const base::Value& value,
                                  PolicyErrorPath expected_failure_path) {
  PolicyErrorPath error_path;
  std::string error;

  bool returned = schema.Validate(value, SCHEMA_STRICT, &error_path, &error);
  ASSERT_FALSE(returned) << error_path.size();
  EXPECT_EQ(error_path, expected_failure_path);
}

std::string SchemaObjectWrapper(const std::string& subschema) {
  return "{"
         "  \"type\": \"object\","
         "  \"properties\": {"
         "    \"SomePropertyName\":" + subschema +
         "  }"
         "}";
}

}  // namespace

TEST(SchemaTest, MinimalSchema) {
  EXPECT_FALSE(ParseFails(R"({ "type": "object" })"));
}

TEST(SchemaTest, InvalidSchemas) {
  EXPECT_TRUE(ParseFails(""));
  EXPECT_TRUE(ParseFails("omg"));
  EXPECT_TRUE(ParseFails("\"omg\""));
  EXPECT_TRUE(ParseFails("123"));
  EXPECT_TRUE(ParseFails("[]"));
  EXPECT_TRUE(ParseFails("null"));
  EXPECT_TRUE(ParseFails("{}"));

  EXPECT_TRUE(ParseFails(R"({
    "type": "object",
    "additionalProperties": { "type":"object" }
  })"));

  EXPECT_TRUE(ParseFails(R"({
    "type": "object",
    "patternProperties": { "a+b*": { "type": "object" } }
  })"));

  EXPECT_TRUE(ParseFails(R"({
    "type": "object",
    "properties": { "Policy": { "type": "bogus" } }
  })"));

  EXPECT_TRUE(ParseFails(R"({
    "type": "object",
    "properties": { "Policy": { "type": ["string", "number"] } }
  })"));

  EXPECT_TRUE(ParseFails(R"({
    "type": "object",
    "properties": { "Policy": { "type": "any" } }
  })"));

  EXPECT_TRUE(ParseFails(R"({
    "type": "object",
    "properties": { "Policy": 123 }
  })"));

  EXPECT_FALSE(ParseFails(R"({
    "type": "object",
    "unknown attribute": "is ignored"
  })"));
}

TEST(SchemaTest, Ownership) {
  const auto schema = Schema::Parse(R"({
    "type": "object",
    "properties": {
      "sub": {
        "type": "object",
        "properties": {
          "subsub": { "type": "string" }
        }
      }
    }
  })");
  ASSERT_TRUE(schema.has_value()) << schema.error();
  ASSERT_EQ(base::Value::Type::DICT, schema->type());

  Schema sub = schema->GetKnownProperty("sub");
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::Type::DICT, sub.type());

  {
    Schema::Iterator it = sub.GetPropertiesIterator();
    ASSERT_FALSE(it.IsAtEnd());
    EXPECT_STREQ("subsub", it.key());

    sub = it.schema();
    it.Advance();
    EXPECT_TRUE(it.IsAtEnd());
  }

  ASSERT_TRUE(sub.valid());
  EXPECT_EQ(base::Value::Type::STRING, sub.type());

  // This test shouldn't leak nor use invalid memory.
}

TEST(SchemaTest, ValidSchema) {
  const auto schema = Schema::Parse(kTestSchema);
  ASSERT_TRUE(schema.has_value()) << schema.error();

  ASSERT_EQ(base::Value::Type::DICT, schema->type());
  EXPECT_FALSE(schema->GetProperty("invalid").valid());

  Schema sub = schema->GetProperty("Boolean");
  ASSERT_TRUE(sub.valid());
  EXPECT_EQ(base::Value::Type::BOOLEAN, sub.type());

  sub = schema->GetProperty("Integer");
  ASSERT_TRUE(sub.valid());
  EXPECT_EQ(base::Value::Type::INTEGER, sub.type());

  sub = schema->GetProperty("Number");
  ASSERT_TRUE(sub.valid());
  EXPECT_EQ(base::Value::Type::DOUBLE, sub.type());

  sub = schema->GetProperty("String");
  ASSERT_TRUE(sub.valid());
  EXPECT_EQ(base::Value::Type::STRING, sub.type());

  sub = schema->GetProperty("Array");
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::Type::LIST, sub.type());
  sub = sub.GetItems();
  ASSERT_TRUE(sub.valid());
  EXPECT_EQ(base::Value::Type::STRING, sub.type());

  sub = schema->GetProperty("ArrayOfObjects");
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::Type::LIST, sub.type());
  sub = sub.GetItems();
  ASSERT_TRUE(sub.valid());
  EXPECT_EQ(base::Value::Type::DICT, sub.type());
  Schema subsub = sub.GetProperty("one");
  ASSERT_TRUE(subsub.valid());
  EXPECT_EQ(base::Value::Type::STRING, subsub.type());
  subsub = sub.GetProperty("two");
  ASSERT_TRUE(subsub.valid());
  EXPECT_EQ(base::Value::Type::INTEGER, subsub.type());
  subsub = sub.GetProperty("invalid");
  EXPECT_FALSE(subsub.valid());

  sub = schema->GetProperty("ArrayOfArray");
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::Type::LIST, sub.type());
  sub = sub.GetItems();
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::Type::LIST, sub.type());
  sub = sub.GetItems();
  ASSERT_TRUE(sub.valid());
  EXPECT_EQ(base::Value::Type::STRING, sub.type());

  sub = schema->GetProperty("Object");
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::Type::DICT, sub.type());
  subsub = sub.GetProperty("one");
  ASSERT_TRUE(subsub.valid());
  EXPECT_EQ(base::Value::Type::BOOLEAN, subsub.type());
  subsub = sub.GetProperty("two");
  ASSERT_TRUE(subsub.valid());
  EXPECT_EQ(base::Value::Type::INTEGER, subsub.type());
  subsub = sub.GetProperty("undeclared");
  ASSERT_TRUE(subsub.valid());
  EXPECT_EQ(base::Value::Type::STRING, subsub.type());

  sub = schema->GetProperty("IntegerWithEnums");
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::Type::INTEGER, sub.type());

  sub = schema->GetProperty("IntegerWithEnumsGaps");
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::Type::INTEGER, sub.type());

  sub = schema->GetProperty("StringWithEnums");
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::Type::STRING, sub.type());

  sub = schema->GetProperty("IntegerWithRange");
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::Type::INTEGER, sub.type());

  sub = schema->GetProperty("StringWithPattern");
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::Type::STRING, sub.type());

  sub = schema->GetProperty("ObjectWithPatternProperties");
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::Type::DICT, sub.type());

  sub = schema->GetProperty("ObjectWithRequiredProperties");
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::Type::DICT, sub.type());

  struct {
    const char* expected_key;
    base::Value::Type expected_type;
  } kExpectedProperties[] = {
      {"Array", base::Value::Type::LIST},
      {"ArrayOfArray", base::Value::Type::LIST},
      {"ArrayOfObjectOfArray", base::Value::Type::LIST},
      {"ArrayOfObjects", base::Value::Type::LIST},
      {"Boolean", base::Value::Type::BOOLEAN},
      {"Integer", base::Value::Type::INTEGER},
      {"IntegerWithEnums", base::Value::Type::INTEGER},
      {"IntegerWithEnumsGaps", base::Value::Type::INTEGER},
      {"IntegerWithRange", base::Value::Type::INTEGER},
      {"Number", base::Value::Type::DOUBLE},
      {"Object", base::Value::Type::DICT},
      {"ObjectOfArray", base::Value::Type::DICT},
      {"ObjectOfObject", base::Value::Type::DICT},
      {"ObjectWithPatternProperties", base::Value::Type::DICT},
      {"ObjectWithRequiredProperties", base::Value::Type::DICT},
      {"String", base::Value::Type::STRING},
      {"StringWithEnums", base::Value::Type::STRING},
      {"StringWithPattern", base::Value::Type::STRING},
  };
  Schema::Iterator it = schema->GetPropertiesIterator();
  for (const auto& props : kExpectedProperties) {
    ASSERT_FALSE(it.IsAtEnd());
    EXPECT_STREQ(props.expected_key, it.key());
    ASSERT_TRUE(it.schema().valid());
    EXPECT_EQ(props.expected_type, it.schema().type());
    it.Advance();
  }
  EXPECT_TRUE(it.IsAtEnd());
}

TEST(SchemaTest, Lookups) {
  {
    const auto schema = Schema::Parse(R"({ "type": "object" })");
    ASSERT_TRUE(schema.has_value()) << schema.error();
    ASSERT_EQ(base::Value::Type::DICT, schema->type());

    // This empty schema should never find named properties.
    EXPECT_FALSE(schema->GetKnownProperty("").valid());
    EXPECT_FALSE(schema->GetKnownProperty("xyz").valid());
    EXPECT_TRUE(schema->GetRequiredProperties().empty());
    EXPECT_TRUE(schema->GetPatternProperties("").empty());
    EXPECT_FALSE(schema->GetAdditionalProperties().valid());
    EXPECT_TRUE(schema->GetPropertiesIterator().IsAtEnd());
  }

  {
    const auto schema = Schema::Parse(R"({
      "type": "object",
      "properties": {
        "Boolean": { "type": "boolean" }
      }
    })");
    ASSERT_TRUE(schema.has_value()) << schema.error();
    ASSERT_EQ(base::Value::Type::DICT, schema->type());

    EXPECT_FALSE(schema->GetKnownProperty("").valid());
    EXPECT_FALSE(schema->GetKnownProperty("xyz").valid());
    EXPECT_TRUE(schema->GetKnownProperty("Boolean").valid());
  }

  {
    const auto schema = Schema::Parse(R"({
      "type": "object",
      "properties": {
        "aa" : { "type": "boolean" },
        "abab" : { "type": "string" },
        "ab" : { "type": "number" },
        "aba" : { "type": "integer" }
      }
    })");
    ASSERT_TRUE(schema.has_value()) << schema.error();
    ASSERT_EQ(base::Value::Type::DICT, schema->type());

    EXPECT_FALSE(schema->GetKnownProperty("").valid());
    EXPECT_FALSE(schema->GetKnownProperty("xyz").valid());

    struct {
      const char* expected_key;
      base::Value::Type expected_type;
    } kExpectedKeys[] = {
        {"aa", base::Value::Type::BOOLEAN},
        {"ab", base::Value::Type::DOUBLE},
        {"aba", base::Value::Type::INTEGER},
        {"abab", base::Value::Type::STRING},
    };
    for (const auto& key : kExpectedKeys) {
      Schema sub = schema->GetKnownProperty(key.expected_key);
      ASSERT_TRUE(sub.valid());
      EXPECT_EQ(key.expected_type, sub.type());
    }
  }

  {
    const auto schema = Schema::Parse(R"(
    {
      "type": "object",
      "properties": {
        "String": { "type": "string" },
        "Object": {
          "type": "object",
          "properties": {"Integer": {"type": "integer"}},
          "required": [ "Integer" ]
        },
        "Number": { "type": "number" }
      },
      "required": [ "String", "Object"]
    })");
    ASSERT_TRUE(schema.has_value()) << schema.error();
    ASSERT_EQ(base::Value::Type::DICT, schema->type());

    EXPECT_EQ(std::vector<std::string>({"String", "Object"}),
              schema->GetRequiredProperties());

    Schema object = schema->GetKnownProperty("Object");
    ASSERT_TRUE(object.valid());
    ASSERT_EQ(base::Value::Type::DICT, object.type());

    EXPECT_EQ(std::vector<std::string>({"Integer"}),
              object.GetRequiredProperties());
  }
}

TEST(SchemaTest, Wrap) {
  const internal::SchemaNode kSchemas[] = {
      {base::Value::Type::DICT, 0},      //  0: root node
      {base::Value::Type::BOOLEAN, -1},  //  1
      {base::Value::Type::INTEGER, -1},  //  2
      {base::Value::Type::DOUBLE, -1},   //  3
      {base::Value::Type::STRING, -1},   //  4
      {base::Value::Type::LIST, 4},      //  5: list of strings.
      {base::Value::Type::LIST, 5},      //  6: list of lists of strings.
      {base::Value::Type::INTEGER, 0},   //  7: integer enumerations.
      {base::Value::Type::INTEGER, 1},   //  8: ranged integers.
      {base::Value::Type::STRING, 2},    //  9: string enumerations.
      {base::Value::Type::STRING, 3},    // 10: string with pattern.
      {base::Value::Type::DICT, 1},      // 11: dictionary with required
                                         //     properties
  };

  const internal::PropertyNode kPropertyNodes[] = {
    { "Boolean",       1},  //  0
    { "DictRequired", 11},  //  1
    { "Integer",       2},  //  2
    { "List",          5},  //  3
    { "Number",        3},  //  4
    { "String",        4},  //  5
    { "IntEnum",       7},  //  6
    { "RangedInt",     8},  //  7
    { "StrEnum",       9},  //  8
    { "StrPat",       10},  //  9
    { "bar+$",         4},  // 10
    { "String",        4},  // 11
    { "Number",        3},  // 12
  };

  const internal::PropertiesNode kProperties[] = {
    // 0 to 10 (exclusive) are the known properties in kPropertyNodes, 9 is
    // patternProperties and 6 is the additionalProperties node.
    { 0, 10, 11, 0, 0, 6 },
    // 11 to 13 (exclusive) are the known properties in kPropertyNodes. 0 to
    // 1 (exclusive) are the required properties in kRequired. -1 indicates
    // no additionalProperties.
    { 11, 13, 13, 0, 1, -1 },
  };

  const internal::RestrictionNode kRestriction[] = {
    {{0, 3}},  // 0: [1, 2, 3]
    {{5, 1}},  // 1: minimum = 1, maximum = 5
    {{0, 3}},  // 2: ["one", "two", "three"]
    {{3, 3}},  // 3: pattern "foo+"
  };

  const char* kRequired[] = {"String"};

  const int kIntEnums[] = {1, 2, 3};

  const char* kStringEnums[] = {
    "one",    // 0
    "two",    // 1
    "three",  // 2
    "foo+",   // 3
  };

  const internal::SchemaData kData = {
      kSchemas,  kPropertyNodes, kProperties,  kRestriction,
      kRequired, kIntEnums,      kStringEnums,
      -1  // validation_schema_root_index
  };

  Schema schema = Schema::Wrap(&kData);
  ASSERT_TRUE(schema.valid());
  EXPECT_EQ(base::Value::Type::DICT, schema.type());

  // Wrapped schemas have no sensitive values.
  EXPECT_FALSE(schema.IsSensitiveValue());

  struct {
    const char* key;
    base::Value::Type type;
  } kExpectedProperties[] = {
      {"Boolean", base::Value::Type::BOOLEAN},
      {"DictRequired", base::Value::Type::DICT},
      {"Integer", base::Value::Type::INTEGER},
      {"List", base::Value::Type::LIST},
      {"Number", base::Value::Type::DOUBLE},
      {"String", base::Value::Type::STRING},
      {"IntEnum", base::Value::Type::INTEGER},
      {"RangedInt", base::Value::Type::INTEGER},
      {"StrEnum", base::Value::Type::STRING},
      {"StrPat", base::Value::Type::STRING},
  };

  Schema::Iterator it = schema.GetPropertiesIterator();
  for (size_t i = 0; i < std::size(kExpectedProperties); ++i) {
    ASSERT_FALSE(it.IsAtEnd());
    EXPECT_STREQ(kExpectedProperties[i].key, it.key());
    Schema sub = it.schema();
    ASSERT_TRUE(sub.valid());
    EXPECT_EQ(kExpectedProperties[i].type, sub.type());

    if (sub.type() == base::Value::Type::LIST) {
      Schema items = sub.GetItems();
      ASSERT_TRUE(items.valid());
      EXPECT_EQ(base::Value::Type::STRING, items.type());
    }

    it.Advance();
  }
  EXPECT_TRUE(it.IsAtEnd());

  Schema sub = schema.GetAdditionalProperties();
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::Type::LIST, sub.type());
  Schema subsub = sub.GetItems();
  ASSERT_TRUE(subsub.valid());
  ASSERT_EQ(base::Value::Type::LIST, subsub.type());
  Schema subsubsub = subsub.GetItems();
  ASSERT_TRUE(subsubsub.valid());
  ASSERT_EQ(base::Value::Type::STRING, subsubsub.type());

  SchemaList schema_list = schema.GetPatternProperties("barr");
  ASSERT_EQ(1u, schema_list.size());
  sub = schema_list[0];
  ASSERT_TRUE(sub.valid());
  EXPECT_EQ(base::Value::Type::STRING, sub.type());

  EXPECT_TRUE(schema.GetPatternProperties("ba").empty());
  EXPECT_TRUE(schema.GetPatternProperties("bar+$").empty());

  Schema dict = schema.GetKnownProperty("DictRequired");
  ASSERT_TRUE(dict.valid());
  ASSERT_EQ(base::Value::Type::DICT, dict.type());

  EXPECT_EQ(std::vector<std::string>({"String"}), dict.GetRequiredProperties());
}

TEST(SchemaTest, Validate) {
  const auto schema = Schema::Parse(kTestSchema);
  ASSERT_TRUE(schema.has_value()) << schema.error();

  base::Value bundle((base::Value::Dict()));
  base::Value::Dict& dict = bundle.GetDict();
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, true);

  // Wrong type, expected integer.
  dict.Set("Integer", true);
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, false);

  // Wrong type, expected list of strings.
  {
    dict.clear();
    base::Value::List list;
    list.Append(1);
    dict.Set("Array", std::move(list));
    TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, false);
  }

  // Wrong type in a sub-object.
  {
    dict.clear();
    base::Value::Dict subdict;
    subdict.Set("one", "one");
    dict.Set("Object", std::move(subdict));
    TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, false);
  }

  // Unknown name.
  dict.clear();
  dict.Set("Unknown", true);
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, false);

  // All of these will be valid.
  dict.clear();
  dict.Set("Boolean", true);
  dict.Set("Integer", 123);
  dict.Set("Number", 3.14);
  dict.Set("String", "omg");

  {
    base::Value::List list;
    list.Append("a string");
    list.Append("another string");
    dict.Set("Array", std::move(list));
  }

  {
    base::Value::Dict subdict;
    subdict.Set("one", "string");
    subdict.Set("two", 2);
    base::Value::List list;
    list.Append(subdict.Clone());
    list.Append(std::move(subdict));
    dict.Set("ArrayOfObjects", std::move(list));
  }

  {
    base::Value::List list;
    list.Append("a string");
    list.Append("another string");
    base::Value::List listlist;
    listlist.Append(list.Clone());
    listlist.Append(std::move(list));
    dict.Set("ArrayOfArray", std::move(listlist));
  }

  {
    base::Value::Dict subdict;
    subdict.Set("one", true);
    subdict.Set("two", 2);
    subdict.Set("additionally", "a string");
    subdict.Set("and also", "another string");
    dict.Set("Object", std::move(subdict));
  }

  {
    base::Value::Dict subdict;
    subdict.Set("Integer", 1);
    subdict.Set("String", "a string");
    subdict.Set("Number", 3.14);
    dict.Set("ObjectWithRequiredProperties", std::move(subdict));
  }

  dict.Set("IntegerWithEnums", 1);
  dict.Set("IntegerWithEnumsGaps", 20);
  dict.Set("StringWithEnums", "two");
  dict.Set("IntegerWithRange", 3);

  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, true);

  dict.Set("IntegerWithEnums", 0);
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, false);
  dict.Set("IntegerWithEnums", 1);

  dict.Set("IntegerWithEnumsGaps", 0);
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, false);
  dict.Set("IntegerWithEnumsGaps", 9);
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, false);
  dict.Set("IntegerWithEnumsGaps", 10);
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, true);
  dict.Set("IntegerWithEnumsGaps", 11);
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, false);
  dict.Set("IntegerWithEnumsGaps", 19);
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, false);
  dict.Set("IntegerWithEnumsGaps", 21);
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, false);
  dict.Set("IntegerWithEnumsGaps", 29);
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, false);
  dict.Set("IntegerWithEnumsGaps", 30);
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, true);
  dict.Set("IntegerWithEnumsGaps", 31);
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, false);
  dict.Set("IntegerWithEnumsGaps", 100);
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, false);
  dict.Set("IntegerWithEnumsGaps", 20);

  dict.Set("StringWithEnums", "FOUR");
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, false);
  dict.Set("StringWithEnums", "two");

  dict.Set("IntegerWithRange", 4);
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, false);
  dict.Set("IntegerWithRange", 3);

  // Unknown top level property.
  dict.Set("boom", "bang");
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, false);
  TestSchemaValidation(*schema, bundle, SCHEMA_ALLOW_UNKNOWN, true);
  TestSchemaValidation(*schema, bundle,
                       SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY, true);
  TestSchemaValidation(*schema, bundle, SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING,
                       true);
  TestSchemaValidationWithPath(*schema, bundle, {});
  dict.Remove("boom");

  // Invalid top level property.
  dict.Set("Boolean", 12345);
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, false);
  TestSchemaValidation(*schema, bundle, SCHEMA_ALLOW_UNKNOWN, false);
  TestSchemaValidation(*schema, bundle,
                       SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY, false);
  TestSchemaValidation(*schema, bundle, SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING,
                       false);
  TestSchemaValidationWithPath(*schema, bundle, {"Boolean"});
  dict.Set("Boolean", true);

  // Tests on ObjectOfObject.
  {
    Schema subschema = schema->GetProperty("ObjectOfObject");
    ASSERT_TRUE(subschema.valid());
    base::Value root((base::Value::Dict()));
    base::Value::Dict& root_dict = root.GetDict();

    // Unknown property.
    root_dict.SetByDottedPath("Object.three", false);
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, false);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN, true);
    TestSchemaValidation(subschema, root,
                         SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY, true);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING,
                         true);
    TestSchemaValidationWithPath(subschema, root, {"Object"});
    root_dict.RemoveByDottedPath("Object.three");

    // Invalid property.
    root_dict.SetByDottedPath("Object.one", 12345);
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, false);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN, false);
    TestSchemaValidation(subschema, root,
                         SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY, false);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING,
                         false);
    TestSchemaValidationWithPath(subschema, root, {"Object", "one"});
    root_dict.RemoveByDottedPath("Object.one");
  }

  // Tests on ArrayOfObjects.
  {
    Schema subschema = schema->GetProperty("ArrayOfObjects");
    ASSERT_TRUE(subschema.valid());
    base::Value root(base::Value::Type::LIST);
    base::Value::List& root_list = root.GetList();

    // Unknown property.
    base::Value::Dict dict1;
    dict1.Set("three", true);
    root_list.Append(std::move(dict1));
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, false);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN, true);
    TestSchemaValidation(subschema, root,
                         SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY, true);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING,
                         true);
    TestSchemaValidationWithPath(subschema, root, {0});
    root_list.erase(root_list.end() - 1);

    // Invalid property.
    base::Value::Dict dict2;
    dict2.Set("two", true);
    root_list.Append(std::move(dict2));
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, false);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN, false);
    TestSchemaValidation(subschema, root,
                         SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY, true);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING,
                         false);
    TestSchemaValidationWithPath(subschema, root, {0, "two"});
  }

  // Tests on ObjectOfArray.
  {
    Schema subschema = schema->GetProperty("ObjectOfArray");
    ASSERT_TRUE(subschema.valid());
    base::Value root((base::Value::Dict()));

    base::Value::List& list_value =
        root.GetDict().Set("List", base::Value::List())->GetList();

    // Test that there are not errors here.
    list_value.Append(12345);
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, true);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN, true);
    TestSchemaValidation(subschema, root,
                         SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY, true);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING,
                         true);

    // Invalid list item.
    list_value.Append("blabla");
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, false);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN, false);
    TestSchemaValidation(subschema, root,
                         SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY, true);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING,
                         false);
    TestSchemaValidationWithPath(subschema, root, {"List", 1});
  }

  // Tests on ArrayOfObjectOfArray.
  {
    Schema subschema = schema->GetProperty("ArrayOfObjectOfArray");
    ASSERT_TRUE(subschema.valid());
    base::Value root{base::Value::List()};

    base::Value::Dict dict_value;
    base::Value* list_value = dict_value.Set("List", base::Value::List());
    root.GetList().Append(std::move(dict_value));

    // Test that there are not errors here.
    list_value->GetList().Append("blabla");
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, true);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN, true);
    TestSchemaValidation(subschema, root,
                         SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY, true);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING,
                         true);

    // Invalid list item.
    list_value->GetList().Append(12345);
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, false);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN, false);
    TestSchemaValidation(subschema, root,
                         SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY, true);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING,
                         false);
    TestSchemaValidationWithPath(subschema, root, {0, "List", 1});
  }

  // Tests on StringWithPattern.
  {
    Schema subschema = schema->GetProperty("StringWithPattern");
    ASSERT_TRUE(subschema.valid());

    TestSchemaValidation(subschema, base::Value("foobar"), SCHEMA_STRICT,
                         false);
    TestSchemaValidation(subschema, base::Value("foo"), SCHEMA_STRICT, true);
    TestSchemaValidation(subschema, base::Value("fo"), SCHEMA_STRICT, false);
    TestSchemaValidation(subschema, base::Value("fooo"), SCHEMA_STRICT, true);
    TestSchemaValidation(subschema, base::Value("^foo+$"), SCHEMA_STRICT,
                         false);
  }

  // Tests on ObjectWithPatternProperties.
  {
    Schema subschema = schema->GetProperty("ObjectWithPatternProperties");
    ASSERT_TRUE(subschema.valid());
    base::Value root((base::Value::Dict()));
    base::Value::Dict& root_dict = root.GetDict();

    ASSERT_EQ(1u, subschema.GetPatternProperties("fooo").size());
    ASSERT_EQ(1u, subschema.GetPatternProperties("foo").size());
    ASSERT_EQ(1u, subschema.GetPatternProperties("barr").size());
    ASSERT_EQ(1u, subschema.GetPatternProperties("bar").size());
    ASSERT_EQ(1u, subschema.GetMatchingProperties("fooo").size());
    ASSERT_EQ(1u, subschema.GetMatchingProperties("foo").size());
    ASSERT_EQ(1u, subschema.GetMatchingProperties("barr").size());
    ASSERT_EQ(2u, subschema.GetMatchingProperties("bar").size());
    ASSERT_TRUE(subschema.GetPatternProperties("foobar").empty());

    root_dict.Set("fooo", 123);
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, true);
    root_dict.Set("fooo", false);
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, false);
    root_dict.Remove("fooo");

    root_dict.Set("foo", 123);
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, true);
    root_dict.Set("foo", false);
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, false);
    root_dict.Remove("foo");

    root_dict.Set("barr", "one");
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, true);
    root_dict.Set("barr", "three");
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, false);
    root_dict.Set("barr", false);
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, false);
    root_dict.Remove("barr");

    root_dict.Set("bar", "one");
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, true);
    root_dict.Set("bar", "two");
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, false);
    root_dict.Set("bar", "three");
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, false);
    root_dict.Remove("bar");

    root_dict.Set("foobar", 123);
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, false);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN, true);
    TestSchemaValidation(subschema, root,
                         SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY, true);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING,
                         true);
    root_dict.Remove("foobar");
  }

  // Tests on ObjectWithRequiredProperties
  {
    Schema subschema = schema->GetProperty("ObjectWithRequiredProperties");
    ASSERT_TRUE(subschema.valid());
    base::Value root((base::Value::Dict()));
    base::Value::Dict& root_dict = root.GetDict();

    // Required property missing.
    root_dict.Set("Integer", 1);
    root_dict.Set("Number", 3.14);
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, false);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN, false);
    TestSchemaValidation(subschema, root,
                         SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY, false);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING,
                         false);

    // Invalid required property.
    root_dict.Set("String", 123);
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, false);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN, false);
    TestSchemaValidation(subschema, root,
                         SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY, false);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING,
                         false);
    root_dict.Set("String", "a string");

    // Invalid subschema of required property with multiple subschemas.
    //
    // The "Integer" property has two subschemas, one in "properties" and one
    // in "patternProperties". The first test generates a valid schema for the
    // first subschema and the second test generates a valid schema for the
    // second subschema. In both cases validation should fail because one of the
    // required properties is invalid.
    root_dict.Set("Integer", 2);
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, false);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN, false);
    TestSchemaValidation(subschema, root,
                         SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY, false);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING,
                         false);

    root_dict.Set("Integer", 3);
    TestSchemaValidation(subschema, root, SCHEMA_STRICT, false);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN, false);
    TestSchemaValidation(subschema, root,
                         SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY, false);
    TestSchemaValidation(subschema, root, SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING,
                         false);
  }

  // Test that integer to double promotion is allowed.
  dict.Set("Number", 31415);
  TestSchemaValidation(*schema, bundle, SCHEMA_STRICT, true);
}

TEST(SchemaTest, InvalidReferences) {
  // References to undeclared schemas fail.
  EXPECT_TRUE(ParseFails(R"({
    "type": "object",
    "properties": {
      "name": { "$ref": "undeclared" }
    }
  })"));

  // Can't refer to self.
  EXPECT_TRUE(ParseFails(R"({
    "type": "object",
    "properties": {
      "name": {
        "id": "self",
        "$ref": "self"
      }
    }
  })"));

  // Duplicated IDs are invalid.
  EXPECT_TRUE(ParseFails(R"({
    "type": "object",
    "properties": {
      "name": {
        "id": "x",
        "type": "string"
      },
      "another": {
        "id": "x",
        "type": "string"
      }
    }
  })"));

  // Main object can't be a reference.
  EXPECT_TRUE(ParseFails(R"({
    "type": "object",
    "id": "main",
    "$ref": "main"
  })"));

  EXPECT_TRUE(ParseFails(R"({
    "type": "object",
    "$ref": "main"
  })"));
}

TEST(SchemaTest, RecursiveReferences) {
  // Verifies that references can go to a parent schema, to define a
  // recursive type.
  const auto schema = Schema::Parse(R"({
    "type": "object",
    "properties": {
      "bookmarks": {
        "type": "array",
        "id": "ListOfBookmarks",
        "items": {
          "type": "object",
          "properties": {
            "name": { "type": "string" },
            "url": { "type": "string" },
            "children": { "$ref": "ListOfBookmarks" }
          }
        }
      }
    }
  })");
  ASSERT_TRUE(schema.has_value()) << schema.error();
  ASSERT_EQ(base::Value::Type::DICT, schema->type());

  Schema parent = schema->GetKnownProperty("bookmarks");
  ASSERT_TRUE(parent.valid());
  ASSERT_EQ(base::Value::Type::LIST, parent.type());

  // Check the recursive type a number of times.
  for (int i = 0; i < 10; ++i) {
    Schema items = parent.GetItems();
    ASSERT_TRUE(items.valid());
    ASSERT_EQ(base::Value::Type::DICT, items.type());

    Schema prop = items.GetKnownProperty("name");
    ASSERT_TRUE(prop.valid());
    ASSERT_EQ(base::Value::Type::STRING, prop.type());

    prop = items.GetKnownProperty("url");
    ASSERT_TRUE(prop.valid());
    ASSERT_EQ(base::Value::Type::STRING, prop.type());

    prop = items.GetKnownProperty("children");
    ASSERT_TRUE(prop.valid());
    ASSERT_EQ(base::Value::Type::LIST, prop.type());

    parent = prop;
  }
}

TEST(SchemaTest, UnorderedReferences) {
  // Verifies that references and IDs can come in any order.
  const auto schema = Schema::Parse(R"({
    "type": "object",
    "properties": {
      "a": { "$ref": "shared" },
      "b": { "$ref": "shared" },
      "c": { "$ref": "shared" },
      "d": { "$ref": "shared" },
      "e": {
        "type": "boolean",
        "id": "shared"
      },
      "f": { "$ref": "shared" },
      "g": { "$ref": "shared" },
      "h": { "$ref": "shared" },
      "i": { "$ref": "shared" }
    }
  })");
  ASSERT_TRUE(schema.has_value()) << schema.error();
  ASSERT_EQ(base::Value::Type::DICT, schema->type());

  for (char c = 'a'; c <= 'i'; ++c) {
    Schema sub = schema->GetKnownProperty(std::string(1, c));
    ASSERT_TRUE(sub.valid()) << c;
    ASSERT_EQ(base::Value::Type::BOOLEAN, sub.type()) << c;
  }
}

TEST(SchemaTest, AdditionalPropertiesReference) {
  // Verifies that "additionalProperties" can be a reference.
  const auto schema = Schema::Parse(R"({
    "type": "object",
    "properties": {
      "policy": {
        "type": "object",
        "properties": {
          "foo": {
            "type": "boolean",
            "id": "FooId"
          }
        },
        "additionalProperties": { "$ref": "FooId" }
      }
    }
  })");
  ASSERT_TRUE(schema.has_value()) << schema.error();
  ASSERT_EQ(base::Value::Type::DICT, schema->type());

  Schema policy = schema->GetKnownProperty("policy");
  ASSERT_TRUE(policy.valid());
  ASSERT_EQ(base::Value::Type::DICT, policy.type());

  Schema foo = policy.GetKnownProperty("foo");
  ASSERT_TRUE(foo.valid());
  EXPECT_EQ(base::Value::Type::BOOLEAN, foo.type());

  Schema additional = policy.GetAdditionalProperties();
  ASSERT_TRUE(additional.valid());
  EXPECT_EQ(base::Value::Type::BOOLEAN, additional.type());

  Schema x = policy.GetProperty("x");
  ASSERT_TRUE(x.valid());
  EXPECT_EQ(base::Value::Type::BOOLEAN, x.type());
}

TEST(SchemaTest, ItemsReference) {
  // Verifies that "items" can be a reference.
  const auto schema = Schema::Parse(R"({
    "type": "object",
    "properties": {
      "foo": {
        "type": "boolean",
        "id": "FooId"
      },
      "list": {
        "type": "array",
        "items": { "$ref": "FooId" }
      }
    }
  })");
  ASSERT_TRUE(schema.has_value()) << schema.error();
  ASSERT_EQ(base::Value::Type::DICT, schema->type());

  Schema foo = schema->GetKnownProperty("foo");
  ASSERT_TRUE(foo.valid());
  EXPECT_EQ(base::Value::Type::BOOLEAN, foo.type());

  Schema list = schema->GetKnownProperty("list");
  ASSERT_TRUE(list.valid());
  ASSERT_EQ(base::Value::Type::LIST, list.type());

  Schema items = list.GetItems();
  ASSERT_TRUE(items.valid());
  ASSERT_EQ(base::Value::Type::BOOLEAN, items.type());
}

TEST(SchemaTest, SchemaNodeSensitiveValues) {
  const std::string kNormalBooleanSchema = "normal_boolean";
  const std::string kSensitiveBooleanSchema = "sensitive_boolean";
  const std::string kSensitiveStringSchema = "sensitive_string";
  const std::string kSensitiveObjectSchema = "sensitive_object";
  const std::string kSensitiveArraySchema = "sensitive_array";
  const std::string kSensitiveIntegerSchema = "sensitive_integer";
  const std::string kSensitiveNumberSchema = "sensitive_number";
  const auto schema = Schema::Parse(R"({
    "type": "object",
    "properties": {
      "normal_boolean": {
        "type": "boolean"
      },
      "sensitive_boolean": {
        "type": "boolean",
        "sensitiveValue": true
      },
      "sensitive_string": {
        "type": "string",
        "sensitiveValue": true
      },
      "sensitive_object": {
        "type": "object",
        "additionalProperties": {
          "type": "boolean"
        },
        "sensitiveValue": true
      },
      "sensitive_array": {
        "type": "array",
        "items": {
          "type": "boolean"
        },
        "sensitiveValue": true
      },
      "sensitive_integer": {
        "type": "integer",
        "sensitiveValue": true
      },
      "sensitive_number": {
        "type": "number",
        "sensitiveValue": true
      }
    }
  })");
  ASSERT_TRUE(schema.has_value()) << schema.error();
  ASSERT_EQ(base::Value::Type::DICT, schema->type());
  EXPECT_FALSE(schema->IsSensitiveValue());
  EXPECT_TRUE(schema->HasSensitiveChildren());

  Schema normal_boolean = schema->GetKnownProperty(kNormalBooleanSchema);
  ASSERT_TRUE(normal_boolean.valid());
  EXPECT_EQ(base::Value::Type::BOOLEAN, normal_boolean.type());
  EXPECT_FALSE(normal_boolean.IsSensitiveValue());
  EXPECT_FALSE(normal_boolean.HasSensitiveChildren());

  Schema sensitive_boolean = schema->GetKnownProperty(kSensitiveBooleanSchema);
  ASSERT_TRUE(sensitive_boolean.valid());
  EXPECT_EQ(base::Value::Type::BOOLEAN, sensitive_boolean.type());
  EXPECT_TRUE(sensitive_boolean.IsSensitiveValue());
  EXPECT_FALSE(sensitive_boolean.HasSensitiveChildren());

  Schema sensitive_string = schema->GetKnownProperty(kSensitiveStringSchema);
  ASSERT_TRUE(sensitive_string.valid());
  EXPECT_EQ(base::Value::Type::STRING, sensitive_string.type());
  EXPECT_TRUE(sensitive_string.IsSensitiveValue());
  EXPECT_FALSE(sensitive_string.HasSensitiveChildren());

  Schema sensitive_object = schema->GetKnownProperty(kSensitiveObjectSchema);
  ASSERT_TRUE(sensitive_object.valid());
  EXPECT_EQ(base::Value::Type::DICT, sensitive_object.type());
  EXPECT_TRUE(sensitive_object.IsSensitiveValue());
  EXPECT_FALSE(sensitive_object.HasSensitiveChildren());

  Schema sensitive_array = schema->GetKnownProperty(kSensitiveArraySchema);
  ASSERT_TRUE(sensitive_array.valid());
  EXPECT_EQ(base::Value::Type::LIST, sensitive_array.type());
  EXPECT_TRUE(sensitive_array.IsSensitiveValue());
  EXPECT_FALSE(sensitive_array.HasSensitiveChildren());

  Schema sensitive_integer = schema->GetKnownProperty(kSensitiveIntegerSchema);
  ASSERT_TRUE(sensitive_integer.valid());
  EXPECT_EQ(base::Value::Type::INTEGER, sensitive_integer.type());
  EXPECT_TRUE(sensitive_integer.IsSensitiveValue());
  EXPECT_FALSE(sensitive_integer.HasSensitiveChildren());

  Schema sensitive_number = schema->GetKnownProperty(kSensitiveNumberSchema);
  ASSERT_TRUE(sensitive_number.valid());
  EXPECT_EQ(base::Value::Type::DOUBLE, sensitive_number.type());
  EXPECT_TRUE(sensitive_number.IsSensitiveValue());
  EXPECT_FALSE(sensitive_number.HasSensitiveChildren());

  // Run |MaskSensitiveValues| on the top-level schema
  base::Value::Dict object;
  object.Set("objectProperty", true);
  base::Value::List array;
  array.Append(true);

  base::Value value((base::Value::Dict()));
  base::Value::Dict& value_dict = value.GetDict();
  value_dict.Set(kNormalBooleanSchema, true);
  value_dict.Set(kSensitiveBooleanSchema, true);
  value_dict.Set(kSensitiveStringSchema, "testvalue");
  value_dict.Set(kSensitiveObjectSchema, std::move(object));
  value_dict.Set(kSensitiveArraySchema, std::move(array));
  value_dict.Set(kSensitiveIntegerSchema, 42);
  value_dict.Set(kSensitiveNumberSchema, 3.141);
  schema->MaskSensitiveValues(&value);

  base::Value value_masked("********");
  base::Value::Dict value_expected;
  value_expected.Set(kNormalBooleanSchema, true);
  value_expected.Set(kSensitiveBooleanSchema, value_masked.Clone());
  value_expected.Set(kSensitiveStringSchema, value_masked.Clone());
  value_expected.Set(kSensitiveObjectSchema, value_masked.Clone());
  value_expected.Set(kSensitiveArraySchema, value_masked.Clone());
  value_expected.Set(kSensitiveIntegerSchema, value_masked.Clone());
  value_expected.Set(kSensitiveNumberSchema, value_masked.Clone());
  EXPECT_EQ(value_expected, value_dict);

  // Run |MaskSensitiveValues| on a sub-schema
  base::Value string_value("testvalue");
  sensitive_string.MaskSensitiveValues(&string_value);
  EXPECT_EQ(value_masked.Clone(), string_value);
}

TEST(SchemaTest, SchemaNodeNoSensitiveValues) {
  const auto schema = Schema::Parse(R"({
    "type": "object",
    "properties": {
      "foo": {
        "type": "boolean"
      }
    }
  })");
  ASSERT_TRUE(schema.has_value()) << schema.error();
  ASSERT_EQ(base::Value::Type::DICT, schema->type());
  EXPECT_FALSE(schema->IsSensitiveValue());

  Schema foo = schema->GetKnownProperty("foo");
  ASSERT_TRUE(foo.valid());
  EXPECT_EQ(base::Value::Type::BOOLEAN, foo.type());
  EXPECT_FALSE(foo.IsSensitiveValue());

  base::Value value(base::Value::Type::DICT);
  value.GetDict().Set("foo", true);

  base::Value expected_value = value.Clone();
  schema->MaskSensitiveValues(&value);
  EXPECT_EQ(expected_value, value);
}

TEST(SchemaTest, EnumerationRestriction) {
  // Enum attribute is a list.
  EXPECT_TRUE(ParseFails(SchemaObjectWrapper(R"({
    "type": "string",
    "enum": 12
  })")));

  // Empty enum attributes is not allowed.
  EXPECT_TRUE(ParseFails(SchemaObjectWrapper(R"({
    "type": "integer",
    "enum": []
  })")));

  // Enum elements type should be same as stated.
  EXPECT_TRUE(ParseFails(SchemaObjectWrapper(R"({
    "type": "string",
    "enum": [1, 2, 3]
  })")));

  EXPECT_FALSE(ParseFails(SchemaObjectWrapper(R"({
    "type": "integer",
    "enum": [1, 2, 3]
  })")));

  EXPECT_FALSE(ParseFails(SchemaObjectWrapper(R"({
    "type": "string",
    "enum": ["1", "2", "3"]
  })")));
}

TEST(SchemaTest, RangedRestriction) {
  EXPECT_TRUE(ParseFails(SchemaObjectWrapper(R"({
    "type": "integer",
    "minimum": 10,
    "maximum": 5
  })")));

  EXPECT_FALSE(ParseFails(SchemaObjectWrapper(R"({
    "type": "integer",
    "minimum": 10,
    "maximum": 20
  })")));
}

TEST(SchemaTest, ParseToDictAndValidate) {
  EXPECT_FALSE(
      Schema::ParseToDictAndValidate("", kSchemaOptionsNone).has_value());
  EXPECT_FALSE(
      Schema::ParseToDictAndValidate("\0", kSchemaOptionsNone).has_value());
  EXPECT_FALSE(
      Schema::ParseToDictAndValidate("string", kSchemaOptionsNone).has_value());
  EXPECT_FALSE(Schema::ParseToDictAndValidate(R"("string")", kSchemaOptionsNone)
                   .has_value());
  EXPECT_FALSE(
      Schema::ParseToDictAndValidate("[]", kSchemaOptionsNone).has_value());
  EXPECT_FALSE(
      Schema::ParseToDictAndValidate("{}", kSchemaOptionsNone).has_value());
  EXPECT_FALSE(
      Schema::ParseToDictAndValidate(R"({ "type": 123 })", kSchemaOptionsNone)
          .has_value());
  EXPECT_FALSE(Schema::ParseToDictAndValidate(R"({ "type": "invalid" })",
                                              kSchemaOptionsNone)
                   .has_value());
  EXPECT_FALSE(Schema::ParseToDictAndValidate(
                   R"({
        "type": "object",
        "properties": []
      })",  // Invalid properties type.
                   kSchemaOptionsNone)
                   .has_value());
  EXPECT_FALSE(Schema::ParseToDictAndValidate(
                   R"({
        "type": "string",
        "enum": [ {} ]
      })",  // "enum" dict values must contain "name".
                   kSchemaOptionsNone)
                   .has_value());
  EXPECT_FALSE(Schema::ParseToDictAndValidate(
                   R"({
        "type": "string",
        "enum": [ { "name": {} } ]
      })",  // "enum" name must be a simple value.
                   kSchemaOptionsNone)
                   .has_value());
  EXPECT_FALSE(Schema::ParseToDictAndValidate(
                   R"({
        "type": "array",
        "items": [ 123 ],
      })",  // "items" must contain a schema or schemas.
                   kSchemaOptionsNone)
                   .has_value());
  EXPECT_TRUE(Schema::ParseToDictAndValidate(R"({ "type": "object" })",
                                             kSchemaOptionsNone)
                  .has_value());
  EXPECT_FALSE(Schema::ParseToDictAndValidate(
                   R"({ "type": ["object", "array"] })", kSchemaOptionsNone)
                   .has_value());
  EXPECT_FALSE(Schema::ParseToDictAndValidate(
                   R"({
        "type": "array",
        "items": [
          { "type": "string" },
          { "type": "integer" }
        ]
      })",
                   kSchemaOptionsNone)
                   .has_value());
  EXPECT_TRUE(Schema::ParseToDictAndValidate(
                  R"({
        "type": "object",
          "properties": {
            "string-property": {
              "type": "string",
              "title": "The String Policy",
              "description": "This policy controls the String widget."
            },
            "integer-property": {
              "type": "number"
            },
            "enum-property": {
              "type": "integer",
              "enum": [0, 1, 10, 100]
            },
            "items-property": {
              "type": "array",
              "items": {
                "type": "string"
              }
            }
        },
        "additionalProperties": {
          "type": "boolean"
        }
      })",
                  kSchemaOptionsNone)
                  .has_value());
  EXPECT_TRUE(Schema::ParseToDictAndValidate(
                  R"#({
        "type": "object",
        "patternProperties": {
          ".": { "type": "boolean" },
          "foo": { "type": "boolean" },
          "^foo$": { "type": "boolean" },
          "foo+": { "type": "boolean" },
          "foo?": { "type": "boolean" },
          "fo{2,4}": { "type": "boolean" },
          "(left)|(right)": { "type": "boolean" }
        }
      })#",
                  kSchemaOptionsNone)
                  .has_value());
  EXPECT_TRUE(Schema::ParseToDictAndValidate(
                  R"({
        "type": "object",
        "unknown attribute": "that should just be ignored"
      })",
                  kSchemaOptionsIgnoreUnknownAttributes)
                  .has_value());
  EXPECT_FALSE(Schema::ParseToDictAndValidate(
                   R"({
        "type": "object",
        "unknown attribute": "that will cause a failure"
      })",
                   kSchemaOptionsNone)
                   .has_value());
  EXPECT_FALSE(Schema::ParseToDictAndValidate(
                   R"({
        "type": "object",
        "properties": {"foo": {"type": "number"}},
        "required": 123
      })",
                   kSchemaOptionsNone)
                   .has_value());
  EXPECT_FALSE(Schema::ParseToDictAndValidate(
                   R"({
        "type": "object",
        "properties": {"foo": {"type": "number"}},
        "required": [ 123 ]
      })",
                   kSchemaOptionsNone)
                   .has_value());
  EXPECT_FALSE(Schema::ParseToDictAndValidate(
                   R"({
        "type": "object",
        "properties": {"foo": {"type": "number"}},
        "required": ["bar"]
      })",
                   kSchemaOptionsNone)
                   .has_value());
  EXPECT_FALSE(Schema::ParseToDictAndValidate(
                   R"({
        "type": "object",
        "required": ["bar"]
      })",
                   kSchemaOptionsNone)
                   .has_value());
  EXPECT_TRUE(Schema::ParseToDictAndValidate(
                  R"({
        "type": "object",
        "properties": {"foo": {"type": "number"}},
        "required": ["foo"]
      })",
                  kSchemaOptionsNone)
                  .has_value());
}

TEST(SchemaTest, ErrorPathToString) {
  EXPECT_EQ(ErrorPathToString("Policy", {}), "");
  EXPECT_EQ(ErrorPathToString("Policy", {"key"}), "Policy.key");
  EXPECT_EQ(ErrorPathToString("Policy", {0}), "Policy[0]");
  EXPECT_EQ(ErrorPathToString("", {"key"}), ".key");
  EXPECT_EQ(ErrorPathToString("", {0}), "[0]");
  EXPECT_EQ(ErrorPathToString("Policy", {"key", "key", "key"}),
            "Policy.key.key.key");
  EXPECT_EQ(ErrorPathToString("Policy", {"a", "b", "c"}), "Policy.a.b.c");
  EXPECT_EQ(ErrorPathToString("Policy", {0, 0, 0}), "Policy[0][0][0]");
  EXPECT_EQ(ErrorPathToString("Policy", {1, 2, 3}), "Policy[1][2][3]");
  EXPECT_EQ(ErrorPathToString("Policy", {0, "key", 5, 7, "example"}),
            "Policy[0].key[5][7].example");
}

}  // namespace policy
