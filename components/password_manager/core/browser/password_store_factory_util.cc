// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_factory_util.h"

#include <memory>
#include <utility>

#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_manager_constants.h"

namespace password_manager {

std::unique_ptr<LoginDatabase> CreateLoginDatabaseForProfileStorage(
    const base::FilePath& profile_path) {
  base::FilePath login_db_file_path =
      profile_path.Append(kLoginDataForProfileFileName);
  return std::make_unique<LoginDatabase>(login_db_file_path,
                                         IsAccountStore(false));
}

std::unique_ptr<LoginDatabase> CreateLoginDatabaseForAccountStorage(
    const base::FilePath& profile_path) {
  base::FilePath login_db_file_path =
      profile_path.Append(kLoginDataForAccountFileName);
  return std::make_unique<LoginDatabase>(login_db_file_path,
                                         IsAccountStore(true));
}

}  // namespace password_manager
