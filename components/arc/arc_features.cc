// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_features.h"

namespace arc {

// Controls whether ARC++ app runtime performance statistics collection is
// enabled.
const base::Feature kAppRuntimePerormanceStatistics{
    "AppRuntimePerormanceStatistics", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls ACTION_BOOT_COMPLETED broadcast for third party applications on ARC.
// When disabled, third party apps will not receive this broadcast.
const base::Feature kBootCompletedBroadcastFeature {
    "ArcBootCompletedBroadcast", base::FEATURE_ENABLED_BY_DEFAULT
};

// Controls whether we should delete all ARC data before transitioning a user
// from regular to child account.
const base::Feature kCleanArcDataOnRegularToChildTransitionFeature{
    "ArcCleanDataOnRegularToChildTransition",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls experimental Custom Tabs feature for ARC.
const base::Feature kCustomTabsExperimentFeature{
    "ArcCustomTabsExperiment", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether ARC applications support zoom in/out.
const base::Feature kEnableApplicationZoomFeature{
    "ArcEnableApplicationZoomFeature", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether ARC handles child->regular account transition.
const base::Feature kEnableChildToRegularTransitionFeature{
    "ArcEnableChildToRegularTransition", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether ARC documents from DocumentsProviders should be shown in
// Chrome OS Files app.
const base::Feature kEnableDocumentsProviderInFilesAppFeature{
    "ArcEnableDocumentsProviderInFilesApp", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether ARC handles regular->child account transition.
const base::Feature kEnableRegularToChildTransitionFeature{
    "ArcEnableRegularToChildTransition", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether secondary accounts are added to ARC++ for child user.
const base::Feature kEnableSecondaryAccountsForChild{
    "ArcEnableSecondaryAccountForChild", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether we should delegate audio focus requests from ARC to Chrome.
const base::Feature kEnableUnifiedAudioFocusFeature{
    "ArcEnableUnifiedAudioFocus", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls experimental file picker feature for ARC.
const base::Feature kFilePickerExperimentFeature{
    "ArcFilePickerExperiment", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls experimental 64-bit native bridge support for ARC on boards that
// have 64-bit native bridge support available but not yet enabled.
const base::Feature kNativeBridge64BitSupportExperimentFeature{
    "ArcNativeBridge64BitSupportExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

// Toggles between native bridge implementations for ARC.
// Note, that we keep the original feature name to preserve
// corresponding metrics.
const base::Feature kNativeBridgeToggleFeature{
    "ArcNativeBridgeExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls ARC picture-in-picture feature. If this is enabled, then Android
// will control which apps can enter PIP. If this is disabled, then ARC PIP
// will be disabled.
const base::Feature kPictureInPictureFeature{"ArcPictureInPicture",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Controls ARC USB host integration.
// When enabled, Android apps will be able to use usb host features.
const base::Feature kUsbHostFeature{"ArcUsbHost",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls ARC USB Storage UI feature.
// When enabled, chrome://settings and Files.app will ask if the user wants
// to expose USB storage devices to ARC.
const base::Feature kUsbStorageUIFeature{"ArcUsbStorageUI",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether ARC uses VideoDecoder-backed video decoding.
// When enabled, GpuArcVideoDecodeAccelerator will use VdVideoDecodeAccelerator
// to delegate decoding tasks to VideoDecoder implementations, instead of using
// VDA implementations created by GpuVideoDecodeAcceleratorFactory.
const base::Feature kVideoDecoder{"ArcVideoDecoder",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace arc
