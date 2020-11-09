// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/schema_org/extractor.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/schema_org/common/improved_metadata.mojom.h"
#include "components/schema_org/schema_org_entity_names.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace schema_org {

using improved::mojom::Entity;
using improved::mojom::EntityPtr;
using improved::mojom::Property;
using improved::mojom::PropertyPtr;
using improved::mojom::Values;
using improved::mojom::ValuesPtr;

class SchemaOrgExtractorTest : public testing::Test {
 public:
  SchemaOrgExtractorTest() : extractor_({entity::kVideoObject}) {}

 protected:
  EntityPtr Extract(const std::string& text) {
    base::RunLoop run_loop;
    EntityPtr out;

    extractor_.Extract(text, base::BindLambdaForTesting([&](EntityPtr entity) {
                         out = std::move(entity);
                         run_loop.Quit();
                       }));

    run_loop.Run();
    return out;
  }

  PropertyPtr CreateStringProperty(const std::string& name,
                                   const std::string& value);

  PropertyPtr CreateBooleanProperty(const std::string& name, const bool& value);

  PropertyPtr CreateLongProperty(const std::string& name, const int64_t& value);

  PropertyPtr CreateDoubleProperty(const std::string& name, double value);

  PropertyPtr CreateDateTimeProperty(const std::string& name,
                                     const base::Time& value);

  PropertyPtr CreateTimeProperty(const std::string& name,
                                 const base::TimeDelta& value);

  PropertyPtr CreateUrlProperty(const std::string& name, const GURL& url);

  PropertyPtr CreateEntityProperty(const std::string& name, EntityPtr value);

  base::test::TaskEnvironment task_environment_;

 private:
  Extractor extractor_;
  data_decoder::test::InProcessDataDecoder data_decoder_;
};

PropertyPtr SchemaOrgExtractorTest::CreateStringProperty(
    const std::string& name,
    const std::string& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->string_values.push_back(value);
  return property;
}

PropertyPtr SchemaOrgExtractorTest::CreateBooleanProperty(
    const std::string& name,
    const bool& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->bool_values.push_back(value);
  return property;
}

PropertyPtr SchemaOrgExtractorTest::CreateLongProperty(const std::string& name,
                                                       const int64_t& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->long_values.push_back(value);
  return property;
}

PropertyPtr SchemaOrgExtractorTest::CreateDoubleProperty(
    const std::string& name,
    double value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->double_values.push_back(value);
  return property;
}

PropertyPtr SchemaOrgExtractorTest::CreateDateTimeProperty(
    const std::string& name,
    const base::Time& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->date_time_values.push_back(value);
  return property;
}

PropertyPtr SchemaOrgExtractorTest::CreateTimeProperty(
    const std::string& name,
    const base::TimeDelta& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->time_values.push_back(value);
  return property;
}

PropertyPtr SchemaOrgExtractorTest::CreateUrlProperty(const std::string& name,
                                                      const GURL& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->url_values.push_back(value);
  return property;
}

PropertyPtr SchemaOrgExtractorTest::CreateEntityProperty(
    const std::string& name,
    EntityPtr value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->entity_values.push_back(std::move(value));
  return property;
}

TEST_F(SchemaOrgExtractorTest, Empty) {
  ASSERT_TRUE(Extract("").is_null());
}

TEST_F(SchemaOrgExtractorTest, Basic) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\", \"@id\": \"1\", \"name\": \"a video!\"}");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->id = "1";
  expected->properties.push_back(CreateStringProperty("name", "a video!"));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, BooleanValue) {
  EntityPtr extracted =
      Extract("{\"@type\": \"VideoObject\", \"requiresSubscription\": true }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(
      CreateBooleanProperty("requiresSubscription", true));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, BooleanValueAsString) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\", \"requiresSubscription\": "
      "\"https://schema.org/True\" }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(
      CreateBooleanProperty("requiresSubscription", true));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, LongValue) {
  EntityPtr extracted =
      Extract("{\"@type\": \"VideoObject\", \"position\": 111 }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(CreateLongProperty("position", 111));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, DoubleValue) {
  EntityPtr extracted =
      Extract("{\"@type\": \"VideoObject\", \"copyrightYear\": 1999.5 }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(CreateDoubleProperty("copyrightYear", 1999.5));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, StringValueRepresentingLong) {
  EntityPtr extracted =
      Extract("{\"@type\": \"VideoObject\",\"copyrightYear\": \"1999\"}");

  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(CreateLongProperty("copyrightYear", 1999));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, StringValueRepresentingDouble) {
  EntityPtr extracted =
      Extract("{\"@type\": \"VideoObject\",\"copyrightYear\": \"1999.5\"}");

  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(CreateDoubleProperty("copyrightYear", 1999.5));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, StringValueRepresentingTime) {
  EntityPtr extracted =
      Extract("{\"@type\": \"VideoObject\",\"startTime\": \"05:30:00\"}");

  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(CreateTimeProperty(
      "startTime", base::TimeDelta::FromMinutes(60 * 5 + 30)));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, StringValueRepresentingDuration) {
  EntityPtr extracted =
      Extract("{\"@type\": \"VideoObject\",\"duration\": \"PT2H0M55S\"}");

  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(CreateTimeProperty(
      "duration",
      base::TimeDelta::FromHours(2) + base::TimeDelta::FromSeconds(55)));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, StringValueRepresentingLongDuration) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\",\"duration\": \"PT1234H5678M1234S\"}");

  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(
      CreateTimeProperty("duration", base::TimeDelta::FromHours(1234) +
                                         base::TimeDelta::FromMinutes(5678) +
                                         base::TimeDelta::FromSeconds(1234)));

  EXPECT_EQ(expected, extracted);
}

