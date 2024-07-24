// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/conditions/attributes_condition.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_controls {

namespace {

constexpr char kGoogleUrl[] = "https://google.com";
constexpr char kChromiumUrl[] = "https://chromium.org";

base::Value CreateDict(const std::string& value) {
  auto dict = base::JSONReader::Read(value, base::JSON_ALLOW_TRAILING_COMMAS);
  EXPECT_TRUE(dict.has_value());
  return std::move(dict.value());
}

}  // namespace

TEST(AttributesConditionTest, InvalidSourceInputs) {
  // Invalid JSON types are rejected.
  ASSERT_FALSE(SourceAttributesCondition::Create(base::Value("some string")));
  ASSERT_FALSE(SourceAttributesCondition::Create(base::Value(12345)));
  ASSERT_FALSE(SourceAttributesCondition::Create(base::Value(99.999)));
  ASSERT_FALSE(SourceAttributesCondition::Create(
      base::Value(std::vector<char>({1, 2, 3, 4, 5}))));

  // Invalid dictionaries are rejected.
  ASSERT_FALSE(SourceAttributesCondition::Create(base::Value::Dict()));
  ASSERT_FALSE(SourceAttributesCondition::Create(CreateDict(R"({"foo": 1})")));

  // Dictionaries with correct keys but wrong schema for values are rejected
  ASSERT_FALSE(SourceAttributesCondition::Create(
      CreateDict(R"({"urls": "https://foo.com"})")));
  ASSERT_FALSE(SourceAttributesCondition::Create(CreateDict(R"({"urls": 1})")));
  ASSERT_FALSE(
      SourceAttributesCondition::Create(CreateDict(R"({"urls": 99.999})")));
  ASSERT_FALSE(
      SourceAttributesCondition::Create(CreateDict(R"({"incognito": "str"})")));
  ASSERT_FALSE(
      SourceAttributesCondition::Create(CreateDict(R"({"incognito": 1234})")));
  ASSERT_FALSE(SourceAttributesCondition::Create(
      CreateDict(R"({"os_clipboard": "str"})")));
  ASSERT_FALSE(SourceAttributesCondition::Create(
      CreateDict(R"({"os_clipboard": 1234})")));
  ASSERT_FALSE(SourceAttributesCondition::Create(
      CreateDict(R"({"other_profile": "str"})")));
  ASSERT_FALSE(SourceAttributesCondition::Create(
      CreateDict(R"({"other_profile": 1234})")));
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_FALSE(SourceAttributesCondition::Create(
      CreateDict(R"({"urls": "https://foo.com", "components": "ARC"})")));
  ASSERT_FALSE(SourceAttributesCondition::Create(
      CreateDict(R"({"urls": 1, "components": "ARC"})")));
  ASSERT_FALSE(SourceAttributesCondition::Create(
      CreateDict(R"({"urls": 99.999, "components": "ARC"})")));
  ASSERT_FALSE(SourceAttributesCondition::Create(
      CreateDict(R"({"components": "ARC"})")));
  ASSERT_FALSE(SourceAttributesCondition::Create(
      CreateDict(R"({"components": 12345})")));
  ASSERT_FALSE(SourceAttributesCondition::Create(
      CreateDict(R"({"components": 99.999})")));
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Dictionaries with valid schemas but invalid URL patterns or components are
  // rejected.
  ASSERT_FALSE(SourceAttributesCondition::Create(
      CreateDict(R"({"urls": ["http://:port"]})")));
  ASSERT_FALSE(SourceAttributesCondition::Create(
      CreateDict(R"({"urls": ["http://?query"]})")));
  ASSERT_FALSE(SourceAttributesCondition::Create(
      CreateDict(R"({"urls": ["https://"]})")));
  ASSERT_FALSE(
      SourceAttributesCondition::Create(CreateDict(R"({"urls": ["//"]})")));
  ASSERT_FALSE(
      SourceAttributesCondition::Create(CreateDict(R"({"urls": ["a", 1]})")));
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_FALSE(SourceAttributesCondition::Create(
      CreateDict(R"({"urls": ["a", 1], "components": ["ARC"]})")));
  ASSERT_FALSE(SourceAttributesCondition::Create(
      CreateDict(R"({"components": ["1", "a"]})")));
  ASSERT_FALSE(SourceAttributesCondition::Create(
      CreateDict(R"({"components": ["5.5"]})")));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST(AttributesConditionTest, InvalidDestinationInputs) {
  // Invalid JSON types are rejected.
  ASSERT_FALSE(
      DestinationAttributesCondition::Create(base::Value("some string")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(base::Value(12345)));
  ASSERT_FALSE(DestinationAttributesCondition::Create(base::Value(99.999)));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      base::Value(std::vector<char>({1, 2, 3, 4, 5}))));

  // Invalid dictionaries are rejected.
  ASSERT_FALSE(DestinationAttributesCondition::Create(base::Value::Dict()));
  ASSERT_FALSE(
      DestinationAttributesCondition::Create(CreateDict(R"({"foo": 1})")));

  // Dictionaries with correct keys but wrong schema for values are rejected
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"urls": "https://foo.com"})")));
  ASSERT_FALSE(
      DestinationAttributesCondition::Create(CreateDict(R"({"urls": 1})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"urls": 99.999})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"incognito": "str"})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"incognito": 1234})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"os_clipboard": "str"})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"os_clipboard": 1234})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"other_profile": "str"})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"other_profile": 1234})")));
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"urls": "https://foo.com", "components": "ARC"})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"urls": 1, "components": "ARC"})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"urls": 99.999, "components": "ARC"})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"components": "ARC"})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"components": 12345})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"components": 99.999})")));
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Dictionaries with valid schemas but invalid URL patterns or components are
  // rejected.
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"urls": ["http://:port"]})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"urls": ["http://?query"]})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"urls": ["https://"]})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"urls": ["//"]})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"urls": ["a", 1]})")));
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"urls": ["a", 1], "components": ["ARC"]})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"components": ["1", "a"]})")));
  ASSERT_FALSE(DestinationAttributesCondition::Create(
      CreateDict(R"({"components": ["5.5"]})")));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST(AttributesConditionTest, AnyURL) {
  auto any_source_url =
      SourceAttributesCondition::Create(CreateDict(R"({"urls": ["*"]})"));
  auto any_destination_url =
      DestinationAttributesCondition::Create(CreateDict(R"({"urls": ["*"]})"));
  ASSERT_TRUE(any_source_url);
  ASSERT_TRUE(any_destination_url);
  for (const char* url : {kGoogleUrl, kChromiumUrl}) {
    ActionContext context = {
        .source = {.url = GURL(url)},
        .destination = {.url = GURL(url)},
    };
    ASSERT_TRUE(any_source_url->IsTriggered(context));
    ASSERT_TRUE(any_destination_url->IsTriggered(context));
  }
}

