// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category_status.h"

namespace ntp_snippets {

bool IsCategoryStatusAvailable(CategoryStatus status) {
  // Note: This code is duplicated in SnippetsBridge.java.
  return status == CategoryStatus::AVAILABLE_LOADING ||
         status == CategoryStatus::AVAILABLE;
}

bool IsCategoryStatusInitOrAvailable(CategoryStatus status) {
  // Note: This code is duplicated in SnippetsBridge.java.
  return status == CategoryStatus::INITIALIZING ||
         IsCategoryStatusAvailable(status);
}

}  // namespace ntp_snippets