// startTime can be a DateTime or a Time. If it parses as DateTime successfully,
// we should use that type.
TEST_F(SchemaOrgExtractorTest, StringValueRepresentingDateTimeOrTime) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\",\"startTime\": "
      "\"2012-12-12T00:00:00 GMT\"}");

  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(CreateDateTimeProperty(
      "startTime", base::Time::FromDeltaSinceWindowsEpoch(
                       base::TimeDelta::FromMilliseconds(12999744000000))));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, StringValueRepresentingDateTime) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\",\"dateCreated\": "
      "\"2012-12-12T00:00:00 GMT\"}");

  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(CreateDateTimeProperty(
      "dateCreated", base::Time::FromDeltaSinceWindowsEpoch(
                         base::TimeDelta::FromMilliseconds(12999744000000))));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, StringValueRepresentingEnum) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\",\"potentialAction\": {\"@type\": "
      "\"Action\", \"actionStatus\": "
      "\"http://schema.org/ActiveActionStatus\"}}");

  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  EntityPtr action = Entity::New();
  action->type = "Action";
  action->properties.push_back(CreateUrlProperty(
      "actionStatus", GURL("http://schema.org/ActiveActionStatus")));
  expected->properties.push_back(
      CreateEntityProperty("potentialAction", std::move(action)));

  EXPECT_EQ(expected, extracted);
}

// The extractor should accept a string value for a property that has an
// expected entity type. https://schema.org/docs/gs.html#schemaorg_expected
TEST_F(SchemaOrgExtractorTest, StringValueForThingType) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\",\"author\": \"Google Chrome Developers\"}");

  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(
      CreateStringProperty("author", "Google Chrome Developers"));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, UrlValue) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\", "
      "\"contentUrl\":\"https://www.google.com\"}");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(
      CreateUrlProperty("contentUrl", GURL("https://www.google.com")));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, NestedEntities) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\", \"actor\": { \"@type\": \"Person\", "
      "\"name\": \"Talented Actor\" }  }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";

  EntityPtr nested = Entity::New();
  nested->type = "Person";
  nested->properties.push_back(CreateStringProperty("name", "Talented Actor"));

  expected->properties.push_back(
      CreateEntityProperty("actor", std::move(nested)));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, RepeatedProperty) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\", \"name\": [\"Movie Title\", \"The Second "
      "One\"] }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";

  PropertyPtr name = Property::New();
  name->name = "name";
  name->values = Values::New();
  name->values->string_values = {"Movie Title", "The Second One"};

  expected->properties.push_back(std::move(name));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, RepeatedPropertyNestedArray) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\", \"name\": [[\"Movie Title\", \"The Second "
      "One\"], [\"Different List Name\"]] }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";

  PropertyPtr name = Property::New();
  name->name = "name";
  name->values = Values::New();
  name->values->string_values = {"Movie Title", "The Second One",
                                 "Different List Name"};

  expected->properties.push_back(std::move(name));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, RepeatedPropertyNestedArrayMaxDepth) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\", \"name\": [[\"Movie Title\", \"The Second "
      "One\"], [[\"this one is too deeply nested\"]]] }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";

  PropertyPtr name = Property::New();
  name->name = "name";
  name->values = Values::New();
  name->values->string_values = {"Movie Title", "The Second One"};

  expected->properties.push_back(std::move(name));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, MixedRepeatedProperty) {
  EntityPtr extracted =
      Extract("{\"@type\": \"VideoObject\", \"version\": [\"6.5a\", 6] }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";

  PropertyPtr version = Property::New();
  version->name = "version";
  version->values = Values::New();
  version->values->string_values.push_back("6.5a");
  version->values->long_values.push_back(6);

  expected->properties.push_back(std::move(version));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, RepeatedObject) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\", \"actor\": [ {\"@type\": \"Person\", "
      "\"name\": \"Talented "
      "Actor\"}, {\"@type\": \"Person\", \"name\": \"Famous Actor\"} ] }");

  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  PropertyPtr actorProperty = Property::New();
  actorProperty->name = "actor";
  actorProperty->values = Values::New();

  EntityPtr nested1 = Entity::New();
  nested1->type = "Person";
  nested1->properties.push_back(CreateStringProperty("name", "Talented Actor"));
  actorProperty->values->entity_values.push_back(std::move(nested1));

  EntityPtr nested2 = Entity::New();
  nested2->type = "Person";
  nested2->properties.push_back(CreateStringProperty("name", "Famous Actor"));
  actorProperty->values->entity_values.push_back(std::move(nested2));

  expected->properties.push_back(std::move(actorProperty));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, TruncateLongString) {
  std::string maxLengthString = "";
  for (int i = 0; i < 200; ++i) {
    maxLengthString += "a";
  }
  std::string tooLongString;
  tooLongString.append(maxLengthString);
  tooLongString.append("a");

  EntityPtr extracted = Extract("{\"@type\": \"VideoObject\", \"name\": \"" +
                                tooLongString + "\"}");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(CreateStringProperty("name", maxLengthString));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, EnforceTypeExists) {
  EntityPtr extracted = Extract("{\"name\": \"a video!\"}");
  ASSERT_TRUE(extracted.is_null());
}