TEST(AttributesConditionTest, SpecificSourceURL) {
  auto google_url_source = SourceAttributesCondition::Create(
      CreateDict(R"({"urls": ["google.com"]})"));
  auto chromium_url_source = SourceAttributesCondition::Create(
      CreateDict(R"({"urls": ["chromium.org"]})"));

  ASSERT_TRUE(google_url_source);
  ASSERT_TRUE(chromium_url_source);

  ASSERT_TRUE(
      google_url_source->IsTriggered({.source = {.url = GURL(kGoogleUrl)}}));
  ASSERT_TRUE(chromium_url_source->IsTriggered(
      {.source = {.url = GURL(kChromiumUrl)}}));

  ASSERT_FALSE(
      google_url_source->IsTriggered({.source = {.url = GURL(kChromiumUrl)}}));
  ASSERT_FALSE(
      chromium_url_source->IsTriggered({.source = {.url = GURL(kGoogleUrl)}}));
}

TEST(AttributesConditionTest, SpecificDestinationURL) {
  auto google_url_destination = DestinationAttributesCondition::Create(
      CreateDict(R"({"urls": ["google.com"]})"));
  auto chromium_url_destination = DestinationAttributesCondition::Create(
      CreateDict(R"({"urls": ["chromium.org"]})"));

  ASSERT_TRUE(google_url_destination);
  ASSERT_TRUE(chromium_url_destination);

  ASSERT_TRUE(google_url_destination->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl)}}));
  ASSERT_TRUE(chromium_url_destination->IsTriggered(
      {.destination = {.url = GURL(kChromiumUrl)}}));

  ASSERT_FALSE(google_url_destination->IsTriggered(
      {.destination = {.url = GURL(kChromiumUrl)}}));
  ASSERT_FALSE(chromium_url_destination->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl)}}));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST(AttributesConditionTest, AllComponents) {
  auto any_component = DestinationAttributesCondition::Create(CreateDict(R"(
    {
      "components": ["ARC", "CROSTINI", "PLUGIN_VM", "USB", "DRIVE", "ONEDRIVE"]
    })"));
  ASSERT_TRUE(any_component);
  for (Component component : kAllComponents) {
    ActionContext context = {.destination = {.component = component}};
    ASSERT_TRUE(any_component->IsTriggered(context));
  }
}

TEST(AttributesConditionTest, OneComponent) {
  for (Component condition_component : kAllComponents) {
    constexpr char kTemplate[] = R"({"components": ["%s"]})";
    auto one_component =
        DestinationAttributesCondition::Create(CreateDict(base::StringPrintf(
            kTemplate, GetComponentMapping(condition_component).c_str())));

    for (Component context_component : kAllComponents) {
      ActionContext context = {.destination = {.component = context_component}};
      if (context_component == condition_component) {
        ASSERT_TRUE(one_component->IsTriggered(context));
      } else {
        ASSERT_FALSE(one_component->IsTriggered(context));
      }
    }
  }
}

TEST(AttributesConditionTest, URLAndAllComponents) {
  auto any_component_or_url =
      DestinationAttributesCondition::Create(CreateDict(R"(
      {
        "urls": ["*"],
        "components": ["ARC", "CROSTINI", "PLUGIN_VM", "USB", "DRIVE",
                       "ONEDRIVE"]
      })"));
  ASSERT_TRUE(any_component_or_url);
  for (Component component : kAllComponents) {
    for (const char* url : {kGoogleUrl, kChromiumUrl}) {
      ActionContext context = {
          .destination = {.url = GURL(url), .component = component}};
      ASSERT_TRUE(any_component_or_url->IsTriggered(context));
    }
  }
}

TEST(AttributesConditionTest, URLAndOneComponent) {
  for (Component condition_component : kAllComponents) {
    constexpr char kTemplate[] =
        R"({"urls": ["google.com"], "components": ["%s"]})";
    auto google_and_one_component =
        DestinationAttributesCondition::Create(CreateDict(base::StringPrintf(
            kTemplate, GetComponentMapping(condition_component).c_str())));

    ASSERT_TRUE(google_and_one_component);
    for (Component context_component : kAllComponents) {
      for (const char* url : {kGoogleUrl, kChromiumUrl}) {
        ActionContext context = {
            .destination = {.url = GURL(url), .component = context_component}};
        if (context_component == condition_component && url == kGoogleUrl) {
          ASSERT_TRUE(google_and_one_component->IsTriggered(context))
              << "Expected " << GetComponentMapping(context_component)
              << " to trigger for " << url;
        } else {
          ASSERT_FALSE(google_and_one_component->IsTriggered(context))
              << "Expected " << GetComponentMapping(context_component)
              << " to not trigger for " << url;
        }
      }
    }
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST(AttributesConditionTest, IncognitoDestination) {
  // A context with only "incognito" and no URL shouldn't be evaluated.
  auto incognito_dst = DestinationAttributesCondition::Create(CreateDict(R"(
      {
        "incognito": true,
      })"));
  ASSERT_TRUE(incognito_dst);
  ASSERT_FALSE(
      incognito_dst->CanBeEvaluated({.destination = {.incognito = true}}));
  ASSERT_FALSE(
      incognito_dst->CanBeEvaluated({.destination = {.incognito = false}}));
  ASSERT_FALSE(incognito_dst->CanBeEvaluated({.source = {.incognito = true}}));
  ASSERT_FALSE(incognito_dst->CanBeEvaluated({.source = {.incognito = false}}));

  auto non_incognito_dst = DestinationAttributesCondition::Create(CreateDict(R"(
      {
        "incognito": false,
      })"));
  ASSERT_TRUE(non_incognito_dst);
  ASSERT_FALSE(
      non_incognito_dst->CanBeEvaluated({.destination = {.incognito = true}}));
  ASSERT_FALSE(
      non_incognito_dst->CanBeEvaluated({.destination = {.incognito = false}}));
  ASSERT_FALSE(
      non_incognito_dst->CanBeEvaluated({.source = {.incognito = true}}));
  ASSERT_FALSE(
      non_incognito_dst->CanBeEvaluated({.source = {.incognito = false}}));
}

TEST(AttributesConditionTest, IncognitoSource) {
  // A context with only "incognito" and no URL shouldn't be evaluated.
  auto incognito_src = SourceAttributesCondition::Create(CreateDict(R"(
      {
        "incognito": true,
      })"));
  ASSERT_TRUE(incognito_src);
  ASSERT_FALSE(
      incognito_src->CanBeEvaluated({.destination = {.incognito = true}}));
  ASSERT_FALSE(
      incognito_src->CanBeEvaluated({.destination = {.incognito = false}}));
  ASSERT_FALSE(incognito_src->CanBeEvaluated({.source = {.incognito = true}}));
  ASSERT_FALSE(incognito_src->CanBeEvaluated({.source = {.incognito = false}}));

  auto non_incognito_src = SourceAttributesCondition::Create(CreateDict(R"(
      {
        "incognito": false,
      })"));
  ASSERT_TRUE(non_incognito_src);
  ASSERT_FALSE(
      non_incognito_src->CanBeEvaluated({.destination = {.incognito = true}}));
  ASSERT_FALSE(
      non_incognito_src->CanBeEvaluated({.destination = {.incognito = false}}));
  ASSERT_FALSE(
      non_incognito_src->CanBeEvaluated({.source = {.incognito = true}}));
  ASSERT_FALSE(
      non_incognito_src->CanBeEvaluated({.source = {.incognito = false}}));
}

TEST(AttributesConditionTest, URLAndIncognitoDestination) {
  auto url_and_incognito = DestinationAttributesCondition::Create(CreateDict(R"(
      {
        "urls": ["google.com"],
        "incognito": true,
      })"));
  ASSERT_TRUE(url_and_incognito);
  ASSERT_TRUE(url_and_incognito->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl), .incognito = true}}));
  ASSERT_FALSE(url_and_incognito->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl), .incognito = false}}));
  ASSERT_FALSE(url_and_incognito->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl)}}));
  ASSERT_FALSE(url_and_incognito->IsTriggered(
      {.destination = {.url = GURL(kChromiumUrl), .incognito = true}}));
  ASSERT_FALSE(url_and_incognito->IsTriggered(
      {.destination = {.url = GURL(kChromiumUrl), .incognito = false}}));
  ASSERT_FALSE(url_and_incognito->IsTriggered(
      {.destination = {.url = GURL(kChromiumUrl)}}));
  ASSERT_FALSE(
      url_and_incognito->CanBeEvaluated({.destination = {.incognito = true}}));
  ASSERT_FALSE(
      url_and_incognito->CanBeEvaluated({.destination = {.incognito = false}}));

  auto url_and_not_incognito =
      DestinationAttributesCondition::Create(CreateDict(R"(
      {
        "urls": ["google.com"],
        "incognito": false,
      })"));
  ASSERT_TRUE(url_and_not_incognito);
  ASSERT_FALSE(url_and_not_incognito->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl), .incognito = true}}));
  ASSERT_TRUE(url_and_not_incognito->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl), .incognito = false}}));
  ASSERT_TRUE(url_and_not_incognito->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl)}}));
  ASSERT_FALSE(url_and_not_incognito->IsTriggered(
      {.destination = {.url = GURL(kChromiumUrl), .incognito = true}}));
  ASSERT_FALSE(url_and_not_incognito->IsTriggered(
      {.destination = {.url = GURL(kChromiumUrl), .incognito = false}}));
  ASSERT_FALSE(url_and_not_incognito->IsTriggered(
      {.destination = {.url = GURL(kChromiumUrl)}}));
  ASSERT_FALSE(url_and_not_incognito->CanBeEvaluated(
      {.destination = {.incognito = true}}));
  ASSERT_FALSE(url_and_not_incognito->CanBeEvaluated(
      {.destination = {.incognito = false}}));
}

