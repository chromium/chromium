// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/bloom_result_builder.h"

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/values.h"
#include "chromeos/components/bloom/bloom_result_builder.h"
#include "chromeos/components/bloom/public/cpp/bloom_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace bloom {

namespace {

// Implemented as a macro so we can fail the test when any of the |ASSERT| fail.
#define GET_NTH_ELEMENT(result, index)       \
  ({                                         \
    ASSERT_GE(result.size(), (size_t)index); \
    result[index].get();                     \
  })

// Implemented as a macro so we can fail the test when any of the |ASSERT| fail.
#define GET_FIRST_ENTRY(result)                       \
  ({                                                  \
    ASSERT_GE(result.sections.size(), 1u);            \
    ASSERT_GE(result.sections[0].entries.size(), 1u); \
    result.sections[0].entries[0].get();              \
  })

}  // namespace

void PrintTo(BloomResultEntry::Type type, std::ostream* os) {
  *os << ToString(type);
}

class BloomResultBuilderTest : public ::testing::Test {
 public:
  BloomResult BuildResult(const std::string& json_string) {
    base::JSONReader::ValueWithError json_value =
        base::JSONReader::ReadAndReturnValueWithError(json_string);

    CHECK(json_value.value) << "Invalid JSON.\n"
                            << "    Error: " << json_value.error_message << "\n"
                            << "    JSON: " << json_string;

    return BloomResultBuilder().Build(json_value.value.value());
  }
};

TEST_F(BloomResultBuilderTest, ShouldSetQuery) {
  BloomResult result = BuildResult(R"(
    {
      "query": { "text": "The Query" }
    }
  )");

  EXPECT_EQ("The Query", result.query);
}

TEST_F(BloomResultBuilderTest, ShouldAddSectionTitles) {
  BloomResult result = BuildResult(R"(
    {
      "contentGroups": [
        { "title": "The First Title" },
        { "title": "The Second Title" }
      ]
    }
  )");

  ASSERT_EQ(2u, result.sections.size());
  EXPECT_EQ("The First Title", result.sections[0].title);
  EXPECT_EQ("The Second Title", result.sections[1].title);
}

TEST_F(BloomResultBuilderTest, ShouldAddQuestionAndAnswerEntry) {
  BloomResult result = BuildResult(R"(
    {
      "contentGroups": [
        {
          "results": [
            {
              "type": "QA",
              "elements": [
                {
                  "text": { "markdown": "The question" }
                },
                {
                  "text": { "markdown": "The answer" }
                },
                {
                  "attribution": {
                    "title": "The source",
                    "faviconUrl": "https://fav.icon"
                  },
                  "url": "https://source.url"
                }
              ]
            }
          ]
        }
      ]
    }
  )");

  const BloomResultEntry& entry = *GET_FIRST_ENTRY(result);

  ASSERT_EQ(BloomResultEntry::Type::kQuestionAndAnswer, entry.type());

  const BloomQuestionAndAnswerEntry& qa_entry = *entry.AsQuestionAndAnswer();

  EXPECT_EQ("The question", qa_entry.question.text);
  EXPECT_EQ("The answer", qa_entry.answer.text);

  EXPECT_EQ("The source", qa_entry.source.text);
  EXPECT_EQ(GURL("https://fav.icon"), qa_entry.source.favicon_url);
  EXPECT_EQ(GURL("https://source.url"), qa_entry.source.url);
}

