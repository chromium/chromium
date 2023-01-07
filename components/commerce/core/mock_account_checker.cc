// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/mock_account_checker.h"

namespace commerce {

MockAccountChecker::MockAccountChecker()
    : AccountChecker(nullptr, nullptr, nullptr) {}
MockAccountChecker::~MockAccountChecker() = default;

bool MockAccountChecker::IsSignedIn() {
  return signed_in_;
}

bool MockAccountChecker::IsAnonymizedUrlDataCollectionEnabled() {
  return anonymized_url_data_collection_enabled_;
}

bool MockAccountChecker::IsWebAndAppActivityEnabled() {
  return web_and_app_activity_enabled_;
}

void MockAccountChecker::SetSignedIn(bool signed_in) {
  signed_in_ = signed_in;
}

void MockAccountChecker::SetAnonymizedUrlDataCollectionEnabled(bool enabled) {
  anonymized_url_data_collection_enabled_ = enabled;
}

void MockAccountChecker::SetWebAndAppActivityEnabled(bool enabled) {
  web_and_app_activity_enabled_ = enabled;
}

}  // namespace commerce
