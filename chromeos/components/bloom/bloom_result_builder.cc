// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/bloom_result_builder.h"

#include <memory>
#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace chromeos {
namespace bloom {

namespace {

std::string ToString(const base::Value& json) {
  std::string result;
  base::JSONWriter::WriteWithOptions(
      json, base::JSONWriter::OPTIONS_PRETTY_PRINT, &result);
  return result;
}

}  // namespace

BloomResultBuilder::BloomResultBuilder() = default;
BloomResultBuilder::~BloomResultBuilder() = default;

BloomResult BloomResultBuilder::Build(const base::Value& root) {
  BloomResult result;

  result.query = GetQuery(root);

  for (const auto& section : FindList("contentGroups", root).GetList())
    result.sections.push_back(BuildSection(section));

  return result;
}

std::string BloomResultBuilder::GetQuery(const base::Value& root) {
  return FindString("query.text", root);
}

BloomResultSection BloomResultBuilder::BuildSection(
    const base::Value& section) {
  BloomResultSection result;

  result.title = FindString("title", section);

  for (const auto& entry : FindList("results", section).GetList()) {
    const std::string& type = FindString("type", entry);

    if (type == "QA")
      result.entries.push_back(BuildQuestionAndAnswer(entry));
    else if (type == "EXPLAINER")
      result.entries.push_back(BuildExplainer(entry));
    else if (type == "VIDEO_PROCEDURAL")
      result.entries.push_back(BuildVideo(entry));
    else if (type == "WEB_RESULT")
      result.entries.push_back(BuildWebResult(entry));
    else
      LOG(WARNING) << "Unknown section type '" << type << "'";
  }

  return result;
}

std::unique_ptr<BloomQuestionAndAnswerEntry>
BloomResultBuilder::BuildQuestionAndAnswer(const base::Value& entry) {
  auto result = std::make_unique<BloomQuestionAndAnswerEntry>();

  result->question = BuildTextElement(FindNthElement(entry, 0));
  result->answer = BuildTextElement(FindNthElement(entry, 1));
  result->source = BuildSourceElement(FindNthElement(entry, 2));

  return result;
}

std::unique_ptr<BloomExplainerEntry> BloomResultBuilder::BuildExplainer(
    const base::Value& entry) {
  auto result = std::make_unique<BloomExplainerEntry>();

  result->title = BuildTitleElement(FindNthElement(entry, 0));
  result->image = BuildImageElement(FindNthElement(entry, 1));

  auto elements = FindList("elements", entry).GetList();
  // |index| starts at 2 because we've already consumed the title and the
  // image.
  for (size_t index = 2; index < elements.size(); index++) {
    auto el = BuildElement(elements[index]);
    if (el)
      result->elements.push_back(std::move(el));
  }

  return result;
}

std::unique_ptr<BloomVideoEntry> BloomResultBuilder::BuildVideo(
    const base::Value& entry) {
  auto result = std::make_unique<BloomVideoEntry>();

  result->video = BuildVideoElement(FindNthElement(entry, 0));
  result->source = BuildSourceElement(FindNthElement(entry, 1));

  return result;
}

std::unique_ptr<BloomWebResultEntry> BloomResultBuilder::BuildWebResult(
    const base::Value& entry) {
  auto result = std::make_unique<BloomWebResultEntry>();

  result->title = BuildTextElement(FindNthElement(entry, 0));
  result->snippet = BuildTextElement(FindNthElement(entry, 1));
  result->source = BuildSourceElement(FindNthElement(entry, 2));

  return result;
}

std::unique_ptr<BloomElement> BloomResultBuilder::BuildElement(
    const base::Value& element) {
  if (HasKey("text", element))
    return std::make_unique<BloomTextElement>(BuildTextElement(element));
  if (HasKey("image", element))
    return std::make_unique<BloomImageElement>(BuildImageElement(element));
  if (HasKey("math", element))
    return std::make_unique<BloomMathElement>(BuildMathElement(element));
  if (HasKey("title", element))
    return std::make_unique<BloomTitleElement>(BuildTitleElement(element));
  if (HasKey("attribution", element))
    return std::make_unique<BloomSourceElement>(BuildSourceElement(element));
  if (HasKey("video", element))
    return std::make_unique<BloomVideoElement>(BuildVideoElement(element));

  LOG(WARNING) << "Received element of unknown type. " << ToString(element);
  return nullptr;
}

BloomImageElement BloomResultBuilder::BuildImageElement(
    const base::Value& element) {
  BloomImageElement result;
  result.description = FindString("image.description", element);
  result.url = GURL(FindString("image.url", element));
  result.width = FindInt("image.width", element);
  result.height = FindInt("image.height", element);
  return result;
}

BloomMathElement BloomResultBuilder::BuildMathElement(
    const base::Value& element) {
  BloomMathElement result;
  result.description = FindString("math.accessibilityDescription", element);
  result.latex = FindString("math.latex", element);
  return result;
}

BloomSourceElement BloomResultBuilder::BuildSourceElement(
    const base::Value& element) {
  BloomSourceElement result;
  result.text = FindString("attribution.title", element);
  result.favicon_url = GURL(FindString("attribution.faviconUrl", element));
  result.url = GURL(FindString("url", element));
  return result;
}

BloomTextElement BloomResultBuilder::BuildTextElement(
    const base::Value& element) {
  BloomTextElement result;
  result.text = FindString("text.markdown", element);
  return result;
}

BloomTitleElement BloomResultBuilder::BuildTitleElement(
    const base::Value& element) {
  BloomTitleElement result;
  result.text = FindString("title.text", element);
  return result;
}

BloomVideoElement BloomResultBuilder::BuildVideoElement(
    const base::Value& element) {
  BloomVideoElement result;
  result.url = GURL(FindString("video.url", element));
  result.title = FindString("video.title", element);
  result.description = FindString("video.description", element);
  result.thumbnail_url = GURL(FindString("video.thumbnailUrl", element));
  result.video_id = FindString("video.videoId", element);
  result.start_time = FindString("video.startTime", element);
  result.duration = FindString("video.duration", element);
  result.channel_title = FindString("video.channelTitle", element);
  result.number_of_likes =
      base::StringPrintf("%i", FindInt("video.numberOfLikes", element));
  result.published_time = FindString("video.publishedTime", element);
  result.number_of_views = FindString("video.numberOfViews", element);
  return result;
}

bool BloomResultBuilder::HasKey(base::StringPiece key,
                                const base::Value& parent) const {
  return parent.FindKey(key) != nullptr;
}

const base::Value& BloomResultBuilder::FindList(
    base::StringPiece key_path,
    const base::Value& parent) const {
  const base::Value* value = parent.FindListPath(key_path);
  if (value)
    return *value;

  return default_list_;
}

int BloomResultBuilder::FindInt(base::StringPiece key_path,
                                const base::Value& parent) const {
  base::Optional<int> value = parent.FindIntPath(key_path);
  return value.value_or(0);
}

const std::string& BloomResultBuilder::FindString(
    base::StringPiece key_path,
    const base::Value& parent) const {
  const std::string* value = parent.FindStringPath(key_path);
  if (value)
    return *value;

  return default_string_;
}

const base::Value& BloomResultBuilder::FindElement(const ConstListView& list,
                                                   size_t index) const {
  if (list.size() > index)
    return list[index];

  // Return a dictionary because element is a dictionary.
  return default_dict_;
}

const base::Value& BloomResultBuilder::FindNthElement(const base::Value& entry,
                                                      size_t index) const {
  auto elements = FindList("elements", entry).GetList();
  return FindElement(elements, index);
}

}  // namespace bloom
}  // namespace chromeos
