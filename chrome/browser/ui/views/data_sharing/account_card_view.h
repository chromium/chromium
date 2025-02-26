// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DATA_SHARING_ACCOUNT_CARD_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DATA_SHARING_ACCOUNT_CARD_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

struct AccountInfo;

class AccountCardView : public views::View {
  METADATA_HEADER(AccountCardView, views::View)
 public:
  explicit AccountCardView(AccountInfo account_info);
  ~AccountCardView() override;

  AccountCardView(const AccountCardView&) = delete;
  AccountCardView& operator=(const AccountCardView&) = delete;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DATA_SHARING_ACCOUNT_CARD_VIEW_H_
