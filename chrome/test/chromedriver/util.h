// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_UTIL_H_
#define CHROME_TEST_CHROMEDRIVER_UTIL_H_

#include <string>
#include <string_view>

#include "base/values.h"

namespace base {
class FilePath;
}

struct Session;
class Status;
class WebView;

// Generates a random, 32-character hexidecimal ID.
std::string GenerateId();

// Send a sequence of key strokes to the active Element in window.
Status SendKeysOnWindow(WebView* web_view,
                        const base::Value::List* key_list,
                        bool release_modifiers,
                        int* sticky_modifiers);

// Decodes the given base64-encoded string, after removing any newlines,
// which are required in some base64 standards. Returns true on success.
bool Base64Decode(const std::string& base64, std::string* bytes);

// Unzips the sole file contained in the given zip data |bytes| into
// |unzip_dir|. The zip data may be a normal zip archive or a single zip file
// entry. If the unzip successfully produced one file, returns true and sets
// |file| to the unzipped file.
// TODO(kkania): Remove the ability to parse single zip file entries when
// the current versions of all WebDriver clients send actual zip files.
Status UnzipSoleFile(const base::FilePath& unzip_dir,
                     const std::string& bytes,
                     base::FilePath* file);

// Calls BeforeCommand for each of |session|'s |CommandListener|s.
// If an error is encountered, will mark |session| for deletion and return.
Status NotifyCommandListenersBeforeCommand(Session* session,
                                           const std::string& command_name);

double ConvertCentimeterToInch(double centimeter);

// Functions to get an optional value of the given type from a dictionary.
// Each function has three different outcomes:
// * Value exists and is of right type:
//   returns true, *has_value = true, *out_value gets the actual value.
// * Value does not exist:
//   returns true, *has_value = false, *out_value is unchanged.
// * Value exists but is of wrong type (error condition):
//   returns false, *has_value undefined, *out_value is unchanged.
// In addition to provide a convenient way to fetch optional values that are
// common in W3C WebDriver spec, some of these functions also handles the
// differences in the definition of an integer:
// * base::Value uses a definition similar to C++, thus 2.0 is not an integer.
//   Also, integers are limited to 32-bit.
// * WebDriver spec (https://www.w3.org/TR/webdriver/#dfn-integer) defines
//   integer to be a number that is unchanged under the ToInteger operation,
//   thus 2.0 is an integer. Also, the spec sometimes uses "safe integer"
//   (https://www.w3.org/TR/webdriver/#dfn-maximum-safe-integer), whose
//   absolute value can occupy up to 53 bits.
bool GetOptionalBool(const base::Value::Dict& dict,
                     std::string_view path,
                     bool* out_value,
                     bool* has_value = nullptr);
bool GetOptionalInt(const base::Value::Dict& dict,
                    std::string_view path,
                    int* out_value,
                    bool* has_value = nullptr);
bool GetOptionalDouble(const base::Value::Dict& dict,
                       std::string_view path,
                       double* out_value,
                       bool* has_value = nullptr);
bool GetOptionalString(const base::Value::Dict& dict,
                       std::string_view path,
                       std::string* out_value,
                       bool* has_value = nullptr);
bool GetOptionalDictionary(const base::Value::Dict& dict,
                           std::string_view path,
                           const base::Value::Dict** out_value,
                           bool* has_value = nullptr);
bool GetOptionalList(const base::Value::Dict& dict,
                     std::string_view path,
                     const base::Value::List** out_value,
                     bool* has_value = nullptr);
// Handles "safe integer" mentioned in W3C spec,
// https://www.w3.org/TR/webdriver/#dfn-maximum-safe-integer.
bool GetOptionalSafeInt(const base::Value::Dict& dict,
                        std::string_view path,
                        int64_t* out_value,
                        bool* has_value = nullptr);

bool SetSafeInt(base::Value::Dict& dict,
                std::string_view path,
                int64_t in_value_64);

#endif  // CHROME_TEST_CHROMEDRIVER_UTIL_H_
