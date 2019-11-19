// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/schema_map.h"
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/external_data_manager.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char kTestSchema[] =
    R"({
      "type": "object",
      "properties": {
        "string": { "type": "string" },
        "integer": { "type": "integer" },
        "boolean": { "type": "boolean" },
        "double": { "type": "number" },
        "list": {
          "type": "array",
          "items": { "type": "string" }
        },
        "object": {
          "type": "object",
          "properties": {
            "a": { "type": "string" },
            "b": { "type": "integer" }
          }
        }
      }
    })";

}  // namespace

class SchemaMapTest : public testing::Test {
 protected:
  Schema CreateTestSchema() {
    std::string error;
    Schema schema = Schema::Parse(kTestSchema, &error);
    if (!schema.valid())
      ADD_FAILURE() << error;
    return schema;
  }

  scoped_refptr<SchemaMap> CreateTestMap() {
    Schema schema = CreateTestSchema();
    ComponentMap component_map;
    component_map["extension-1"] = schema;
    component_map["extension-2"] = schema;
    component_map["legacy-extension"] = Schema();

    DomainMap domain_map;
    domain_map[POLICY_DOMAIN_EXTENSIONS] = component_map;

    return new SchemaMap(domain_map);
  }
};

TEST_F(SchemaMapTest, Empty) {
  scoped_refptr<SchemaMap> map = new SchemaMap();
  EXPECT_TRUE(map->GetDomains().empty());
  EXPECT_FALSE(map->GetComponents(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(map->GetComponents(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_FALSE(map->GetSchema(PolicyNamespace(POLICY_DOMAIN_CHROME, "")));
  EXPECT_FALSE(map->HasComponents());
}

TEST_F(SchemaMapTest, HasComponents) {
  scoped_refptr<SchemaMap> map = new SchemaMap();
  EXPECT_FALSE(map->HasComponents());

  // The Chrome schema does not count as a component.
  Schema schema = CreateTestSchema();
  ComponentMap component_map;
  component_map[""] = schema;
  DomainMap domain_map;
  domain_map[POLICY_DOMAIN_CHROME] = component_map;
  map = new SchemaMap(domain_map);
  EXPECT_FALSE(map->HasComponents());

  // An extension schema does.
  domain_map[POLICY_DOMAIN_EXTENSIONS] = component_map;
  map = new SchemaMap(domain_map);
  EXPECT_TRUE(map->HasComponents());
}

TEST_F(SchemaMapTest, Lookups) {
  scoped_refptr<SchemaMap> map = CreateTestMap();
  ASSERT_TRUE(map.get());
  EXPECT_TRUE(map->HasComponents());

  EXPECT_FALSE(map->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_CHROME, "")));
  EXPECT_FALSE(map->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_CHROME, "extension-1")));
  EXPECT_FALSE(map->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_CHROME, "legacy-extension")));
  EXPECT_FALSE(map->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "")));
  EXPECT_FALSE(map->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "extension-3")));

  const Schema* schema =
      map->GetSchema(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "extension-1"));
  ASSERT_TRUE(schema);
  EXPECT_TRUE(schema->valid());

  schema = map->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "legacy-extension"));
  ASSERT_TRUE(schema);
  EXPECT_FALSE(schema->valid());
}

TEST_F(SchemaMapTest, FilterBundle) {
  std::string error;
  Schema schema = Schema::Parse(kTestSchema, &error);
  ASSERT_TRUE(schema.valid()) << error;

  DomainMap domain_map;
  domain_map[POLICY_DOMAIN_EXTENSIONS]["abc"] = schema;
  scoped_refptr<SchemaMap> schema_map = new SchemaMap(domain_map);

  PolicyBundle bundle;
  schema_map->FilterBundle(&bundle);
  const PolicyBundle empty_bundle;
  EXPECT_TRUE(bundle.Equals(empty_bundle));

  // The Chrome namespace isn't filtered.
  PolicyBundle expected_bundle;
  PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, "");
  expected_bundle.Get(chrome_ns).Set(
      "ChromePolicy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("value"), nullptr);
  bundle.CopyFrom(expected_bundle);

  // Unknown components are filtered out.
  PolicyNamespace another_extension_ns(POLICY_DOMAIN_EXTENSIONS, "xyz");
  bundle.Get(another_extension_ns)
      .Set("AnotherExtensionPolicy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("value"),
           nullptr);
  schema_map->FilterBundle(&bundle);
  EXPECT_TRUE(bundle.Equals(expected_bundle));

  PolicyNamespace extension_ns(POLICY_DOMAIN_EXTENSIONS, "abc");
  PolicyMap& map = expected_bundle.Get(extension_ns);
  base::ListValue list;
  list.AppendString("a");
  list.AppendString("b");
  map.Set("list", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          POLICY_SOURCE_CLOUD, list.CreateDeepCopy(), nullptr);
  map.Set("boolean", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true), nullptr);
  map.Set("integer", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(1), nullptr);
  map.Set("double", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(1.2), nullptr);
  base::DictionaryValue dict;
  dict.SetString("a", "b");
  dict.SetInteger("b", 2);
  map.Set("object", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          POLICY_SOURCE_CLOUD, dict.CreateDeepCopy(), nullptr);
  map.Set("string", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("value"), nullptr);

  bundle.MergeFrom(expected_bundle);
  bundle.Get(extension_ns)
      .Set("Unexpected", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("to-be-removed"),
           nullptr);

  schema_map->FilterBundle(&bundle);
  // Merged twice so this causes a conflict.
  expected_bundle.Get(chrome_ns)
      .GetMutable("ChromePolicy")
      ->AddConflictingPolicy(
          expected_bundle.Get(chrome_ns).Get("ChromePolicy")->DeepCopy());
  expected_bundle.Get(chrome_ns)
      .GetMutable("ChromePolicy")
      ->AddWarning(IDS_POLICY_CONFLICT_SAME_VALUE);
  EXPECT_TRUE(bundle.Equals(expected_bundle));

  // Mismatched types are also removed.
  bundle.Clear();
  PolicyMap& badmap = bundle.Get(extension_ns);
  badmap.Set("list", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(false),
             nullptr);
  badmap.Set("boolean", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(0), nullptr);
  badmap.Set("integer", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(false),
             nullptr);
  badmap.Set("null", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(false),
             nullptr);
  badmap.Set("double", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(false),
             nullptr);
  badmap.Set("object", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(false),
             nullptr);
  badmap.Set("string", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, nullptr,
             std::make_unique<ExternalDataFetcher>(nullptr, std::string()));

  schema_map->FilterBundle(&bundle);
  EXPECT_TRUE(bundle.Equals(empty_bundle));
}

