// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATABASE_UTILS_URL_CONVERTER_H_
#define COMPONENTS_DATABASE_UTILS_URL_CONVERTER_H_

#include <string>

class GURL;

namespace database_utils {

// Converts a GURL to a string used in databases.
std::string GurlToDatabaseUrl(const GURL& url);

}  // namespace database_utils

#endif  // COMPONENTS_DATABASE_UTILS_URL_CONVERTER_H_