TEST(AttributesConditionTest, URLAndIncognitoSource) {
  auto url_and_incognito = SourceAttributesCondition::Create(CreateDict(R"(
      {
        "urls": ["google.com"],
        "incognito": true,
      })"));
  ASSERT_TRUE(url_and_incognito);
  ASSERT_TRUE(url_and_incognito->IsTriggered(
      {.source = {.url = GURL(kGoogleUrl), .incognito = true}}));
  ASSERT_FALSE(url_and_incognito->IsTriggered(
      {.source = {.url = GURL(kGoogleUrl), .incognito = false}}));
  ASSERT_FALSE(
      url_and_incognito->IsTriggered({.source = {.url = GURL(kGoogleUrl)}}));
  ASSERT_FALSE(url_and_incognito->IsTriggered(
      {.source = {.url = GURL(kChromiumUrl), .incognito = true}}));
  ASSERT_FALSE(url_and_incognito->IsTriggered(
      {.source = {.url = GURL(kChromiumUrl), .incognito = false}}));
  ASSERT_FALSE(
      url_and_incognito->IsTriggered({.source = {.url = GURL(kChromiumUrl)}}));
  ASSERT_FALSE(
      url_and_incognito->CanBeEvaluated({.source = {.incognito = true}}));
  ASSERT_FALSE(
      url_and_incognito->CanBeEvaluated({.source = {.incognito = false}}));

  auto url_and_not_incognito = SourceAttributesCondition::Create(CreateDict(R"(
      {
        "urls": ["google.com"],
        "incognito": false,
      })"));
  ASSERT_TRUE(url_and_not_incognito);
  ASSERT_FALSE(url_and_not_incognito->IsTriggered(
      {.source = {.url = GURL(kGoogleUrl), .incognito = true}}));
  ASSERT_TRUE(url_and_not_incognito->IsTriggered(
      {.source = {.url = GURL(kGoogleUrl), .incognito = false}}));
  ASSERT_TRUE(url_and_not_incognito->IsTriggered(
      {.source = {.url = GURL(kGoogleUrl)}}));
  ASSERT_FALSE(url_and_not_incognito->IsTriggered(
      {.source = {.url = GURL(kChromiumUrl), .incognito = true}}));
  ASSERT_FALSE(url_and_not_incognito->IsTriggered(
      {.source = {.url = GURL(kChromiumUrl), .incognito = false}}));
  ASSERT_FALSE(url_and_not_incognito->IsTriggered(
      {.source = {.url = GURL(kChromiumUrl)}}));
  ASSERT_FALSE(
      url_and_not_incognito->CanBeEvaluated({.source = {.incognito = true}}));
  ASSERT_FALSE(
      url_and_not_incognito->CanBeEvaluated({.source = {.incognito = false}}));
}

TEST(AttributesConditionTest, URLAndNoIncognitoDestination) {
  // When "incognito" is not in the condition, its value in the context
  // shouldn't affect whether the condition is triggered or not.
  auto any_url = DestinationAttributesCondition::Create(CreateDict(R"(
      {
        "urls": ["*"],
      })"));
  ASSERT_TRUE(any_url);
  ASSERT_TRUE(any_url->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl), .incognito = true}}));
  ASSERT_TRUE(any_url->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl), .incognito = false}}));
  ASSERT_TRUE(any_url->IsTriggered({.destination = {.url = GURL(kGoogleUrl)}}));
  ASSERT_FALSE(any_url->CanBeEvaluated({.destination = {.incognito = true}}));
  ASSERT_FALSE(any_url->CanBeEvaluated({.destination = {.incognito = false}}));
  ASSERT_FALSE(any_url->CanBeEvaluated({.destination = {}}));
}

