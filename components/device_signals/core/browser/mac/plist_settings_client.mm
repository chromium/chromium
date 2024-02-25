// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/mac/plist_settings_client.h"

#import <Foundation/Foundation.h>

#include <utility>

#import "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/device_signals/core/browser/settings_client.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/platform_utils.h"

namespace device_signals {

namespace {

// Max plist file size.
constexpr int kMaxFileSizeInMb = 500;

// Max size of the setting element.
constexpr size_t kMaxStringSizeInBytes = 1024;

// Parses the `data_obj` for the item at the given key `path`. Returns the
// object in the event of a successful parse, and nil otherwise.
id ParseArrays(id data_obj, NSString* path) {
  NSArray* indexes = [path componentsSeparatedByString:@"["];

  for (NSString* index_with_bracket in indexes) {
    NSArray* data_array = base::apple::ObjCCast<NSArray>(data_obj);
    if (!data_array) {
      return nil;
    }

    if (!index_with_bracket.length) {
      continue;
    }

    // Checking for the square brackets being closed. If they are not, then the
    // key path is malformed.
    NSRange range = [index_with_bracket rangeOfString:@"]"];
    if (range.location == NSNotFound ||
        range.location < index_with_bracket.length - 1) {
      return nil;
    }
    NSString* index_str = [index_with_bracket substringToIndex:range.location];

    // Validate that the index is a numeric. If the index is an alpha, issues
    // will occur during string to integer conversion.
    NSCharacterSet* numeric_set = NSCharacterSet.decimalDigitCharacterSet;
    if (![numeric_set
            isSupersetOfSet:[NSCharacterSet characterSetWithCharactersInString:
                                                index_str]]) {
      return nil;
    }

    NSUInteger index = base::checked_cast<NSUInteger>(index_str.integerValue);
    if (index > data_array.count) {
      return nil;
    }

    data_obj = data_array[index];
  }

  return data_obj;
}

// Parses the loaded plist `dict` for the setting item at `key_path`. Returns
// the setting object if it is found or nil otherwise.
id ParsePlist(NSDictionary* dict, NSString* key_path) {
  // Check if an array exists in the path, If not, the plist can be parsed
  // directly.
  NSRange test_range = [key_path rangeOfString:@"["];
  if (test_range.location == NSNotFound)
    return [dict valueForKeyPath:key_path];

  NSDictionary* current_obj = dict;
  for (NSString* sub_path in [key_path componentsSeparatedByString:@"."]) {
    NSRange range = [sub_path rangeOfString:@"["];
    if (range.location == NSNotFound) {
      current_obj = [current_obj valueForKey:sub_path];
    } else {
      current_obj =
          [current_obj valueForKey:[sub_path substringToIndex:range.location]];
      current_obj = ParseArrays(current_obj,
                                [sub_path substringFromIndex:range.location]);
    }
  }

  // This will occur if the key path is incorrect and does not actually point to
  // a setting item. At the end of a parse, the only remaining object should be
  // the single setting item.
  if ([current_obj isKindOfClass:[NSArray class]] && current_obj.count != 1) {
    return nil;
  }
  return current_obj;
}

// Using the setting `options`, this is responsible for loading and parsing the
// plists for setting values specified for the setting options. A collection of
// the setting items are returned to the caller.
std::vector<SettingsItem> GetSettingItems(
    const std::vector<GetSettingsOptions>& options) {
  std::vector<SettingsItem> items;

  for (GetSettingsOptions option : options) {
    SettingsItem item;
    item.key = option.key;
    item.path = option.path;

    // Load Plist file into memory.
    base::FilePath resolved_path;
    if (!ResolvePath(base::FilePath(option.path), &resolved_path)) {
      item.presence = PresenceValue::kNotFound;
      items.push_back(item);
      continue;
    }

    int64_t plist_file_size = 0;
    if (!base::GetFileSize(resolved_path, &plist_file_size) ||
        plist_file_size > (kMaxFileSizeInMb << 20)) {
      item.presence = PresenceValue::kNotFound;
      items.push_back(item);
      continue;
    }

    NSError* error = nil;
    NSURL* url = base::apple::FilePathToNSURL(resolved_path);
    NSDictionary* plist_dict =
        [[NSDictionary alloc] initWithContentsOfURL:url error:&error];
    if (error && error.code == NSFileReadNoPermissionError) {
      item.presence = PresenceValue::kAccessDenied;
      items.push_back(item);
      continue;
    }

    if (!plist_dict) {
      item.presence = PresenceValue::kNotFound;
      items.push_back(item);
    }

    id value_ptr = ParsePlist(plist_dict, base::SysUTF8ToNSString(option.key));
    if (!value_ptr) {
      item.presence = PresenceValue::kNotFound;
      items.push_back(item);
      continue;
    }

    item.presence = PresenceValue::kFound;
    if (!option.get_value) {
      items.push_back(item);
      continue;
    }

    if (NSString* setting_str = base::apple::ObjCCast<NSString>(value_ptr)) {
      if (setting_str.length <= kMaxStringSizeInBytes) {
        std::string setting_json_string;
        base::JSONWriter::Write(
            base::Value(base::SysNSStringToUTF8(setting_str)),
            &setting_json_string);
        item.setting_json_value = setting_json_string;
      }
    } else if (NSNumber* value_num =
                   base::apple::ObjCCast<NSNumber>(value_ptr)) {
      // Differentiating between integer and float types.
      const char* value_type = value_num.objCType;
      if (strcmp(value_type, "d") == 0 || strcmp(value_type, "f") == 0) {
        double setting_num = value_num.doubleValue;
        item.setting_json_value = base::StringPrintf("%f", setting_num);
      } else {
        int setting_num = value_num.integerValue;
        item.setting_json_value = base::StringPrintf("%d", setting_num);
      }
    }
    items.push_back(item);
  }
  return items;
}

}  // namespace

PlistSettingsClient::PlistSettingsClient() = default;

PlistSettingsClient::~PlistSettingsClient() = default;

void PlistSettingsClient::GetSettings(
    const std::vector<GetSettingsOptions>& options,
    GetSettingsSignalsCallback callback) {
  std::vector<SettingsItem> items;

  // Used to ensure that this function is being called on the main thread.
  SEQUENCE_CHECKER(sequence_checker_);

  if (options.empty()) {
    std::move(callback).Run(std::vector<SettingsItem>());
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&GetSettingItems, std::move(options)),
      std::move(callback));
}

}  // namespace device_signals
