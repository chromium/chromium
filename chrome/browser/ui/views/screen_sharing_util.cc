// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/screen_sharing_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"

using content::DesktopMediaID;

void RecordUma(TabSharingInfoBarInteraction interaction) {
  base::UmaHistogramEnumeration(
      "Media.Ui.GetDisplayMedia.TabSharingInfoBarInteraction", interaction);
}

ScreensharingControlsHistogramLogger::ScreensharingControlsHistogramLogger(
    content::DesktopMediaID::Type captured_surface_type)
    : captured_surface_type_(captured_surface_type) {}

ScreensharingControlsHistogramLogger::~ScreensharingControlsHistogramLogger() {
  if (!interaction_with_controls_logged_) {
    Log(GetDisplayMediaUserInteractionWithControls::kNoInteraction);
  }
}

void ScreensharingControlsHistogramLogger::Log(
    GetDisplayMediaUserInteractionWithControls interaction) {
  const char* display_surface = nullptr;
  switch (captured_surface_type_) {
    case DesktopMediaID::Type::TYPE_NONE:
      return;
    case DesktopMediaID::Type::TYPE_WEB_CONTENTS:
      display_surface = "Tabs";
      break;
    case DesktopMediaID::Type::TYPE_WINDOW:
      display_surface = "Windows";
      break;
    case DesktopMediaID::Type::TYPE_SCREEN:
      display_surface = "Screens";
      break;
  }
  CHECK_NE(display_surface, nullptr);

  const std::string name = base::StringPrintf(
      "Media.Ui.GetDisplayMedia.%s.UserInteractionWithControls",
      display_surface);

  base::UmaHistogramEnumeration(name, interaction);
  interaction_with_controls_logged_ = true;
}