TEST(AttributesConditionTest, URLAndNoIncognitoSource) {
  // When "incognito" is not in the condition, its value in the context
  // shouldn't affect whether the condition is triggered or not.
  auto any_url = SourceAttributesCondition::Create(CreateDict(R"(
      {
        "urls": ["*"],
      })"));
  ASSERT_TRUE(any_url);
  ASSERT_TRUE(any_url->IsTriggered(
      {.source = {.url = GURL(kGoogleUrl), .incognito = true}}));
  ASSERT_TRUE(any_url->IsTriggered(
      {.source = {.url = GURL(kGoogleUrl), .incognito = false}}));
  ASSERT_TRUE(any_url->IsTriggered({.source = {.url = GURL(kGoogleUrl)}}));
  ASSERT_FALSE(any_url->CanBeEvaluated({.source = {.incognito = true}}));
  ASSERT_FALSE(any_url->CanBeEvaluated({.source = {.incognito = false}}));
  ASSERT_FALSE(any_url->CanBeEvaluated({.source = {}}));
}

TEST(AttributesConditionTest, OtherProfileDestination) {
  // A context with only "other_profile" and no URL shouldn't be evaluated.
  auto other_profile_dst = DestinationAttributesCondition::Create(CreateDict(R"(
      {
        "other_profile": true,
      })"));
  ASSERT_TRUE(other_profile_dst);
  ASSERT_FALSE(other_profile_dst->CanBeEvaluated(
      {.destination = {.other_profile = true}}));
  ASSERT_FALSE(other_profile_dst->CanBeEvaluated(
      {.destination = {.other_profile = false}}));
  ASSERT_FALSE(
      other_profile_dst->CanBeEvaluated({.source = {.other_profile = true}}));
  ASSERT_FALSE(
      other_profile_dst->CanBeEvaluated({.source = {.other_profile = false}}));

  auto non_other_profile_dst =
      DestinationAttributesCondition::Create(CreateDict(R"(
      {
        "other_profile": false,
      })"));
  ASSERT_TRUE(non_other_profile_dst);
  ASSERT_FALSE(non_other_profile_dst->CanBeEvaluated(
      {.destination = {.other_profile = true}}));
  ASSERT_FALSE(non_other_profile_dst->CanBeEvaluated(
      {.destination = {.other_profile = false}}));
  ASSERT_FALSE(non_other_profile_dst->CanBeEvaluated(
      {.source = {.other_profile = true}}));
  ASSERT_FALSE(non_other_profile_dst->CanBeEvaluated(
      {.source = {.other_profile = false}}));
}

TEST(AttributesConditionTest, OtherProfileSource) {
  // A context with only "other_profile" and no URL shouldn't be evaluated.
  auto other_profile_src = SourceAttributesCondition::Create(CreateDict(R"(
      {
        "other_profile": true,
      })"));
  ASSERT_TRUE(other_profile_src);
  ASSERT_FALSE(other_profile_src->CanBeEvaluated(
      {.destination = {.other_profile = true}}));
  ASSERT_FALSE(other_profile_src->CanBeEvaluated(
      {.destination = {.other_profile = false}}));
  ASSERT_FALSE(
      other_profile_src->CanBeEvaluated({.source = {.other_profile = true}}));
  ASSERT_FALSE(
      other_profile_src->CanBeEvaluated({.source = {.other_profile = false}}));

  auto non_other_profile_src = SourceAttributesCondition::Create(CreateDict(R"(
      {
        "other_profile": false,
      })"));
  ASSERT_TRUE(non_other_profile_src);
  ASSERT_FALSE(non_other_profile_src->CanBeEvaluated(
      {.destination = {.other_profile = true}}));
  ASSERT_FALSE(non_other_profile_src->CanBeEvaluated(
      {.destination = {.other_profile = false}}));
  ASSERT_FALSE(non_other_profile_src->CanBeEvaluated(
      {.source = {.other_profile = true}}));
  ASSERT_FALSE(non_other_profile_src->CanBeEvaluated(
      {.source = {.other_profile = false}}));
}

TEST(AttributesConditionTest, URLAndOtherProfileDestination) {
  auto url_and_other_profile =
      DestinationAttributesCondition::Create(CreateDict(R"(
      {
        "urls": ["google.com"],
        "other_profile": true,
      })"));
  ASSERT_TRUE(url_and_other_profile);
  ASSERT_TRUE(url_and_other_profile->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl), .other_profile = true}}));
  ASSERT_FALSE(url_and_other_profile->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl), .other_profile = false}}));
  ASSERT_FALSE(url_and_other_profile->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl)}}));
  ASSERT_FALSE(url_and_other_profile->IsTriggered(
      {.destination = {.url = GURL(kChromiumUrl), .other_profile = true}}));
  ASSERT_FALSE(url_and_other_profile->IsTriggered(
      {.destination = {.url = GURL(kChromiumUrl), .other_profile = false}}));
  ASSERT_FALSE(url_and_other_profile->IsTriggered(
      {.destination = {.url = GURL(kChromiumUrl)}}));
  ASSERT_FALSE(url_and_other_profile->CanBeEvaluated(
      {.destination = {.other_profile = true}}));
  ASSERT_FALSE(url_and_other_profile->CanBeEvaluated(
      {.destination = {.other_profile = false}}));

  auto url_and_not_other_profile =
      DestinationAttributesCondition::Create(CreateDict(R"(
      {
        "urls": ["google.com"],
        "other_profile": false,
      })"));
  ASSERT_TRUE(url_and_not_other_profile);
  ASSERT_FALSE(url_and_not_other_profile->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl), .other_profile = true}}));
  ASSERT_TRUE(url_and_not_other_profile->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl), .other_profile = false}}));
  ASSERT_TRUE(url_and_not_other_profile->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl)}}));
  ASSERT_FALSE(url_and_not_other_profile->IsTriggered(
      {.destination = {.url = GURL(kChromiumUrl), .other_profile = true}}));
  ASSERT_FALSE(url_and_not_other_profile->IsTriggered(
      {.destination = {.url = GURL(kChromiumUrl), .other_profile = false}}));
  ASSERT_FALSE(url_and_not_other_profile->IsTriggered(
      {.destination = {.url = GURL(kChromiumUrl)}}));
  ASSERT_FALSE(url_and_not_other_profile->CanBeEvaluated(
      {.destination = {.other_profile = true}}));
  ASSERT_FALSE(url_and_not_other_profile->CanBeEvaluated(
      {.destination = {.other_profile = false}}));
}

