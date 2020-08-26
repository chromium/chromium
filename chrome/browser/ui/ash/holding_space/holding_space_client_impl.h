// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_CLIENT_IMPL_H_

#include "ash/public/cpp/holding_space/holding_space_client.h"

class Profile;

// Implementation of the holding space browser client.
class ASH_PUBLIC_EXPORT HoldingSpaceClientImpl
    : public ash::HoldingSpaceClient {
 public:
  explicit HoldingSpaceClientImpl(Profile* profile);
  HoldingSpaceClientImpl(const HoldingSpaceClientImpl& other) = delete;
  HoldingSpaceClientImpl& operator=(const HoldingSpaceClientImpl& other) =
      delete;
  ~HoldingSpaceClientImpl() override;

  // ash::HoldingSpaceClient:
  void OpenItem(const ash::HoldingSpaceItem& item,
                OpenItemCallback callback) override;

 private:
  Profile* const profile_;
};

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_CLIENT_IMPL_H_
