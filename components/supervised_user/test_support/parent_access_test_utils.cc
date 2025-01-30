// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/test_support/parent_access_test_utils.h"

#include <string>

#include "base/base64.h"
#include "components/supervised_user/core/browser/proto/parent_access_callback.pb.h"

namespace supervised_user {

std::string CreatePacpApprovalResult() {
  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  kids::platform::parentaccess::client::proto::OnParentVerified*
      on_parent_verified = parent_access_callback.mutable_on_parent_verified();
  kids::platform::parentaccess::client::proto::ParentAccessToken* token =
      on_parent_verified->mutable_parent_access_token();
  token->set_token("TEST_TOKEN");
  kids::platform::parentaccess::client::proto::Timestamp* expire_time =
      token->mutable_expire_time();
  expire_time->set_seconds(123456);
  return base::Base64Encode(parent_access_callback.SerializeAsString());
}

std::string CreatePacpResizeResult() {
  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  kids::platform::parentaccess::client::proto::OnPageSizeChanged*
      on_page_size_changed =
          parent_access_callback.mutable_on_page_size_changed();
  on_page_size_changed->set_content_height(40);
  return base::Base64Encode(parent_access_callback.SerializeAsString());
}

}  // namespace supervised_user
