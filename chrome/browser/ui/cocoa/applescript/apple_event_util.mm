// Copyright (c) 2013 The Chromium Authors. All rights reserved.
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

namespace chrome {
namespace mac {

NSAppleEventDescriptor* ValueToAppleEventDescriptor(const base::Value* value) {
  NSAppleEventDescriptor* descriptor = nil;

  switch (value->type()) {
    case base::Value::Type::NONE:
      descriptor = [NSAppleEventDescriptor
          descriptorWithTypeCode:cMissingValue];
      break;

    case base::Value::Type::BOOLEAN: {
      descriptor =
          [NSAppleEventDescriptor descriptorWithBoolean:value->GetBool()];
      break;
    }

    case base::Value::Type::INTEGER: {
      descriptor = [NSAppleEventDescriptor descriptorWithInt32:value->GetInt()];
      break;
    }

    case base::Value::Type::DOUBLE: {
      double double_value = value->GetDouble();
      descriptor = [NSAppleEventDescriptor
          descriptorWithDescriptorType:typeIEEE64BitFloatingPoint
                                 bytes:&double_value
                                length:sizeof(double_value)];
      break;
    }

    case base::Value::Type::STRING: {
      descriptor = [NSAppleEventDescriptor
          descriptorWithString:base::SysUTF8ToNSString(value->GetString())];
      break;
    }

    case base::Value::Type::BINARY:
      NOTREACHED();
      break;

    case base::Value::Type::DICTIONARY: {
      const base::DictionaryValue* dictionary_value;
      value->GetAsDictionary(&dictionary_value);
      descriptor = [NSAppleEventDescriptor recordDescriptor];
      NSAppleEventDescriptor* userRecord = [NSAppleEventDescriptor
          listDescriptor];
      for (base::DictionaryValue::Iterator iter(*dictionary_value);
           !iter.IsAtEnd();
           iter.Advance()) {
        [userRecord insertDescriptor:[NSAppleEventDescriptor
            descriptorWithString:base::SysUTF8ToNSString(iter.key())]
                             atIndex:0];
        [userRecord insertDescriptor:ValueToAppleEventDescriptor(&iter.value())
                             atIndex:0];
      }
      // Description of what keyASUserRecordFields does.
      // http://lists.apple.com/archives/cocoa-dev/2009/Jul/msg01216.html
      [descriptor setDescriptor:userRecord forKeyword:keyASUserRecordFields];
      break;
    }

    case base::Value::Type::LIST: {
      const base::ListValue* list_value;
      value->GetAsList(&list_value);
      descriptor = [NSAppleEventDescriptor listDescriptor];
      for (size_t i = 0; i < list_value->GetSize(); ++i) {
        const base::Value* item;
        list_value->Get(i, &item);
        [descriptor insertDescriptor:ValueToAppleEventDescriptor(item)
                             atIndex:0];
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

}  // namespace mac
}  // namespace chrome
