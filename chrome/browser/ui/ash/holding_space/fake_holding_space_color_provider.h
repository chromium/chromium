// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_FAKE_HOLDING_SPACE_COLOR_PROVIDER_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_FAKE_HOLDING_SPACE_COLOR_PROVIDER_H_

#include "ash/public/cpp/holding_space/holding_space_color_provider.h"

namespace ash {
namespace holding_space {

class FakeHoldingSpaceColorProvider : public ash::HoldingSpaceColorProvider {
 public:
  FakeHoldingSpaceColorProvider() = default;
  FakeHoldingSpaceColorProvider(const FakeHoldingSpaceColorProvider&) = delete;
  FakeHoldingSpaceColorProvider& operator=(
      const FakeHoldingSpaceColorProvider&) = delete;
  ~FakeHoldingSpaceColorProvider() override = default;

  // ash::HoldingSpaceColorProvider:
  SkColor GetBackgroundColor() const override;
  SkColor GetFileIconColor() const override;
};

}  // namespace holding_space
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_FAKE_HOLDING_SPACE_COLOR_PROVIDER_H_