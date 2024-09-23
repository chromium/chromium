// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/apple_event_util.h"

#import <Carbon/Carbon.h>
#include <stddef.h>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chrome::mac {

namespace {

void AppendValueToListDescriptor(NSAppleEventDescriptor* list,
                                 const base::Value& value) {
  // Note that index 0 means "append to end of list"; see the docs for
  // -[NSAppleEventDescriptor insertDescriptor:atIndex:] and ultimately for
  // AEPutDesc().
  [list insertDescriptor:ValueToAppleEventDescriptor(value) atIndex:0];
}

NSAppleEventDescriptor* RecordDescriptorForKeyValuePairs(
    NSAppleEventDescriptor* list) {
  // /!\ Extreme undocumented subtlety ahead /!\
  //
  // AERecords have the restriction that their keys are `FourChar` codes
  // (e.g. keyAS* values from ASRegistry.h). This does not mesh well with
  // Cocoa, where keys in `NSDictionary` objects are full objects (though
  // usually strings).
  //
  // Therefore, the AppleScript team developed a special way to wrap an
  // NSDictionary in an AERecord. NSDictionary objects are wrapped by
  // creating an an AERecord has a single key of keyASUserRecordFields
  // that has a value of an AEList. That list must have alternating
  // keys (required to be string type) and values.
  //
  // As Value dictionaries have string-typed keys, they are perfect for
  // this wrapping, so wrap them in this manner too. See
  //   https://lists.apple.com/archives/cocoa-dev/2009/Jul/msg01216.html
  // for more details.
  NSAppleEventDescriptor* descriptor =
      [NSAppleEventDescriptor recordDescriptor];
  [descriptor setDescriptor:list forKeyword:keyASUserRecordFields];
  return descriptor;
}

}  // namespace

NSAppleEventDescriptor* ValueToAppleEventDescriptor(const base::Value& value) {
  NSAppleEventDescriptor* descriptor = nil;

  switch (value.type()) {
    case base::Value::Type::NONE:
      descriptor = [NSAppleEventDescriptor
          descriptorWithTypeCode:cMissingValue];
      break;

    case base::Value::Type::BOOLEAN: {
      descriptor =
          [NSAppleEventDescriptor descriptorWithBoolean:value.GetBool()];
      break;
    }

    case base::Value::Type::INTEGER: {
      descriptor = [NSAppleEventDescriptor descriptorWithInt32:value.GetInt()];
      break;
    }

    case base::Value::Type::DOUBLE: {
      double double_value = value.GetDouble();
      descriptor = [NSAppleEventDescriptor
          descriptorWithDescriptorType:typeIEEE64BitFloatingPoint
                                 bytes:&double_value
                                length:sizeof(double_value)];
      break;
    }

    case base::Value::Type::STRING: {
      descriptor = [NSAppleEventDescriptor
          descriptorWithString:base::SysUTF8ToNSString(value.GetString())];
      break;
    }

    case base::Value::Type::BINARY:
      NOTREACHED_IN_MIGRATION();
      break;

    case base::Value::Type::DICT: {
      NSAppleEventDescriptor* keyValuePairs =
          [NSAppleEventDescriptor listDescriptor];
      for (auto iter : value.GetDict()) {
        AppendValueToListDescriptor(keyValuePairs, base::Value(iter.first));
        AppendValueToListDescriptor(keyValuePairs, iter.second);
      }
      descriptor = RecordDescriptorForKeyValuePairs(keyValuePairs);
      break;
    }

    case base::Value::Type::LIST: {
      descriptor = [NSAppleEventDescriptor listDescriptor];
      for (const auto& item : value.GetList()) {
        AppendValueToListDescriptor(descriptor, item);
      }
      break;
    }
  }

  return descriptor;
}

bool IsJavaScriptEnabledForProfile(Profile* profile) {
  DCHECK(profile);
  PrefService* prefs = profile->GetPrefs();
  return prefs->GetBoolean(prefs::kAllowJavascriptAppleEvents);
}

}  // namespace chrome::mac
