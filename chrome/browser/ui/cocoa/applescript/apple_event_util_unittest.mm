// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/apple_event_util.h"

#include <CoreServices/CoreServices.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/json/json_reader.h"
#include "base/mac/scoped_aedesc.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "testing/gtest_mac.h"

namespace {

std::string FourCharToString(FourCharCode code) {
  std::string result(6, '\'');
  result[1] = (code >> 24) & 0xFF;
  result[2] = (code >> 16) & 0xFF;
  result[3] = (code >> 8) & 0xFF;
  result[4] = code & 0xFF;
  return result;
}

// This function returns a string description of the contents of the given
// AEDesc in the AEGizmos/AEPrintDescToHandle format.
//
// The -[NSAppleEventDescriptor description] method does this too, but the
// problem is that it is implemented using AEPrintDescToHandle, which is both
// flaky <http://crbug.com/239807> and constantly buffer-overflows and fails
// ASan tests <http://crbug.com/177177>.
//
// This function does not handle every type that AEPrintDescToHandle does, but
// it covers the cases hit by the unit test, and fails in an obvious way should
// the Apple Event code change.
std::string AEDescToString(const AEDesc* aedesc) {
  switch (aedesc->descriptorType) {
    case typeType: {
      FourCharCode code;
      OSErr err = AEGetDescData(aedesc, &code, sizeof(code));
      if (err != noErr) {
        NOTREACHED();
        return std::string();
      }

      return FourCharToString(code);
    }
    case typeSInt16:
    case typeSInt32:
    case typeSInt64: {
      base::mac::ScopedAEDesc<> wide_desc;
      OSErr err = AECoerceDesc(aedesc, typeSInt64, wide_desc.OutPointer());
      if (err != noErr) {
        NOTREACHED();
        return std::string();
      }

      int64_t value;
      err = AEGetDescData(wide_desc, &value, sizeof(value));
      if (err != noErr) {
        NOTREACHED();
        return std::string();
      }

      return base::NumberToString(value);
    }
    case typeIEEE32BitFloatingPoint:
    case typeIEEE64BitFloatingPoint: {
      base::mac::ScopedAEDesc<> wide_desc;
      OSErr err = AECoerceDesc(aedesc, typeIEEE64BitFloatingPoint,
                               wide_desc.OutPointer());
      if (err != noErr) {
        NOTREACHED();
        return std::string();
      }

      double value;
      err = AEGetDescData(wide_desc, &value, sizeof(value));
      if (err != noErr) {
        NOTREACHED();
        return std::string();
      }

      return base::NumberToString(value);
    }
    // Text formats look like:
    //  'utxt'("string here")
    case typeUnicodeText: {
      size_t byte_length = AEGetDescDataSize(aedesc);
      std::vector<base::char16> data_vector(byte_length / sizeof(base::char16));
      OSErr err = AEGetDescData(aedesc, data_vector.data(), byte_length);
      if (err != noErr) {
        NOTREACHED();
        return std::string();
      }
      return FourCharToString(typeUnicodeText) + "(\"" +
             base::UTF16ToUTF8(
                 base::string16(data_vector.begin(), data_vector.end())) +
             "\")";
    }
    // Lists look like:
    //  [ item1, item2, item3 ]
    // and records look like:
    //  { 'key1':value1, 'key2': value2 }
    case typeAEList:
    case typeAERecord: {
      bool is_record = aedesc->descriptorType == typeAERecord;

      std::string result = is_record ? "{ " : "[ ";
      long list_count;
      OSErr err = AECountItems(aedesc, &list_count);
      if (err != noErr) {
        NOTREACHED();
        return std::string();
      }
      for (long i = 0; i < list_count; ++i) {
        AEKeyword key;
        base::mac::ScopedAEDesc<> value_desc;
        err = AEGetNthDesc(aedesc, i + 1 /* 1-based! */, typeWildCard, &key,
                           value_desc.OutPointer());
        if (err != noErr) {
          NOTREACHED();
          return std::string();
        }

        if (is_record) {
          result += FourCharToString(key);
          result += ":";
        }

        result += AEDescToString(value_desc);

        if (i < list_count - 1)
          result += ", ";
      }

      result += is_record ? " }" : " ]";
      return result;
    }
    default: {
      NOTREACHED() << "unexpected descriptor type "
                   << FourCharToString(aedesc->descriptorType);
      return std::string();
    }
  }
}

class AppleEventUtilTest : public CocoaTest { };

struct TestCase {
  const char* json_input;
  const char* expected_aedesc_dump;
  DescType expected_aedesc_type;
};

TEST_F(AppleEventUtilTest, ValueToAppleEventDescriptor) {
  const struct TestCase cases[] = {
    { "null",         "'msng'",             typeType },
    { "-1000",        "-1000",              typeSInt32 },
    { "0",            "0",                  typeSInt32 },
    { "1000",         "1000",               typeSInt32 },
    { "-1e100",       "-1e+100",            typeIEEE64BitFloatingPoint },
    { "0.0",          "0",                  typeIEEE64BitFloatingPoint },
    { "1e100",        "1e+100",             typeIEEE64BitFloatingPoint },
    { "\"\"",         "'utxt'(\"\")",       typeUnicodeText },
    { "\"string\"",   "'utxt'(\"string\")", typeUnicodeText },
    { "{}",           "{ 'usrf':[  ] }",    typeAERecord },
    { "[]",           "[  ]",               typeAEList },
    { "{\"Image\": {"
      "\"Width\": 800,"
      "\"Height\": 600,"
      "\"Title\": \"View from 15th Floor\","
      "\"Thumbnail\": {"
      "\"Url\": \"http://www.example.com/image/481989943\","
      "\"Height\": 125,"
      "\"Width\": \"100\""
      "},"
      "\"IDs\": [116, 943, 234, 38793]"
      "}"
      "}",
      "{ 'usrf':[ 'utxt'(\"Image\"), { 'usrf':[ 'utxt'(\"Height\"), 600, "
      "'utxt'(\"IDs\"), [ 116, 943, 234, 38793 ], 'utxt'(\"Thumbnail\"), "
      "{ 'usrf':[ 'utxt'(\"Height\"), 125, 'utxt'(\"Url\"), "
      "'utxt'(\"http://www.example.com/image/481989943\"), 'utxt'(\"Width\"), "
      "'utxt'(\"100\") ] }, 'utxt'(\"Title\"), "
      "'utxt'(\"View from 15th Floor\"), 'utxt'(\"Width\"), 800 ] } ] }",
      typeAERecord },
    { "["
      "{"
      "\"precision\": \"zip\","
      "\"Latitude\": 37.7668,"
      "\"Longitude\": -122.3959,"
      "\"Address\": \"\","
      "\"City\": \"SAN FRANCISCO\","
      "\"State\": \"CA\","
      "\"Zip\": \"94107\","
      "\"Country\": \"US\""
      "},"
      "{"
      "\"precision\": \"zip\","
      "\"Latitude\": 37.371991,"
      "\"Longitude\": -122.026020,"
      "\"Address\": \"\","
      "\"City\": \"SUNNYVALE\","
      "\"State\": \"CA\","
      "\"Zip\": \"94085\","
      "\"Country\": \"US\""
      "}"
      "]",
      "[ { 'usrf':[ 'utxt'(\"Address\"), 'utxt'(\"\"), 'utxt'(\"City\"), "
      "'utxt'(\"SAN FRANCISCO\"), 'utxt'(\"Country\"), 'utxt'(\"US\"), "
      "'utxt'(\"Latitude\"), 37.7668, 'utxt'(\"Longitude\"), -122.3959, "
      "'utxt'(\"State\"), 'utxt'(\"CA\"), 'utxt'(\"Zip\"), 'utxt'(\"94107\"), "
      "'utxt'(\"precision\"), 'utxt'(\"zip\") ] }, { 'usrf':[ "
      "'utxt'(\"Address\"), 'utxt'(\"\"), 'utxt'(\"City\"), "
      "'utxt'(\"SUNNYVALE\"), 'utxt'(\"Country\"), 'utxt'(\"US\"), "
      "'utxt'(\"Latitude\"), 37.371991, 'utxt'(\"Longitude\"), -122.02602, "
      "'utxt'(\"State\"), 'utxt'(\"CA\"), 'utxt'(\"Zip\"), 'utxt'(\"94085\"), "
      "'utxt'(\"precision\"), 'utxt'(\"zip\") ] } ]",
      typeAEList },
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    std::unique_ptr<base::Value> value =
        base::JSONReader::ReadDeprecated(cases[i].json_input);
    NSAppleEventDescriptor* descriptor =
        chrome::mac::ValueToAppleEventDescriptor(value.get());

    EXPECT_EQ(cases[i].expected_aedesc_dump,
              AEDescToString([descriptor aeDesc]))
        << "i: " << i;
    EXPECT_EQ(cases[i].expected_aedesc_type,
              [descriptor descriptorType]) << "i: " << i;
  }

  // Test boolean values separately because boolean NSAppleEventDescriptors
  // return different values across different system versions when their
  // -description method is called.

  const bool all_bools[] = { true, false };
  for (bool b : all_bools) {
    base::Value value(b);
    NSAppleEventDescriptor* descriptor =
        chrome::mac::ValueToAppleEventDescriptor(&value);

    EXPECT_EQ(typeBoolean, [descriptor descriptorType]);
    EXPECT_EQ(b, [descriptor booleanValue]);
  }
}

}  // namespace
