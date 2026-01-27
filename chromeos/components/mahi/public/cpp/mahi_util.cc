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

MahiActionType MatchButtonTypeToActionType(const ButtonType button_type) {
  switch (button_type) {
    case ButtonType::kSummary:
      return MahiActionType::kSummary;
    case ButtonType::kOutline:
      return MahiActionType::kOutline;
    case ButtonType::kSettings:
      return MahiActionType::kSettings;
    case ButtonType::kQA:
      return MahiActionType::kQA;
    case ButtonType::kElucidation:
      return MahiActionType::kElucidation;
    case ButtonType::kSummaryOfSelection:
      return MahiActionType::kSummaryOfSelection;
  }
}

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsMahiMenuKey, false)

}  // namespace chromeos::mahi