TEST_F(SchemaMapTest, LegacyComponents) {
  std::string error;
  Schema schema = Schema::Parse(
      R"({
        "type": "object",
        "properties": {
          "String": { "type": "string" }
        }
      })",
      &error);
  ASSERT_TRUE(schema.valid()) << error;

  DomainMap domain_map;
  domain_map[POLICY_DOMAIN_EXTENSIONS]["with-schema"] = schema;
  domain_map[POLICY_DOMAIN_EXTENSIONS]["without-schema"] = Schema();
  scoped_refptr<SchemaMap> schema_map = new SchemaMap(domain_map);

  // |bundle| contains policies loaded by a policy provider.
  PolicyBundle bundle;

  // Known components with schemas are filtered.
  PolicyNamespace extension_ns(POLICY_DOMAIN_EXTENSIONS, "with-schema");
  bundle.Get(extension_ns)
      .Set("String", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("value 1"),
           nullptr);

  // The Chrome namespace isn't filtered.
  PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, "");
  bundle.Get(chrome_ns).Set("ChromePolicy", POLICY_LEVEL_MANDATORY,
                            POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                            std::make_unique<base::Value>("value 3"), nullptr);

  PolicyBundle expected_bundle;
  expected_bundle.MergeFrom(bundle);

  // Known components without a schema are filtered out completely.
  PolicyNamespace without_schema_ns(POLICY_DOMAIN_EXTENSIONS, "without-schema");
  bundle.Get(without_schema_ns)
      .Set("Schemaless", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("value 2"),
           nullptr);

  // Unknown policies of known components with a schema are removed.
  bundle.Get(extension_ns)
      .Set("Surprise", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("value 4"),
           nullptr);

  // Unknown components are removed.
  PolicyNamespace unknown_ns(POLICY_DOMAIN_EXTENSIONS, "unknown");
  bundle.Get(unknown_ns)
      .Set("Surprise", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("value 5"),
           nullptr);

  schema_map->FilterBundle(&bundle);
  EXPECT_TRUE(bundle.Equals(expected_bundle));
}

TEST_F(SchemaMapTest, GetChanges) {
  DomainMap map;
  map[POLICY_DOMAIN_CHROME][""] = Schema();
  scoped_refptr<SchemaMap> older = new SchemaMap(map);
  map[POLICY_DOMAIN_CHROME][""] = Schema();
  scoped_refptr<SchemaMap> newer = new SchemaMap(map);

  PolicyNamespaceList removed;
  PolicyNamespaceList added;
  newer->GetChanges(older, &removed, &added);
  EXPECT_TRUE(removed.empty());
  EXPECT_TRUE(added.empty());

  map[POLICY_DOMAIN_CHROME][""] = Schema();
  map[POLICY_DOMAIN_EXTENSIONS]["xyz"] = Schema();
  newer = new SchemaMap(map);
  newer->GetChanges(older, &removed, &added);
  EXPECT_TRUE(removed.empty());
  ASSERT_EQ(1u, added.size());
  EXPECT_EQ(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "xyz"), added[0]);

  older = newer;
  map[POLICY_DOMAIN_EXTENSIONS]["abc"] = Schema();
  newer = new SchemaMap(map);
  newer->GetChanges(older, &removed, &added);
  ASSERT_EQ(2u, removed.size());
  EXPECT_EQ(PolicyNamespace(POLICY_DOMAIN_CHROME, ""), removed[0]);
  EXPECT_EQ(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "xyz"), removed[1]);
  ASSERT_EQ(1u, added.size());
  EXPECT_EQ(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc"), added[0]);
}

}  // namespace policy