TEST(AttributesConditionTest, URLAndOtherProfileSource) {
  auto url_and_other_profile = SourceAttributesCondition::Create(CreateDict(R"(
      {
        "urls": ["google.com"],
        "other_profile": true,
      })"));
  ASSERT_TRUE(url_and_other_profile);
  ASSERT_TRUE(url_and_other_profile->IsTriggered(
      {.source = {.url = GURL(kGoogleUrl), .other_profile = true}}));
  ASSERT_FALSE(url_and_other_profile->IsTriggered(
      {.source = {.url = GURL(kGoogleUrl), .other_profile = false}}));
  ASSERT_FALSE(url_and_other_profile->IsTriggered(
      {.source = {.url = GURL(kGoogleUrl)}}));
  ASSERT_FALSE(url_and_other_profile->IsTriggered(
      {.source = {.url = GURL(kChromiumUrl), .other_profile = true}}));
  ASSERT_FALSE(url_and_other_profile->IsTriggered(
      {.source = {.url = GURL(kChromiumUrl), .other_profile = false}}));
  ASSERT_FALSE(url_and_other_profile->IsTriggered(
      {.source = {.url = GURL(kChromiumUrl)}}));
  ASSERT_FALSE(url_and_other_profile->CanBeEvaluated(
      {.source = {.other_profile = true}}));
  ASSERT_FALSE(url_and_other_profile->CanBeEvaluated(
      {.source = {.other_profile = false}}));

  auto url_and_not_other_profile =
      SourceAttributesCondition::Create(CreateDict(R"(
      {
        "urls": ["google.com"],
        "other_profile": false,
      })"));
  ASSERT_TRUE(url_and_not_other_profile);
  ASSERT_FALSE(url_and_not_other_profile->IsTriggered(
      {.source = {.url = GURL(kGoogleUrl), .other_profile = true}}));
  ASSERT_TRUE(url_and_not_other_profile->IsTriggered(
      {.source = {.url = GURL(kGoogleUrl), .other_profile = false}}));
  ASSERT_TRUE(url_and_not_other_profile->IsTriggered(
      {.source = {.url = GURL(kGoogleUrl)}}));
  ASSERT_FALSE(url_and_not_other_profile->IsTriggered(
      {.source = {.url = GURL(kChromiumUrl), .other_profile = true}}));
  ASSERT_FALSE(url_and_not_other_profile->IsTriggered(
      {.source = {.url = GURL(kChromiumUrl), .other_profile = false}}));
  ASSERT_FALSE(url_and_not_other_profile->IsTriggered(
      {.source = {.url = GURL(kChromiumUrl)}}));
  ASSERT_FALSE(url_and_not_other_profile->CanBeEvaluated(
      {.source = {.other_profile = true}}));
  ASSERT_FALSE(url_and_not_other_profile->CanBeEvaluated(
      {.source = {.other_profile = false}}));
}

TEST(AttributesConditionTest, URLAndNoOtherProfileDestination) {
  // When "other_profile" is not in the condition, its value in the context
  // shouldn't affect whether the condition is triggered or not.
  auto any_url = DestinationAttributesCondition::Create(CreateDict(R"(
      {
        "urls": ["*"],
      })"));
  ASSERT_TRUE(any_url);
  ASSERT_TRUE(any_url->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl), .other_profile = true}}));
  ASSERT_TRUE(any_url->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl), .other_profile = false}}));
  ASSERT_TRUE(any_url->IsTriggered({.destination = {.url = GURL(kGoogleUrl)}}));
  ASSERT_FALSE(
      any_url->CanBeEvaluated({.destination = {.other_profile = true}}));
  ASSERT_FALSE(
      any_url->CanBeEvaluated({.destination = {.other_profile = false}}));
  ASSERT_FALSE(any_url->CanBeEvaluated({.destination = {}}));
}

