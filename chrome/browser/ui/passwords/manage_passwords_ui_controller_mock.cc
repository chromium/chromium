// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"

#include "base/memory/ptr_util.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

ManagePasswordsUIControllerMock::ManagePasswordsUIControllerMock(
    content::WebContents* contents)
    : ManagePasswordsUIController(contents) {
  // Do not silently replace an existing ManagePasswordsUIController because it
  // unregisters itself in WebContentsDestroyed().
  EXPECT_FALSE(contents->GetUserData(UserDataKey()));
  contents->SetUserData(UserDataKey(), base::WrapUnique(this));
}

ManagePasswordsUIControllerMock::~ManagePasswordsUIControllerMock() = default;
