// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_SCENE_CONSTANTS_H_
#define CHROME_BROWSER_VR_UI_SCENE_CONSTANTS_H_

#include "base/numerics/angle_conversions.h"

namespace vr {

inline constexpr float kExitWarningDistance = 0.6f;
inline constexpr float kExitWarningTextWidthDMM = 0.44288f;
inline constexpr float kExitWarningFontHeightDMM = 0.024576f;
inline constexpr float kExitWarningXPaddingDMM = 0.033f;
inline constexpr float kExitWarningYPaddingDMM = 0.023f;
inline constexpr float kExitWarningCornerRadiusDMM = 0.008f;

inline constexpr float kContentDistance = 2.5f;
inline constexpr float kContentWidthDMM = 0.96f;
inline constexpr float kContentHeightDMM = 0.64f;
inline constexpr float kContentWidth = kContentWidthDMM * kContentDistance;
inline constexpr float kContentHeight = kContentHeightDMM * kContentDistance;
inline constexpr float kContentVerticalOffsetDMM = -0.1f;
inline constexpr float kContentVerticalOffset =
    kContentVerticalOffsetDMM * kContentDistance;
inline constexpr float kContentCornerRadius = 0.005f * kContentWidth;
inline constexpr float kLoadingIndicatorHeightDMM = 0.016f;
inline constexpr float kLoadingIndicatorHeight = 0.016f * kContentDistance;
inline constexpr float kLoadingIndicatorYOffset = -0.002f;
inline constexpr float kBackplaneSize = 1000.0f;
inline constexpr float kBackgroundDistanceMultiplier = 1.414f;

inline constexpr float kFullscreenDistance = 3.0f;
// Make sure that the aspect ratio for fullscreen is 16:9. Otherwise, we may
// experience visual artefacts for fullscreened videos.
inline constexpr float kFullscreenHeightDMM = 0.64f;
inline constexpr float kFullscreenHeight =
    kFullscreenHeightDMM * kFullscreenDistance;
inline constexpr float kFullscreenWidth = 1.138f * kFullscreenDistance;
inline constexpr float kFullscreenVerticalOffsetDMM = -0.1f;
inline constexpr float kFullscreenVerticalOffset =
    kFullscreenVerticalOffsetDMM * kFullscreenDistance;

inline constexpr float kUrlBarDistance = 2.4f;
inline constexpr float kUrlBarHeightDMM = 0.088f;
// This is the non-DMM relative offset of the URL bar. It is used to position
// the DMM root of the URL bar.
inline constexpr float kUrlBarRelativeOffset = -0.45f;
// This is the absolute offset of the URL bar's neutral position in DMM.
inline constexpr float kUrlBarVerticalOffsetDMM = -0.516f;
inline constexpr float kUrlBarRotationRad = base::DegToRad(-10.0f);
inline constexpr float kUrlBarFontHeightDMM = 0.027f;
inline constexpr float kUrlBarButtonSizeDMM = 0.064f;
inline constexpr float kUrlBarButtonIconSizeDMM = 0.038f;
inline constexpr float kUrlBarEndButtonIconOffsetDMM = 0.0045f;
inline constexpr float kUrlBarEndButtonWidthDMM = 0.088f;
inline constexpr float kUrlBarSeparatorWidthDMM = 0.002f;
inline constexpr float kUrlBarOriginRegionWidthDMM = 0.492f;
inline constexpr float kUrlBarOriginRightMarginDMM = 0.020f;
inline constexpr float kUrlBarOriginContentOffsetDMM = 0.020f;
inline constexpr float kUrlBarItemCornerRadiusDMM = 0.006f;
inline constexpr float kUrlBarUrlWidthDMM = kUrlBarOriginRegionWidthDMM -
                                            kUrlBarEndButtonWidthDMM -
                                            kUrlBarOriginRightMarginDMM;
inline constexpr float kUrlBarButtonIconScaleFactor =
    kUrlBarButtonIconSizeDMM / kUrlBarButtonSizeDMM;

inline constexpr float kIndicatorHeightDMM = 0.064f;
inline constexpr float kIndicatorIconScaleFactor = 0.55f;
inline constexpr float kIndicatorXPaddingDMM = 0.024f;
inline constexpr float kIndicatorYPaddingDMM = 0.018f;
inline constexpr float kIndicatorCornerRadiusDMM = 0.006f;
inline constexpr float kIndicatorOffsetDMM = -0.008f;
inline constexpr float kIndicatorMarginDMM = 0.001f;
inline constexpr float kIndicatorVerticalOffset = 0.1f;
inline constexpr float kIndicatorDistanceOffset = 0.1f;
inline constexpr float kIndicatorDepth = 2.4f;

inline constexpr float kWebVrToastDistance = 1.0f;
inline constexpr float kToastXPaddingDMM = 0.017f;
inline constexpr float kToastYPaddingDMM = 0.02f;
inline constexpr float kToastCornerRadiusDMM = 0.004f;
inline constexpr float kToastTextFontHeightDMM = 0.023f;
inline constexpr int kToastTimeoutSeconds = 6;
inline constexpr int kWindowsInitialIndicatorsTimeoutSeconds = 10;
inline constexpr float kPlatformToastVerticalOffset = 0.5f;

inline constexpr float kSplashScreenTextDistance = 2.5f;
inline constexpr float kSplashScreenTextFontHeightDMM = 0.05f;
inline constexpr float kSplashScreenTextWidthDMM = 0.9f;
inline constexpr float kSplashScreenTextVerticalOffsetDMM = -0.072f;
inline constexpr float kSplashScreenMinDurationSeconds = 2.0f;

inline constexpr float kButtonDiameterDMM = 0.088f;
inline constexpr float kButtonZOffsetHoverDMM = 0.048f;

inline constexpr float kCloseButtonDistance = 2.4f;
inline constexpr float kCloseButtonRelativeOffset = -0.8f;
inline constexpr float kCloseButtonVerticalOffset =
    kFullscreenVerticalOffset - (kFullscreenHeight * 0.5f) - 0.35f;
inline constexpr float kCloseButtonDiameter =
    kButtonDiameterDMM * kCloseButtonDistance;
inline constexpr float kCloseButtonFullscreenDistance = 2.9f;
inline constexpr float kCloseButtonFullscreenVerticalOffset =
    kFullscreenVerticalOffset - (kFullscreenHeight / 2) - 0.35f;
inline constexpr float kCloseButtonFullscreenDiameter =
    kButtonDiameterDMM * kCloseButtonFullscreenDistance;

inline constexpr float kSceneSize = 25.0f;
inline constexpr float kSceneHeight = 4.0f;
inline constexpr float kFloorHeight = -kSceneHeight / 2.0f;
inline constexpr int kFloorGridlineCount = 40;

inline constexpr float kVoiceSearchCloseButtonDiameterDMM = 0.096f;
inline constexpr float kVoiceSearchCloseButtonDiameter =
    kVoiceSearchCloseButtonDiameterDMM * kContentDistance;
inline constexpr float kVoiceSearchCloseButtonYOffset =
    0.316f * kContentDistance + 0.5f * kVoiceSearchCloseButtonDiameter;
inline constexpr float kVoiceSearchRecognitionResultTextHeight =
    0.026f * kContentDistance;
inline constexpr float kVoiceSearchRecognitionResultTextWidth =
    0.4f * kContentDistance;

inline constexpr float kTimeoutScreenDisatance = 2.5f;
inline constexpr float kTimeoutSpinnerSizeDMM = 0.088f;
inline constexpr float kTimeoutSpinnerVerticalOffsetDMM =
    kSplashScreenTextVerticalOffsetDMM;

inline constexpr float kTimeoutMessageHorizontalPaddingDMM = 0.04f;
inline constexpr float kTimeoutMessageVerticalPaddingDMM = 0.024f;

inline constexpr float kTimeoutMessageCornerRadiusDMM = 0.008f;

inline constexpr float kTimeoutMessageLayoutGapDMM = 0.024f;
inline constexpr float kTimeoutMessageIconWidthDMM = 0.056f;
inline constexpr float kTimeoutMessageIconHeightDMM = 0.056f;
inline constexpr float kTimeoutMessageTextFontHeightDMM = 0.022f;
inline constexpr float kTimeoutMessageTextWidthDMM = 0.4f;

inline constexpr float kTimeoutButtonDepthOffset = -0.1f;
inline constexpr float kTimeoutButtonRotationRad = kUrlBarRotationRad;
inline constexpr float kWebVrTimeoutMessageButtonDiameterDMM = 0.096f;

inline constexpr float kTimeoutButtonTextWidthDMM = 0.058f;
inline constexpr float kTimeoutButtonTextVerticalOffsetDMM = 0.024f;

inline constexpr float kHostedUiHeightRatio = 0.6f;
inline constexpr float kHostedUiWidthRatio = 0.6f;
inline constexpr float kHostedUiDepthOffset = 0.3f;
inline constexpr float kHostedUiShadowOffset = 0.09f;
inline constexpr float kFloatingHostedUiDistance = 0.01f;

inline constexpr float kScreenDimmerOpacity = 0.9f;

inline constexpr gfx::Point3F kOrigin = {0.0f, 0.0f, 0.0f};

inline constexpr float kLaserWidth = 0.01f;

inline constexpr float kReticleWidth = 0.025f;
inline constexpr float kReticleHeight = 0.025f;

inline constexpr float kOmniboxWidthDMM = 0.672f;
inline constexpr float kOmniboxHeightDMM = 0.088f;
inline constexpr float kOmniboxVerticalOffsetDMM = -0.2f;
inline constexpr float kOmniboxTextHeightDMM = 0.032f;
inline constexpr float kOmniboxTextMarginDMM = 0.024f;
inline constexpr float kOmniboxMicIconRightMarginDMM = 0.012f;
inline constexpr float kOmniboxCloseButtonDiameterDMM = kButtonDiameterDMM;
inline constexpr float kOmniboxCloseButtonVerticalOffsetDMM = -0.75f;
inline constexpr float kOmniboxCornerRadiusDMM = 0.006f;
inline constexpr float kOmniboxCloseButtonDepthOffset = -0.35f;
inline constexpr int kOmniboxTransitionMs = 300;

inline constexpr float kOmniboxTextFieldIconButtonSizeDMM = 0.064f;
inline constexpr float kUrlBarButtonHoverOffsetDMM = 0.012f;
inline constexpr float kOmniboxTextFieldRightMargin =
    ((kOmniboxHeightDMM - kOmniboxTextFieldIconButtonSizeDMM) / 2);

inline constexpr float kSuggestionHeightDMM = 0.088f;
inline constexpr float kSuggestionGapDMM = 0.0018f;
inline constexpr float kSuggestionLineGapDMM = 0.01f;
inline constexpr float kSuggestionIconSizeDMM = 0.036f;
inline constexpr float kSuggestionIconFieldWidthDMM = 0.104f;
inline constexpr float kSuggestionRightMarginDMM = 0.024f;
inline constexpr float kSuggestionTextFieldWidthDMM =
    kOmniboxWidthDMM - kSuggestionIconFieldWidthDMM - kSuggestionRightMarginDMM;
inline constexpr float kSuggestionContentTextHeightDMM = 0.024f;
inline constexpr float kSuggestionDescriptionTextHeightDMM = 0.020f;
inline constexpr float kSuggestionVerticalPaddingDMM = 0.008f;

inline constexpr int kControllerFadeInMs = 200;
inline constexpr int kControllerFadeOutMs = 550;

inline constexpr float kSpeechRecognitionResultTextYOffset = 0.5f;
inline constexpr int kSpeechRecognitionResultTimeoutMs = 2000;
inline constexpr int kSpeechRecognitionOpacityAnimationDurationMs = 200;

inline constexpr float kModalPromptFadeOpacity = 0.5f;

inline constexpr float kKeyboardDistance = 2.2f;
inline constexpr float kKeyboardVerticalOffsetDMM = -0.45f;
inline constexpr float kKeyboardWebInputOffset = 1.2f;

inline constexpr float kControllerLabelSpacerSize = 0.025f;
inline constexpr float kControllerLabelLayoutMargin = -0.005f;
inline constexpr float kControllerLabelCalloutWidth = 0.02f;
inline constexpr float kControllerLabelCalloutHeight = 0.001f;
inline constexpr float kControllerLabelFontHeight = 0.05f;
inline constexpr float kControllerLabelScale = 0.2f;

// TODO(vollick): these should be encoded in the controller mesh.
inline constexpr float kControllerTrackpadOffset = -0.035f;
inline constexpr float kControllerExitButtonOffset = -0.008f;
inline constexpr float kControllerBackButtonOffset = -0.008f;

inline constexpr int kControllerLabelTransitionDurationMs = 700;

inline constexpr float kControllerWidth = 0.035f;
inline constexpr float kControllerHeight = 0.016f;
inline constexpr float kControllerLength = 0.105f;
inline constexpr float kControllerSmallButtonSize = kControllerWidth * 0.306f;
inline constexpr float kControllerAppButtonZ = kControllerLength * -0.075f;
inline constexpr float kControllerHomeButtonZ = kControllerLength * 0.075f;
inline constexpr float kControllerBatteryDotMargin = kControllerWidth * 0.07f;
inline constexpr float kControllerBatteryDotSize = kControllerWidth * 0.07f;
inline constexpr float kControllerBatteryDotZ = kControllerLength * 0.325f;
inline constexpr int kControllerBatteryDotCount = 5;

inline constexpr float kSkyDistance = 1000.0f;
inline constexpr float kGridOpacity = 0.5f;

inline constexpr float kRepositionContentOpacity = 0.2f;

inline constexpr float kWebVrPermissionCornerRadius = 0.006f;
inline constexpr float kWebVrPermissionLeftPadding = 0.024f;
inline constexpr float kWebVrPermissionRightPadding = 0.032f;
inline constexpr float kWebVrPermissionTopPadding = 0.026f;
inline constexpr float kWebVrPermissionBottomPadding = 0.026f;
inline constexpr float kWebVrPermissionMargin = 0.016f;
inline constexpr float kWebVrPermissionIconSize = 0.034f;
inline constexpr float kWebVrPermissionFontHeight = 0.024f;
inline constexpr float kWebVrPermissionTextWidth = 0.380f;
inline constexpr float kWebVrPermissionOuterMargin = 0.008f;
inline constexpr float kWebVrPermissionDepth = 0.015f;
inline constexpr float kWebVrPermissionOffsetStart = 0.3f;
inline constexpr float kWebVrPermissionOffsetOvershoot = -0.01f;
inline constexpr float kWebVrPermissionOffsetFinal = 0.0f;
inline constexpr int kWebVrPermissionOffsetMs = 250;
inline constexpr int kWebVrPermissionAnimationDurationMs = 750;

inline constexpr float kPromptVerticalOffsetDMM = -0.1f;
inline constexpr float kPromptShadowOffsetDMM = 0.1f;
inline constexpr float kPromptDistance = 2.4f;
inline constexpr float kPromptPadding = 0.028f;
inline constexpr float kPromptCornerRadius = 0.006f;
inline constexpr float kPromptTextWidth = 0.522f;
inline constexpr float kPromptFontSize = 0.028f;
inline constexpr float kPromptIconSize = 0.042f;
inline constexpr float kPromptButtonCornerRadius = 0.0035f;
inline constexpr float kPromptIconTextGap = 0.010f;
inline constexpr float kPromptMessageButtonGap = 0.056f;
inline constexpr float kPromptButtonTextSize = 0.024f;
inline constexpr float kPromptButtonGap = 0.014f;

inline constexpr float kRepositionCursorBackgroundSize = 1.85f;
inline constexpr float kRepositionCursorSize = 1.5f;

inline constexpr float kMinResizerScale = 0.5f;
inline constexpr float kMaxResizerScale = 1.5f;

inline constexpr float kRepositionFrameTopPadding = 0.25f;
inline constexpr float kRepositionFrameEdgePadding = 0.04f;
inline constexpr float kRepositionFrameHitPlaneTopPadding = 0.5f;
inline constexpr float kRepositionFrameTransitionDurationMs = 300;

inline constexpr float kOverflowMenuOffset = 0.016f;
inline constexpr float kOverflowMenuMinimumWidth = 0.312f;
inline constexpr float kOverflowButtonRegionHeight = 0.088f;
inline constexpr float kOverflowButtonRegionOpacity = 0.97f;
inline constexpr float kOverflowButtonXPadding = 0.016f;
inline constexpr float kOverflowButtonYPadding = 0.012f;
inline constexpr float kOverflowMenuYPadding = 0.012f;
inline constexpr float kOverflowMenuItemHeight = 0.080f;
inline constexpr float kOverflowMenuItemXPadding = 0.024f;
inline constexpr float kOverflowMenuMaxSpan = 0.384f - kOverflowMenuYPadding;

inline constexpr const char* kCrashVrBrowserUrl = "chrome://crash-vr-browser";

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_SCENE_CONSTANTS_H_