TEST(AttributesConditionTest, URLAndNoOtherProfileSource) {
  // When "other_profile" is not in the condition, its value in the context
  // shouldn't affect whether the condition is triggered or not.
  auto any_url = SourceAttributesCondition::Create(CreateDict(R"(
      {
        "urls": ["*"],
      })"));
  ASSERT_TRUE(any_url);
  ASSERT_TRUE(any_url->IsTriggered(
      {.source = {.url = GURL(kGoogleUrl), .other_profile = true}}));
  ASSERT_TRUE(any_url->IsTriggered(
      {.source = {.url = GURL(kGoogleUrl), .other_profile = false}}));
  ASSERT_TRUE(any_url->IsTriggered({.source = {.url = GURL(kGoogleUrl)}}));
  ASSERT_FALSE(any_url->CanBeEvaluated({.source = {.other_profile = true}}));
  ASSERT_FALSE(any_url->CanBeEvaluated({.source = {.other_profile = false}}));
  ASSERT_FALSE(any_url->CanBeEvaluated({.source = {}}));
}

TEST(AttributesConditionTest, URLOtherProfileIncognitoSource) {
  auto condition = SourceAttributesCondition::Create(CreateDict(R"(
      {
        "urls": ["google.com"],
        "incognito": true,
        "other_profile": true,
      })"));
  ASSERT_TRUE(condition);

  // The condition only triggers when all 3 tab-related attributes are matched,
  // any other context should fail to trigger it.
  ASSERT_TRUE(condition->IsTriggered({.source = {.url = GURL(kGoogleUrl),
                                                 .incognito = true,
                                                 .other_profile = true}}));

  ASSERT_FALSE(condition->IsTriggered(
      {.source = {.url = GURL(kGoogleUrl), .incognito = true}}));
  ASSERT_FALSE(condition->IsTriggered(
      {.source = {.url = GURL(kGoogleUrl), .other_profile = true}}));
  ASSERT_FALSE(condition->IsTriggered({.source = {.url = GURL(kChromiumUrl),
                                                  .incognito = true,
                                                  .other_profile = true}}));
  ASSERT_FALSE(condition->IsTriggered(
      {.source = {.url = GURL(kChromiumUrl), .incognito = true}}));
  ASSERT_FALSE(condition->IsTriggered(
      {.source = {.url = GURL(kChromiumUrl), .other_profile = true}}));
  ASSERT_FALSE(condition->IsTriggered({.source = {.url = GURL(kGoogleUrl)}}));
  ASSERT_FALSE(condition->IsTriggered({.source = {.url = GURL(kChromiumUrl)}}));
  ASSERT_FALSE(condition->CanBeEvaluated({.source = {.incognito = true}}));
  ASSERT_FALSE(condition->CanBeEvaluated({.source = {.incognito = false}}));

  ASSERT_FALSE(condition->CanBeEvaluated({.source = {.other_profile = true}}));
  ASSERT_FALSE(condition->CanBeEvaluated({.source = {.other_profile = false}}));
}

TEST(AttributesConditionTest, URLOtherProfileIncognitoDestination) {
  auto condition = DestinationAttributesCondition::Create(CreateDict(R"(
      {
        "urls": ["google.com"],
        "incognito": true,
        "other_profile": true,
      })"));
  ASSERT_TRUE(condition);

  // The condition only triggers when all 3 tab-related attributes are matched,
  // any other context should fail to trigger it.
  ASSERT_TRUE(condition->IsTriggered({.destination = {.url = GURL(kGoogleUrl),
                                                      .incognito = true,
                                                      .other_profile = true}}));

  ASSERT_FALSE(condition->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl), .incognito = true}}));
  ASSERT_FALSE(condition->IsTriggered(
      {.destination = {.url = GURL(kGoogleUrl), .other_profile = true}}));
  ASSERT_FALSE(
      condition->IsTriggered({.destination = {.url = GURL(kChromiumUrl),
                                              .incognito = true,
                                              .other_profile = true}}));
  ASSERT_FALSE(condition->IsTriggered(
      {.destination = {.url = GURL(kChromiumUrl), .incognito = true}}));
  ASSERT_FALSE(condition->IsTriggered(
      {.destination = {.url = GURL(kChromiumUrl), .other_profile = true}}));
  ASSERT_FALSE(
      condition->IsTriggered({.destination = {.url = GURL(kGoogleUrl)}}));
  ASSERT_FALSE(
      condition->IsTriggered({.destination = {.url = GURL(kChromiumUrl)}}));
  ASSERT_FALSE(condition->CanBeEvaluated({.destination = {.incognito = true}}));
  ASSERT_FALSE(
      condition->CanBeEvaluated({.destination = {.incognito = false}}));

  ASSERT_FALSE(
      condition->CanBeEvaluated({.destination = {.other_profile = true}}));
  ASSERT_FALSE(
      condition->CanBeEvaluated({.destination = {.other_profile = false}}));
}

