// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_features_parser.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"

namespace arc {

namespace {

constexpr const base::FilePath::CharType kArcFeaturesJsonFile[] =
    FILE_PATH_LITERAL("/etc/arc/features.json");

base::Optional<ArcFeatures> ParseFeaturesJson(base::StringPiece input_json) {
  ArcFeatures arc_features;

  int error_code;
  std::string error_msg;
  std::unique_ptr<base::Value> json_value =
      base::JSONReader::ReadAndReturnErrorDeprecated(
          input_json, base::JSON_PARSE_RFC, &error_code, &error_msg);
  if (!json_value || !json_value->is_dict()) {
    LOG(ERROR) << "Error parsing feature JSON: " << error_msg;
    return base::nullopt;
  }

  // Parse each item under features.
  const base::Value* feature_list =
      json_value->FindKeyOfType("features", base::Value::Type::LIST);
  if (!feature_list) {
    LOG(ERROR) << "No feature list in JSON.";
    return base::nullopt;
  }
  for (auto& feature_item : feature_list->GetList()) {
    const base::Value* feature_name =
        feature_item.FindKeyOfType("name", base::Value::Type::STRING);
    const base::Value* feature_version =
        feature_item.FindKeyOfType("version", base::Value::Type::INTEGER);
    if (!feature_name || feature_name->GetString().empty()) {
      LOG(ERROR) << "Missing name in the feature.";
      return base::nullopt;
    }
    if (!feature_version) {
      LOG(ERROR) << "Missing version in the feature.";
      return base::nullopt;
    }
    arc_features.feature_map.emplace(feature_name->GetString(),
                                     feature_version->GetInt());
  }

  // Parse each item under unavailable_features.
  const base::Value* unavailable_feature_list = json_value->FindKeyOfType(
      "unavailable_features", base::Value::Type::LIST);
  if (!unavailable_feature_list) {
    LOG(ERROR) << "No unavailable feature list in JSON.";
    return base::nullopt;
  }
  for (auto& feature_item : unavailable_feature_list->GetList()) {
    if (!feature_item.is_string()) {
      LOG(ERROR) << "Item in the unavailable feature list is not a string.";
      return base::nullopt;
    }

    if (feature_item.GetString().empty()) {
      LOG(ERROR) << "Missing name in the feature.";
      return base::nullopt;
    }
    arc_features.unavailable_features.emplace_back(feature_item.GetString());
  }

  // Parse each item under properties.
  const base::Value* properties =
      json_value->FindKeyOfType("properties", base::Value::Type::DICTIONARY);
  if (!properties) {
    LOG(ERROR) << "No properties in JSON.";
    return base::nullopt;
  }
  for (const auto& item : properties->DictItems()) {
    if (!item.second.is_string()) {
      LOG(ERROR) << "Item in the properties mapping is not a string.";
      return base::nullopt;
    }

    arc_features.build_props.emplace(item.first, item.second.GetString());
  }

  // Parse the Play Store version
  const base::Value* play_version = json_value->FindKeyOfType(
      "play_store_version", base::Value::Type::STRING);
  if (!play_version) {
    LOG(ERROR) << "No Play Store version in JSON.";
    return base::nullopt;
  }
  arc_features.play_store_version = play_version->GetString();

  return arc_features;
}

base::Optional<ArcFeatures> ReadOnFileThread(const base::FilePath& file_path) {
  DCHECK(!file_path.empty());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::string input_json;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    if (!base::ReadFileToString(file_path, &input_json)) {
      PLOG(ERROR) << "Cannot read file " << file_path.value()
                  << " into string.";
      return base::nullopt;
    }
  }

  if (input_json.empty()) {
    LOG(ERROR) << "Input JSON is empty in file " << file_path.value();
    return base::nullopt;
  }

  return ParseFeaturesJson(input_json);
}

}  // namespace

ArcFeatures::ArcFeatures() = default;
ArcFeatures::ArcFeatures(ArcFeatures&& other) = default;
ArcFeatures::~ArcFeatures() = default;
ArcFeatures& ArcFeatures::operator=(ArcFeatures&& other) = default;

void ArcFeaturesParser::GetArcFeatures(
    base::OnceCallback<void(base::Optional<ArcFeatures>)> callback) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReadOnFileThread, base::FilePath(kArcFeaturesJsonFile)),
      std::move(callback));
}

base::Optional<ArcFeatures> ArcFeaturesParser::ParseFeaturesJsonForTesting(
    base::StringPiece input_json) {
  return ParseFeaturesJson(input_json);
}

}  // namespace arc
