// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/screen_sharing_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"

namespace {

using content::DesktopMediaID;
using InteractionWithControls = GetDisplayMediaUserInteractionWithControls;

// This enum is used to record UMA histogram metrics for interactions
// with the TabSharingInfoBar.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// TODO(crbug.com/440459628): Sunset this histogram and use
// GetDisplayMediaUserInteractionWithControls in its stead.
//
// LINT.IfChange(TabSharingInfoBarInteraction)
enum class TabSharingInfoBarInteraction {
  kCapturedToCapturing = 0,
  kCapturingToCaptured = 1,
  kOtherToCapturing = 2,
  kOtherToCaptured = 3,
  kStopButtonClicked = 4,
  kShareThisTabInsteadButtonClicked = 5,
  kMaxValue = kShareThisTabInsteadButtonClicked,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/media/enums.xml:MediaUiGetDisplayMediaTabSharingInfoBarInteraction)

std::optional<TabSharingInfoBarInteraction> ToLegacyUma(
    InteractionWithControls interaction) {
  switch (interaction) {
    case InteractionWithControls::kNoInteraction:
    case InteractionWithControls::kHideButtonClicked:
      return std::nullopt;  // No corresponding value.
    case InteractionWithControls::kStopButtonClicked:
      return TabSharingInfoBarInteraction::kStopButtonClicked;
    case InteractionWithControls::kCapturedToCapturingClicked:
      return TabSharingInfoBarInteraction::kCapturedToCapturing;
    case InteractionWithControls::kCapturingToCapturedClicked:
      return TabSharingInfoBarInteraction::kCapturingToCaptured;
    case InteractionWithControls::kOtherToCapturingClicked:
      return TabSharingInfoBarInteraction::kOtherToCapturing;
    case InteractionWithControls::kOtherToCapturedClicked:
      return TabSharingInfoBarInteraction::kOtherToCaptured;
    case InteractionWithControls::kShareThisTabInsteadClicked:
      return TabSharingInfoBarInteraction::kShareThisTabInsteadButtonClicked;
  }
  NOTREACHED();
}

}  // namespace

ScreensharingControlsHistogramLogger::ScreensharingControlsHistogramLogger(
    content::DesktopMediaID::Type captured_surface_type)
    : captured_surface_type_(captured_surface_type) {}

ScreensharingControlsHistogramLogger::~ScreensharingControlsHistogramLogger() {
  if (!interaction_with_controls_logged_) {
    Log(InteractionWithControls::kNoInteraction);
  }
}

void ScreensharingControlsHistogramLogger::Log(
    InteractionWithControls interaction) {
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

  // Record legacy UMA.
  const std::optional<TabSharingInfoBarInteraction> legacy_interaction =
      ToLegacyUma(interaction);
  if (legacy_interaction.has_value()) {
    base::UmaHistogramEnumeration(
        "Media.Ui.GetDisplayMedia.TabSharingInfoBarInteraction",
        legacy_interaction.value());
  }
}

base::WeakPtr<ScreensharingControlsHistogramLogger>
ScreensharingControlsHistogramLogger::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
