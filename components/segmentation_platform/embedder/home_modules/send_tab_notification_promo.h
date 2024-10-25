// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_SEND_TAB_NOTIFICATION_PROMO_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_SEND_TAB_NOTIFICATION_PROMO_H_

#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"

namespace segmentation_platform::home_modules {

// Signal Keys for this card.
extern const char kSendTabInfobarReceivedInLastSessionSignalKey[];

// Defines input labels and ranking logic for the Send Tab To Self notification
// ephemeral card shown on home modules stack.
class SendTabNotificationPromo : public CardSelectionInfo {
 public:
  explicit SendTabNotificationPromo(int send_tab_promo_count);
  ~SendTabNotificationPromo() override = default;

  static bool IsEnabled(int impression_count);

  // CardSelectionInfo
  std::map<SignalKey, FeatureQuery> GetInputs() override;
  ShowResult ComputeCardResult(
      const CardSelectionSignals& signals) const override;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_SEND_TAB_NOTIFICATION_PROMO_H_
