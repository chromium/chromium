// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ERROR_UMA_LOGGING_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ERROR_UMA_LOGGING_H_

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/types/expected.h"

namespace web_app {

// Every error type that is logged by the UmaLogExpectedStatus function should
// be converted to a UMA-friendly enumeration. This function is an
// implementation for the case when the error type is enumeration.
template <class E>
E ToErrorEnum(E e) {
  static_assert(std::is_enum<E>::value, "E is not an enum.");
  return e;
}

// These functions return the names of the histograms where the
// wrapped in base::expected<> error are logged.
std::string ToSuccessHistogramName(std::string_view base_name);
std::string ToErrorHistogramName(std::string_view base_name);

// UMA-logs an error wrapped in base::expected<void, ErrorType>. It creates
// 2 histograms by appending corresponding suffixes to `base_name`.
// One histogram logs the success/failure rate. The other histogram logs
// the error type if any.
// Any custom error type should provide a specialized template function that
// converts the custom error type to error enum.
template <class E>
void UmaLogExpectedStatus(std::string_view base_name,
                          const base::expected<void, E>& status) {
  base::UmaHistogramBoolean(ToSuccessHistogramName(base_name),
                            status.has_value());

  if (!status.has_value()) {
    base::UmaHistogramEnumeration(ToErrorHistogramName(base_name),
                                  ToErrorEnum(status.error()));
  }
}

}  // namespace web_app
#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ERROR_UMA_LOGGING_H_
