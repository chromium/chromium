// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DEPENDENCIES_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DEPENDENCIES_UTIL_H_

#include <string>

#include "components/variations/service/variations_service.h"

namespace autofill_assistant::dependencies_util {

std::string GetCountryCode(variations::VariationsService* variations_service);

}  // namespace autofill_assistant::dependencies_util

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DEPENDENCIES_UTIL_H_