TEST_F(SchemaOrgExtractorTest, UnhandledTypeIgnored) {
  EntityPtr extracted =
      Extract("{\"@type\": \"UnsupportedType\", \"name\": \"a video!\"}");
  ASSERT_TRUE(extracted.is_null());
}

TEST_F(SchemaOrgExtractorTest, TruncateTooManyValuesInField) {
  std::string largeRepeatedField = "[";
  for (int i = 0; i < 101; ++i) {
    largeRepeatedField += "\"a\"";
    if (i != 100) {
      largeRepeatedField += ",";
    }
  }
  largeRepeatedField += "]";

  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\", \"name\": " + largeRepeatedField + "}");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  PropertyPtr name = Property::New();
  name->name = "name";
  name->values = Values::New();
  std::vector<std::string> nameValues;

  for (int i = 0; i < 100; i++) {
    nameValues.push_back("a");
  }
  name->values->string_values = std::move(nameValues);
  expected->properties.push_back(std::move(name));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, TruncateTooManyProperties) {
  // Create an entity with more than the supported number of properties. All the
  // properties must be valid to be included. 26 properties below, should
  // truncate to 25.
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\","
      "\"name\": \"a video!\","
      "\"transcript\":\"a short movie\","
      "\"videoFrameSize\":\"1200x800\","
      "\"videoQuality\":\"high\","
      "\"bitrate\":\"24mbps\","
      "\"contentSize\":\"8MB\","
      "\"encodingFormat\":\"H264\","
      "\"accessMode\":\"visual\","
      "\"accessibilitySummary\":\"short description\","
      "\"alternativeHeadline\":\"OR other title\","
      "\"award\":\"best picture\","
      "\"educationalUse\":\"assignment\","
      "\"headline\":\"headline\","
      "\"interactivityType\":\"active\","
      "\"keywords\":\"video\","
      "\"learningResourceType\":\"presentation\","
      "\"material\":\"film\","
      "\"mentions\":\"other work\","
      "\"schemaVersion\":\"http://schema.org/version/2.0/\","
      "\"text\":\"a short work\","
      "\"typicalAgeRange\":\"5-\","
      "\"version\":\"5\","
      "\"alternateName\":\"other title\","
      "\"description\":\"a short description\","
      "\"disambiguatingDescription\":\"clarifying point\","
      "\"identifier\":\"ID12345\""
      "}");

  ASSERT_FALSE(extracted.is_null());

  EXPECT_EQ(25u, extracted->properties.size());
}

