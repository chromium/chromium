// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBID_IDENTITY_UI_UTILS_H_
#define CHROME_BROWSER_UI_WEBID_IDENTITY_UI_UTILS_H_

#include <optional>

#include "build/build_config.h"
#include "build/buildflag.h"
#include "ui/gfx/image/image_skia.h"

namespace content {
class IdentityRequestAccount;
}  // namespace content

using IdentityRequestAccountPtr =
    scoped_refptr<content::IdentityRequestAccount>;

// This file contains helper methods that are used in FedCM UI on both desktop
// and Android.

#if BUILDFLAG(IS_ANDROID)
// The desired size of the avatars of user accounts.
inline constexpr int kDesiredAvatarSize = 40;
#else
// The desired size of the avatars of user accounts.
inline constexpr int kDesiredAvatarSize = 30;
#endif  // BUILDFLAG(IS_ANDROID)
// The size of avatars in the modal dialog.
inline constexpr int kModalAvatarSize = 36;
// Size of the IDP icon offset when badging the IDP icon in the account button.
inline constexpr int kIdpBadgeOffset = 8;
// safe_zone_diameter/icon_size as defined in
// https://www.w3.org/TR/appmanifest/#icon-masks
inline constexpr float kMaskableWebIconSafeZoneRatio = 0.8f;
// The opacity of the avatar when the account is filtered out.
inline constexpr double kDisabledAvatarOpacity = 0.38;

// Extracts the initial letter from the provided string.
std::u16string GetInitialLetterAsUppercase(const std::string& utf8_string);

// Creates a circle cropped image of the given size from the original image.
gfx::ImageSkia CreateCircleCroppedImage(const gfx::ImageSkia& original_image,
                                        int image_size);

// Computes the circle cropped picture from the given account and of the given
// size. If `idp_image` is not std::nullopt, the image is circle cropped and
// badged into the account picture as well.
gfx::ImageSkia ComputeAccountCircleCroppedPicture(
    const content::IdentityRequestAccount& account,
    int avatar_size,
    std::optional<gfx::ImageSkia> idp_image);

#endif  // CHROME_BROWSER_UI_WEBID_IDENTITY_UI_UTILS_H_
