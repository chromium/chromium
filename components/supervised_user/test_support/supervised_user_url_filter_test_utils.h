// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_SUPERVISED_USER_URL_FILTER_TEST_UTILS_H_
#define COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_SUPERVISED_USER_URL_FILTER_TEST_UTILS_H_

#include <string>

#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"

namespace version_info {
enum class Channel;
}

namespace supervised_user {

class FakeURLFilterDelegate : public SupervisedUserURLFilter::Delegate {
 public:
  bool SupportsWebstoreURL(const GURL& url) const override;
};

class FakePlatformDelegate : public SupervisedUserService::PlatformDelegate {
 public:
  std::string GetCountryCode() const override;
  version_info::Channel GetChannel() const override;
  void CloseIncognitoTabs() override;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_SUPERVISED_USER_URL_FILTER_TEST_UTILS_H_
