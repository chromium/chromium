// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_HTTP_INTERNET_UNITTEST_HELPERS_H_
#define CHROME_CHROME_CLEANER_HTTP_INTERNET_UNITTEST_HELPERS_H_

#include <map>
#include <string>

namespace chrome_cleaner {

// Verifies that the supplied multipart MIME message body is plausibly
// formatted. Adds non-fatal GTest failures if verification fails.
// @param boundary The boundary specified in the Content-Type header that
//     accompanied the body.
// @param parameters The parameters that are expected to be encoded in the body.
// @param file The file contents that are expdected to be encoded in the body.
// @param file_part_name The name expected to be assigned to the file parameter.
void ExpectMultipartMimeMessageIsPlausible(
    const std::wstring& boundary,
    const std::map<std::wstring, std::wstring>& parameters,
    const std::string& file,
    const std::string& file_part_name,
    const std::string& body);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_HTTP_INTERNET_UNITTEST_HELPERS_H_
