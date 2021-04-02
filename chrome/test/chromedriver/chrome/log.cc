// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
IsVLogOnFunc Log::is_vlog_on_func = NULL;

namespace {

void TruncateString(std::string* data) {
  const size_t kMaxLength = 200;
  if (data->length() > kMaxLength) {
    data->resize(kMaxLength);
    data->replace(kMaxLength - 3, 3, "...");
  }
}

std::unique_ptr<base::Value> SmartDeepCopy(const base::Value* value) {
  const size_t kMaxChildren = 20;
  const base::ListValue* list = NULL;
  const base::DictionaryValue* dict = NULL;
  std::string data;
  if (value->GetAsDictionary(&dict)) {
    std::unique_ptr<base::DictionaryValue> dict_copy(
        new base::DictionaryValue());
    for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd();
         it.Advance()) {
      if (dict_copy->size() >= kMaxChildren - 1) {
        dict_copy->SetKey("~~~", base::Value("..."));
        break;
      }
      const base::Value* child = NULL;
      dict->GetWithoutPathExpansion(it.key(), &child);
      dict_copy->SetWithoutPathExpansion(it.key(), SmartDeepCopy(child));
    }
    return std::move(dict_copy);
  } else if (value->GetAsList(&list)) {
    std::unique_ptr<base::ListValue> list_copy(new base::ListValue());
    for (size_t i = 0; i < list->GetSize(); ++i) {
      const base::Value* child = NULL;
      if (!list->Get(i, &child))
        continue;
      if (list_copy->GetSize() >= kMaxChildren - 1) {
        list_copy->AppendString("...");
        break;
      }
      list_copy->Append(SmartDeepCopy(child));
    }
    return std::move(list_copy);
  } else if (value->GetAsString(&data)) {
    TruncateString(&data);
    return std::make_unique<base::Value>(data);
  }
  return std::unique_ptr<base::Value>(value->DeepCopy());
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
#if defined(OS_WIN)
  base::RemoveChars(json, "\r", &json);
#endif
  // Remove the trailing newline.
  if (json.length())
    json.resize(json.length() - 1);
  return json;
}

std::string FormatValueForDisplay(const base::Value& value) {
  return PrettyPrintValue(Log::truncate_logged_params ? *SmartDeepCopy(&value)
                                                      : value);
}

std::string FormatJsonForDisplay(const std::string& json) {
  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(json);
  if (!value)
    value = std::make_unique<base::Value>(json);
  return FormatValueForDisplay(*value);
}
