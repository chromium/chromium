// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"

#include "base/version_info/channel.h"

namespace supervised_user {

std::string FakeURLFilterDelegate::GetCountryCode() const {
  // Country code information is not used in tests.
  return std::string();
}

version_info::Channel FakeURLFilterDelegate::GetChannel() const {
  // Channel information is not used in tests.
  return version_info::Channel::UNKNOWN;
}

}  // namespace supervised_user
