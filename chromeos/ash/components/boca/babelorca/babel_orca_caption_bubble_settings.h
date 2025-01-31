// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_CAPTION_BUBBLE_SETTINGS_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_CAPTION_BUBBLE_SETTINGS_H_

#include "components/live_caption/caption_bubble_settings.h"

namespace ash::babelorca {

class BabelOrcaCaptionBubbleSettings
    : public ::captions::CaptionBubbleSettings {
 public:
  BabelOrcaCaptionBubbleSettings(const BabelOrcaCaptionBubbleSettings&) =
      delete;
  BabelOrcaCaptionBubbleSettings& operator=(
      const BabelOrcaCaptionBubbleSettings&) = delete;

  ~BabelOrcaCaptionBubbleSettings() override = default;

  virtual void SetLiveTranslateEnabled(bool enabled) = 0;

 protected:
  BabelOrcaCaptionBubbleSettings() = default;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_CAPTION_BUBBLE_SETTINGS_H_
