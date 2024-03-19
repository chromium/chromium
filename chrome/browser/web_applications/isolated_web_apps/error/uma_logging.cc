// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/error/uma_logging.h"

#include <string_view>

namespace web_app {
namespace {
const char kUmaSuccessSuffix[] = "Success";
const char kUmaErrorSuffix[] = "Error";
}  // namespace

std::string ToSuccessHistogramName(std::string_view base_name) {
  return base::StrCat({base_name, kUmaSuccessSuffix});
}

std::string ToErrorHistogramName(std::string_view base_name) {
  return base::StrCat({base_name, kUmaErrorSuffix});
}
}  // namespace web_app
