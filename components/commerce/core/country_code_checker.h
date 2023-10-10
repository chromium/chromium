// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COUNTRY_CODE_CHECKER_H_
#define COMPONENTS_COMMERCE_CORE_COUNTRY_CODE_CHECKER_H_

#include <string>

namespace variations {
class VariationsService;
}  // namespace variations

namespace commerce {
// TODO(crbug.com/1155712): We need this in a separate file because of this bug,
// otherwise it can be included in the commerce_utils.
// Get the user's current country code. If access through variations fails, the
// country_codes component is used.
std::string GetCurrentCountryCode(variations::VariationsService* variations);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COUNTRY_CODE_CHECKER_H_