TEST_F(BloomResultBuilderTest, ShouldAddExplainer) {
  BloomResult result = BuildResult(R"(
    {
      "contentGroups": [
        {
          "results": [
            {
              "type": "EXPLAINER",
              "elements": [
                {
                  "title": { "text": "The title" }
                },
                {
                  "image":  {
                    "url": "https://image.url",
                    "description": "The image",
                    "width": 111,
                    "height": 222
                  }
                },
                {
                  "text": { "markdown": "The explanation" }
                },
                {
                  "text": { "markdown": "More explanation" }
                },
                {
                  "math": {
                    "latex": "The latex formula",
                    "accessibilityDescription": "A Math result"
                  }
                }
              ]
            }
          ]
        }
      ]
    }
  )");

  const BloomResultEntry& entry = *GET_FIRST_ENTRY(result);

  ASSERT_EQ(BloomResultEntry::Type::kExplainer, entry.type());

  const BloomExplainerEntry& explainer = *entry.AsExplainer();

  EXPECT_EQ("The title", explainer.title.text);

  EXPECT_EQ("The image", explainer.image.description);
  EXPECT_EQ(GURL("https://image.url"), explainer.image.url);
  EXPECT_EQ(111, explainer.image.width);
  EXPECT_EQ(222, explainer.image.height);

  const BloomElement& first_element = *GET_NTH_ELEMENT(explainer.elements, 0);
  ASSERT_EQ(BloomElement::Type::kText, first_element.type());
  EXPECT_EQ("The explanation", first_element.AsText()->text);

  const BloomElement& second_element = *GET_NTH_ELEMENT(explainer.elements, 1);
  ASSERT_EQ(BloomElement::Type::kText, second_element.type());
  EXPECT_EQ("More explanation", second_element.AsText()->text);

  const BloomElement& third_element = *GET_NTH_ELEMENT(explainer.elements, 2);
  ASSERT_EQ(BloomElement::Type::kMath, third_element.type());
  EXPECT_EQ("The latex formula", third_element.AsMath()->latex);
  EXPECT_EQ("A Math result", third_element.AsMath()->description);
}

TEST_F(BloomResultBuilderTest, ShouldAddVideo) {
  BloomResult result = BuildResult(R"(
    {
      "contentGroups": [
        {
          "results": [
            {
              "type": "VIDEO_PROCEDURAL",
              "elements": [
                {
                  "video":  {
                    "url": "https://video.url",
                    "title": "The video title",
                    "description": "The video description",
                    "thumbnailUrl": "https://thumbnail.url",
                    "videoId": "The video id",
                    "startTime": "The start time",
                    "duration": "The duration",
                    "channelTitle": "The channel title",
                    "numberOfLikes": 333,
                    "publishedTime": "The published time",
                    "numberOfViews": "666"
                  }
                },
                {
                  "attribution": {
                    "title": "The source",
                    "faviconUrl": "https://fav.icon"
                  },
                  "url": "https://source.url"
                }
              ]
            }
          ]
        }
      ]
    }
  )");

  const BloomResultEntry& entry = *GET_FIRST_ENTRY(result);

  ASSERT_EQ(BloomResultEntry::Type::kVideo, entry.type());

  const BloomVideoEntry& video = *entry.AsVideo();

  EXPECT_EQ(GURL("https://video.url"), video.video.url);
  EXPECT_EQ("The video title", video.video.title);
  EXPECT_EQ("The video description", video.video.description);
  EXPECT_EQ(GURL("https://thumbnail.url"), video.video.thumbnail_url);
  EXPECT_EQ("The video id", video.video.video_id);
  EXPECT_EQ("The start time", video.video.start_time);
  EXPECT_EQ("The duration", video.video.duration);
  EXPECT_EQ("The channel title", video.video.channel_title);
  EXPECT_EQ("333", video.video.number_of_likes);
  EXPECT_EQ("The published time", video.video.published_time);
  EXPECT_EQ("666", video.video.number_of_views);

  EXPECT_EQ("The source", video.source.text);
  EXPECT_EQ(GURL("https://fav.icon"), video.source.favicon_url);
  EXPECT_EQ(GURL("https://source.url"), video.source.url);
}

TEST_F(BloomResultBuilderTest, ShouldAddWebResult) {
  BloomResult result = BuildResult(R"(
    {
      "contentGroups": [
        {
          "results": [
            {
              "type": "WEB_RESULT",
              "elements": [
                {
                  "text": { "markdown": "The title" }
                },
                {
                  "text": { "markdown": "The snippet" }
                },
                {
                  "attribution": {
                    "title": "The source",
                    "faviconUrl": "https://fav.icon"
                  },
                  "url": "https://source.url"
                }
              ]
            }
          ]
        }
      ]
    }
  )");

  const BloomResultEntry& entry = *GET_FIRST_ENTRY(result);

  ASSERT_EQ(BloomResultEntry::Type::kWebResult, entry.type());

  const BloomWebResultEntry& web_result = *entry.AsWebResult();

  EXPECT_EQ("The title", web_result.title.text);
  EXPECT_EQ("The snippet", web_result.snippet.text);

  EXPECT_EQ("The source", web_result.source.text);
  EXPECT_EQ(GURL("https://fav.icon"), web_result.source.favicon_url);
  EXPECT_EQ(GURL("https://source.url"), web_result.source.url);
}

}  // namespace bloom
}  // namespace chromeos
