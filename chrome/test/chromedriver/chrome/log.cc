// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/log.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"

void Log::AddEntry(Level level, const std::string& message) {
  AddEntry(level, "", message);
}

void Log::AddEntry(Level level,
                   const std::string& source,
                   const std::string& message) {
  AddEntryTimestamped(base::Time::Now(), level, source, message);
}

bool Log::truncate_logged_params = true;
IsVLogOnFunc Log::is_vlog_on_func = nullptr;

namespace {

void TruncateString(std::string* data) {
  const size_t kMaxLength = 200;
  if (data->length() > kMaxLength) {
    data->resize(kMaxLength);
    data->replace(kMaxLength - 3, 3, "...");
  }
}

base::Value SmartDeepCopy(const base::Value* value) {
  const size_t kMaxChildren = 20;
  if (value->is_dict()) {
    base::Value::Dict dict_copy;
    for (auto [dict_key, dict_value] : value->GetDict()) {
      if (dict_copy.size() >= kMaxChildren - 1) {
        dict_copy.Set("~~~", "...");
        break;
      }
      dict_copy.Set(dict_key, SmartDeepCopy(&dict_value));
    }
    return base::Value(std::move(dict_copy));
  } else if (value->is_list()) {
    base::Value::List list_copy;
    for (const base::Value& child : value->GetList()) {
      if (list_copy.size() >= kMaxChildren - 1) {
        list_copy.Append("...");
        break;
      }
      list_copy.Append(SmartDeepCopy(&child));
    }
    return base::Value(std::move(list_copy));
  } else if (value->is_string()) {
    std::string data = value->GetString();
    TruncateString(&data);
    return base::Value(std::move(data));
  }
  return value->Clone();
}

}  // namespace

bool IsVLogOn(int vlog_level) {
  if (!Log::is_vlog_on_func)
    return false;
  return Log::is_vlog_on_func(vlog_level);
}

std::string PrettyPrintValue(const base::Value& value) {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
#if BUILDFLAG(IS_WIN)
  base::RemoveChars(json, "\r", &json);
#endif
  // Remove the trailing newline.
  if (json.length())
    json.resize(json.length() - 1);
  return json;
}

std::string FormatValueForDisplay(const base::Value& value) {
  if (Log::truncate_logged_params) {
    return PrettyPrintValue(SmartDeepCopy(&value));
  } else {
    return PrettyPrintValue(value);
  }
}

std::string FormatJsonForDisplay(const std::string& json) {
  std::optional<base::Value> value = base::JSONReader::Read(json);
  if (!value)
    value.emplace(json);
  return FormatValueForDisplay(*value);
}