TEST(AttributesConditionTest, OsClipboardDestination) {
  auto os_clipboard_dst = DestinationAttributesCondition::Create(CreateDict(R"(
      {
        "os_clipboard": true,
      })"));
  ASSERT_TRUE(os_clipboard_dst);
  ASSERT_TRUE(
      os_clipboard_dst->IsTriggered({.destination = {.os_clipboard = true}}));
  ASSERT_FALSE(os_clipboard_dst->CanBeEvaluated(
      {.destination = {.os_clipboard = false}}));
  ASSERT_FALSE(
      os_clipboard_dst->CanBeEvaluated({.source = {.os_clipboard = true}}));
  ASSERT_FALSE(
      os_clipboard_dst->CanBeEvaluated({.source = {.os_clipboard = false}}));

  auto non_os_clipboard_dst =
      DestinationAttributesCondition::Create(CreateDict(R"(
      {
        "os_clipboard": false,
      })"));
  ASSERT_TRUE(non_os_clipboard_dst);
  ASSERT_FALSE(non_os_clipboard_dst->IsTriggered(
      {.destination = {.os_clipboard = true}}));
  ASSERT_FALSE(non_os_clipboard_dst->CanBeEvaluated(
      {.destination = {.os_clipboard = false}}));
  ASSERT_FALSE(
      non_os_clipboard_dst->CanBeEvaluated({.source = {.os_clipboard = true}}));
  ASSERT_FALSE(non_os_clipboard_dst->CanBeEvaluated(
      {.source = {.os_clipboard = false}}));
}

TEST(AttributesConditionTest, OsClipboardSource) {
  auto os_clipboard_src = SourceAttributesCondition::Create(CreateDict(R"(
      {
        "os_clipboard": true,
      })"));
  ASSERT_TRUE(os_clipboard_src);
  ASSERT_FALSE(os_clipboard_src->CanBeEvaluated(
      {.destination = {.os_clipboard = true}}));
  ASSERT_FALSE(os_clipboard_src->CanBeEvaluated(
      {.destination = {.os_clipboard = false}}));
  ASSERT_TRUE(
      os_clipboard_src->IsTriggered({.source = {.os_clipboard = true}}));
  ASSERT_FALSE(
      os_clipboard_src->CanBeEvaluated({.source = {.os_clipboard = false}}));

  auto non_os_clipboard_src = SourceAttributesCondition::Create(CreateDict(R"(
      {
        "os_clipboard": false,
      })"));
  ASSERT_TRUE(non_os_clipboard_src);
  ASSERT_FALSE(
      non_os_clipboard_src->IsTriggered({.source = {.os_clipboard = true}}));
  ASSERT_FALSE(non_os_clipboard_src->CanBeEvaluated(
      {.source = {.os_clipboard = false}}));
  ASSERT_FALSE(non_os_clipboard_src->CanBeEvaluated(
      {.destination = {.os_clipboard = true}}));
  ASSERT_FALSE(non_os_clipboard_src->CanBeEvaluated(
      {.destination = {.os_clipboard = false}}));
}

}  // namespace data_controls
