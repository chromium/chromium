// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBID_IDENTITY_UI_UTILS_H_
#define CHROME_BROWSER_UI_WEBID_IDENTITY_UI_UTILS_H_

#include <optional>

#include "build/build_config.h"
#include "build/buildflag.h"
#include "content/public/browser/webid/identity_request_account.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"

using IdentityRequestAccountPtr =
    scoped_refptr<content::IdentityRequestAccount>;

// This file contains helper methods that are used in FedCM UI on both desktop
// and Android.
namespace webid {

#if BUILDFLAG(IS_ANDROID)
// The desired size of the avatars of user accounts.
inline constexpr int kDesiredAvatarSize = 40;
#else
// The desired size of the avatars of user accounts.
inline constexpr int kDesiredAvatarSize = 30;
#endif  // BUILDFLAG(IS_ANDROID)
// The desired size of the avatars of user accounts in autofill dropdown.
inline constexpr int kDesiredAvatarSizeInAutofillDropdown = 20;
// The size of avatars in the modal dialog.
inline constexpr int kModalAvatarSize = 36;
// Size of the IDP icon offset when badging the IDP icon in the account button.
inline constexpr int kIdpBadgeOffset = 8;
// safe_zone_diameter/icon_size as defined in
// https://www.w3.org/TR/appmanifest/#icon-masks
inline constexpr float kMaskableWebIconSafeZoneRatio = 0.8f;
// The opacity of the avatar when the account is filtered out.
inline constexpr double kDisabledAvatarOpacity = 0.38;

// This enum is used for histograms. Do not remove or modify existing values,
// but you may add new values at the end.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ui.android.webid
// LINT.IfChange(SheetType)

enum class SheetType {
  kAccountSelection = 0,
  kVerifying = 1,
  kAutoReauthn = 2,
  kSignInToIdpStatic = 3,
  kSignInError = 4,
  kLoading = 5,
  kMaxValue = kLoading
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:FedCmSheetType)

// This enum describes the outcome of the account chooser and is used for
// histograms. Do not remove or modify existing values, but you may add new
// values at the end.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ui.android.webid
// LINT.IfChange(AccountChooserResult)

enum class AccountChooserResult {
  kAccountRow = 0,
  kCancelButton = 1,
  kUseOtherAccountButton = 2,
  kTabClosed = 3,
  // Android-specific
  kSwipe = 4,
  // Android-specific
  kBackPress = 5,
  // Android-specific
  kTapScrim = 6,
  kMaxValue = kTapScrim
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:FedCmAccountChooserResult)

// This enum describes the outcome of the loading dialog and is used for
// histograms. Do not remove or modify existing values, but you may add new
// values at the end. This enum should be kept in sync with
// FedCmLoadingDialogResult in tools/metrics/histograms/enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ui.android.webid
// LINT.IfChange(LoadingDialogResult)

enum class LoadingDialogResult {
  kProceed = 0,
  kCancel = 1,
  kProceedThroughPopup = 2,
  kDestroy = 3,
  // Android-specific
  kSwipe = 4,
  // Android-specific
  kBackPress = 5,
  // Android-specific
  kTapScrim = 6,
  kMaxValue = kTapScrim
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:FedCmLoadingDialogResult)

// This enum describes the outcome of the disclosure dialog and is used for
// histograms. Do not remove or modify existing values, but you may add new
// values at the end.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ui.android.webid
// LINT.IfChange(DisclosureDialogResult)

enum class DisclosureDialogResult {
  kContinue = 0,
  kCancel = 1,
  kBack = 2,
  kDestroy = 3,
  // Android-specific
  kSwipe = 4,
  // Android-specific
  kBackPress = 5,
  // Android-specific
  kTapScrim = 6,
  kMaxValue = kTapScrim
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:FedCmDisclosureDialogResult)

// Extracts the initial letter from the provided string.
std::u16string GetInitialLetterAsUppercase(const std::string& utf8_string);

// Creates a circle cropped image of the given size from the original image.
gfx::ImageSkia CreateCircleCroppedImage(const gfx::ImageSkia& original_image,
                                        int image_size);

// Computes the circle cropped picture from the given account and of the given
// size. If `idp_image` is not std::nullopt, the image is circle cropped and
// badged into the account picture as well. `device_scale_factor` is the device
// scale factor, so that the image returned is of the correct resolution.
gfx::ImageSkia ComputeAccountCircleCroppedPicture(
    const content::IdentityRequestAccount& account,
    int avatar_size,
    std::optional<gfx::ImageSkia> idp_image,
    float device_scale_factor);

// A CanvasImageSource that draws a letter in a circle.
class LetterCircleCroppedImageSkiaSource : public gfx::CanvasImageSource {
 public:
  LetterCircleCroppedImageSkiaSource(const std::u16string& letter, int size);
  LetterCircleCroppedImageSkiaSource(
      const LetterCircleCroppedImageSkiaSource&) = delete;
  LetterCircleCroppedImageSkiaSource& operator=(
      const LetterCircleCroppedImageSkiaSource&) = delete;
  ~LetterCircleCroppedImageSkiaSource() override = default;

  void Draw(gfx::Canvas* canvas) override;

 private:
  const std::u16string letter_;
};

// A CanvasImageSource that:
// 1) Applies an optional square center-crop.
// 2) Resizes the cropped image (while maintaining the image's aspect ratio) to
//    fit into the target canvas. If no center-crop was applied and the source
//    image is rectangular, the image is resized so that
//    `avatar` small edge size == `canvas_edge_size`.
// 3) Circle center-crops the resized image.
class CircleCroppedImageSkiaSource : public gfx::CanvasImageSource {
 public:
  CircleCroppedImageSkiaSource(
      gfx::ImageSkia avatar,
      const std::optional<int>& pre_resize_avatar_crop_size,
      int canvas_edge_size);

  CircleCroppedImageSkiaSource(const CircleCroppedImageSkiaSource&) = delete;
  CircleCroppedImageSkiaSource& operator=(const CircleCroppedImageSkiaSource&) =
      delete;
  ~CircleCroppedImageSkiaSource() override = default;

  // CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;

 private:
  gfx::ImageSkia avatar_;
};

}  // namespace webid

#endif  // CHROME_BROWSER_UI_WEBID_IDENTITY_UI_UTILS_H_
