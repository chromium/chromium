// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_SUPERVISED_USER_URL_FILTER_TEST_UTILS_H_
#define COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_SUPERVISED_USER_URL_FILTER_TEST_UTILS_H_

#include <string>

#include "components/supervised_user/core/browser/supervised_user_url_filter.h"

namespace supervised_user {

class FakeURLFilterDelegate : public SupervisedUserURLFilter::Delegate {
 public:
  std::string GetCountryCode() override;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_SUPERVISED_USER_URL_FILTER_TEST_UTILS_H_
