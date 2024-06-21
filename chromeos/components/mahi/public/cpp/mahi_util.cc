// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/public/cpp/mahi_util.h"

namespace chromeos::mahi {

const char kMahiContentExtractionTriggeringLatency[] =
    "ChromeOS.Mahi.ContentExtraction.TriggeringLatency";
const char kMahiContextMenuActivated[] =
    "ChromeOS.Mahi.ContextMenuView.Activated";
const char kMahiContextMenuActivatedFailed[] =
    "ChromeOS.Mahi.ContextMenuView.ActivatedFailed";

ActionType MatchButtonTypeToActionType(const ButtonType button_type) {
  switch (button_type) {
    case ButtonType::kSummary:
      return ActionType::kSummary;
    case ButtonType::kOutline:
      return ActionType::kOutline;
    case ButtonType::kSettings:
      return ActionType::kSettings;
    case ButtonType::kQA:
      return ActionType::kQA;
  }
}

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsMahiMenuKey, false)

}  // namespace chromeos::mahi
