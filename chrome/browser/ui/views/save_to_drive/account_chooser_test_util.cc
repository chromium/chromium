// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/save_to_drive/account_chooser_test_util.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace save_to_drive::testing {

AccountInfo GetTestAccount(const std::string& name,
                           const std::string& domain,
                           int32_t gaia_id) {
  AccountInfo account_info;
  account_info.full_name = name;
  account_info.email = base::StrCat({name, "@", domain});
  account_info.account_id =
      CoreAccountId::FromGaiaId(GaiaId(base::NumberToString(gaia_id)));
  account_info.account_image =
      gfx::Image::CreateFrom1xBitmap(gfx::test::CreateBitmap(1));
  return account_info;
}

std::vector<AccountInfo> GetTestAccounts(const std::vector<std::string>& names,
                                         const std::string& domain) {
  std::vector<AccountInfo> accounts;
  int32_t id = 0;
  for (const auto& name : names) {
    AccountInfo account_info;
    account_info.full_name = name;
    account_info.email = base::StrCat({name, "@", domain});
    account_info.account_id =
        CoreAccountId::FromGaiaId(GaiaId(base::NumberToString(id)));
    account_info.account_image =
        gfx::Image::CreateFrom1xBitmap(gfx::test::CreateBitmap(1));
    accounts.push_back(account_info);
    ++id;
  }
  return accounts;
}

bool VerifyAccountChooserRow(views::View* row_view,
                             const AccountInfo& account) {
  std::vector<raw_ptr<views::View, VectorExperimental>> account_view_children =
      row_view->children();
  if (account_view_children.size() != 2u) {
    return false;
  }
  views::ImageView* image_view =
      static_cast<views::ImageView*>(account_view_children.front());
  if (!image_view) {
    return false;
  }

  std::vector<raw_ptr<views::View, VectorExperimental>> account_info_children =
      account_view_children.back()->children();
  if (account_info_children.size() != 2u) {
    return false;
  }
  views::Label* name_view =
      static_cast<views::Label*>(account_info_children.front());
  if (!name_view) {
    return false;
  }
  if (name_view->GetText() != base::UTF8ToUTF16(account.full_name)) {
    return false;
  }
  views::Label* email_view =
      static_cast<views::Label*>(account_info_children.back());
  if (!email_view) {
    return false;
  }
  return email_view->GetText() == base::UTF8ToUTF16(account.email);
}

}  // namespace save_to_drive::testing