TEST_F(SchemaOrgExtractorTest, IgnorePropertyWithEmptyArray) {
  EntityPtr extracted = Extract("{\"@type\": \"VideoObject\", \"name\": [] }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, EnforceMaxNestingDepth) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\", \"name\": \"a video!\","
      "\"actor\": {"
      "  \"address\": {"
      "    \"addressCountry\": {"
      "      \"containedInPlace\": {"
      "        \"containedInPlace\": {"
      "          \"containedInPlace\": {"
      "            \"containedInPlace\": {"
      "              \"containedInPlace\": {"
      "                \"containedInPlace\": {"
      "                  \"containedInPlace\": {"
      "                    \"name\": \"matroska\""
      "                  }"
      "                }"
      "              }"
      "            }"
      "          }"
      "        }"
      "      }"
      "    }"
      "  }"
      "}"
      "}");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";

  EntityPtr entity1 = Entity::New();
  entity1->type = "Thing";
  EntityPtr entity2 = Entity::New();
  entity2->type = "Thing";
  EntityPtr entity3 = Entity::New();
  entity3->type = "Thing";
  EntityPtr entity4 = Entity::New();
  entity4->type = "Thing";
  EntityPtr entity5 = Entity::New();
  entity5->type = "Thing";
  EntityPtr entity6 = Entity::New();
  entity6->type = "Thing";
  EntityPtr entity7 = Entity::New();
  entity7->type = "Thing";
  EntityPtr entity8 = Entity::New();
  entity8->type = "Thing";
  EntityPtr entity9 = Entity::New();
  entity9->type = "Thing";

  entity8->properties.push_back(
      CreateEntityProperty("containedInPlace", std::move(entity9)));
  entity7->properties.push_back(
      CreateEntityProperty("containedInPlace", std::move(entity8)));
  entity6->properties.push_back(
      CreateEntityProperty("containedInPlace", std::move(entity7)));
  entity5->properties.push_back(
      CreateEntityProperty("containedInPlace", std::move(entity6)));
  entity4->properties.push_back(
      CreateEntityProperty("containedInPlace", std::move(entity5)));
  entity3->properties.push_back(
      CreateEntityProperty("containedInPlace", std::move(entity4)));
  entity2->properties.push_back(
      CreateEntityProperty("addressCountry", std::move(entity3)));
  entity1->properties.push_back(
      CreateEntityProperty("address", std::move(entity2)));
  expected->properties.push_back(
      CreateEntityProperty("actor", std::move(entity1)));
  expected->properties.push_back(CreateStringProperty("name", "a video!"));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, MaxNestingDepthWithTerminalProperty) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\", \"name\": \"a video!\","
      "\"actor\": {"
      "  \"address\": {"
      "    \"addressCountry\": {"
      "      \"containedInPlace\": {"
      "        \"containedInPlace\": {"
      "          \"containedInPlace\": {"
      "            \"containedInPlace\": {"
      "              \"containedInPlace\": {"
      "                \"containedInPlace\": {"
      "                   \"name\": \"matroska\""
      "                }"
      "              }"
      "            }"
      "          }"
      "        }"
      "      }"
      "    }"
      "  }"
      "}"
      "}");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";

  EntityPtr entity1 = Entity::New();
  entity1->type = "Thing";
  EntityPtr entity2 = Entity::New();
  entity2->type = "Thing";
  EntityPtr entity3 = Entity::New();
  entity3->type = "Thing";
  EntityPtr entity4 = Entity::New();
  entity4->type = "Thing";
  EntityPtr entity5 = Entity::New();
  entity5->type = "Thing";
  EntityPtr entity6 = Entity::New();
  entity6->type = "Thing";
  EntityPtr entity7 = Entity::New();
  entity7->type = "Thing";
  EntityPtr entity8 = Entity::New();
  entity8->type = "Thing";
  EntityPtr entity9 = Entity::New();
  entity9->type = "Thing";

  entity9->properties.push_back(CreateStringProperty("name", "matroska"));
  entity8->properties.push_back(
      CreateEntityProperty("containedInPlace", std::move(entity9)));
  entity7->properties.push_back(
      CreateEntityProperty("containedInPlace", std::move(entity8)));
  entity6->properties.push_back(
      CreateEntityProperty("containedInPlace", std::move(entity7)));
  entity5->properties.push_back(
      CreateEntityProperty("containedInPlace", std::move(entity6)));
  entity4->properties.push_back(
      CreateEntityProperty("containedInPlace", std::move(entity5)));
  entity3->properties.push_back(
      CreateEntityProperty("containedInPlace", std::move(entity4)));
  entity2->properties.push_back(
      CreateEntityProperty("addressCountry", std::move(entity3)));
  entity1->properties.push_back(
      CreateEntityProperty("address", std::move(entity2)));
  expected->properties.push_back(
      CreateEntityProperty("actor", std::move(entity1)));
  expected->properties.push_back(CreateStringProperty("name", "a video!"));

  EXPECT_EQ(expected, extracted);
}

}  // namespace schema_org
