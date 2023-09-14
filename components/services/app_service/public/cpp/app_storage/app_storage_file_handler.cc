// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_storage/app_storage_file_handler.h"

#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"

namespace apps {

namespace {

constexpr char kAppServiceDirName[] = "app_service";
constexpr char kAppStorageFileName[] = "AppStorage";

constexpr char kTypeKey[] = "type";
constexpr char kNameKey[] = "name";

absl::optional<std::string> GetStringValueFromDict(
    const base::Value::Dict& dict,
    base::StringPiece key_name) {
  const std::string* value = dict.FindString(key_name);
  return value ? absl::optional<std::string>(*value) : absl::nullopt;
}

}  // namespace

AppStorageFileHandler::AppStorageFileHandler(const base::FilePath& base_path)
    : RefCountedDeleteOnSequence(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      file_path_(base_path.AppendASCII(kAppServiceDirName)
                     .AppendASCII(kAppStorageFileName)) {}

std::vector<AppPtr> AppStorageFileHandler::ReadFromFile() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!base::PathExists(file_path_)) {
    return std::vector<AppPtr>();
  }

  std::string app_info_data;
  if (!base::ReadFileToString(file_path_, &app_info_data) ||
      app_info_data.empty()) {
    return std::vector<AppPtr>();
  }

  base::JSONReader::Result app_info_value =
      base::JSONReader::ReadAndReturnValueWithError(app_info_data);
  if (!app_info_value.has_value()) {
    LOG(ERROR)
        << "Fail to deserialize json value from string with error message: "
        << app_info_value.error().message << ", in line "
        << app_info_value.error().line << ", column "
        << app_info_value.error().column;
    return std::vector<AppPtr>();
  }

  return ConvertValueToApps(std::move(*app_info_value));
}

AppStorageFileHandler::~AppStorageFileHandler() = default;

std::vector<AppPtr> AppStorageFileHandler::ConvertValueToApps(
    base::Value app_info_value) {
  std::vector<AppPtr> apps;

  base::Value::Dict* dict = app_info_value.GetIfDict();
  if (!dict) {
    LOG(ERROR) << "Fail to parse the app info value. "
               << "Cannot find the app info dict.";
    return apps;
  }

  for (auto [app_id, app_value] : *dict) {
    base::Value::Dict* value = app_value.GetIfDict();

    if (!value) {
      LOG(ERROR) << "Fail to parse the app info value. "
                 << "Cannot find the value for the app:" << app_id;
      continue;
    }

    auto app_type = value->FindInt(kTypeKey);
    if (!app_type.has_value() ||
        app_type.value() < static_cast<int>(AppType::kUnknown) ||
        app_type.value() > static_cast<int>(AppType::kMaxValue)) {
      LOG(ERROR) << "Fail to parse the app info value. "
                 << "Cannot find the app type for the app:" << app_id;
      continue;
    }

    auto app =
        std::make_unique<App>(static_cast<AppType>(app_type.value()), app_id);
    app->name = GetStringValueFromDict(*value, kNameKey);

    // TODO(crbug.com/1385932): Add other files in the App structure.
    apps.push_back(std::move(app));
  }
  return apps;
}

}  // namespace apps
