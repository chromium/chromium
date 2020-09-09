// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_CLIENT_IMPL_H_

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "base/callback.h"

class Profile;

namespace ash {

// Implementation of the holding space browser client.
class HoldingSpaceClientImpl : public HoldingSpaceClient {
 public:
  explicit HoldingSpaceClientImpl(Profile* profile);
  HoldingSpaceClientImpl(const HoldingSpaceClientImpl& other) = delete;
  HoldingSpaceClientImpl& operator=(const HoldingSpaceClientImpl& other) =
      delete;
  ~HoldingSpaceClientImpl() override;

  // HoldingSpaceClient:
  void CopyToClipboard(const HoldingSpaceItem&, SuccessCallback) override;
  void OpenItem(const HoldingSpaceItem&, SuccessCallback) override;
  void OpenItemInFolder(const HoldingSpaceItem&, SuccessCallback) override;
  void PinItem(const HoldingSpaceItem& item) override;
  void UnpinItem(const HoldingSpaceItem& item) override;

 private:
  Profile* const profile_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_CLIENT_IMPL_H_
