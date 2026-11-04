// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_AMBIENT_LOGIN_CREDENTIAL_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_AMBIENT_LOGIN_CREDENTIAL_ICON_H_

#include <optional>
#include <string>
#include <string_view>

#include "third_party/skia/include/core/SkColor.h"

namespace ambient_signin {
struct PasskeyOrPasswordCredential;
}  // namespace ambient_signin

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Image;
}  // namespace gfx

namespace ui {
class ImageModel;
}  // namespace ui

namespace ambient_login_credential_icon {

// The size of vector icons in the ambient sign-in bubble, in DIPs, e.g. the
// generic passkey or password icon shown instead of a monogram.
inline constexpr int kVectorIconSize = 20;

// Returns the site brand colour for `web_contents`, using its favicon if it
// has loaded and its theme colour, or nullopt if the site has neither.
std::optional<SkColor> GetSiteColorForWebContents(
    content::WebContents* web_contents);

// Returns the icon for `credential`: a monogram as large as an account picture
// (`ambient_signin::kAccountPictureSize`), with `site_color` (or the theme's
// primary colour if nullopt) as its background. The monogram is outlined if its
// colour is very close to the bubble's background (see
// `internal::NeedsMonogramOutline()`). If there is no character to show,
// returns the generic passkey or password icon, `kVectorIconSize` DIPs square.
ui::ImageModel CreateCredentialIcon(
    const ambient_signin::PasskeyOrPasswordCredential& credential,
    std::optional<SkColor> site_color);

// Returns the provider badge (`ambient_signin::kBadgeImageSize` DIPs square) to
// show on `credential`'s monogram.
ui::ImageModel GetCredentialBadge(
    const ambient_signin::PasskeyOrPasswordCredential& credential);

namespace internal {

// Exposed for unit testing.

// Returns whether a monogram with `monogram_background` needs an outline to
// stand out against `surrounding_color`, because the two are very close.
bool NeedsMonogramOutline(SkColor monogram_background,
                          SkColor surrounding_color);

// Returns the brand colour for a site: the dominant colour of its `favicon` if
// there is one, otherwise its `theme_color` (from <meta name="theme-color">),
// otherwise nullopt.
std::optional<SkColor> GetSiteColor(const gfx::Image& favicon,
                                    std::optional<SkColor> theme_color);

// Returns the character shown in a monogram for `name`: the first letter,
// number, or symbol (excluding ignorable characters) in `name`, upper-cased, or
// an empty string if `name` has none.
std::u16string GetMonogramText(std::u16string_view name);

// Returns the character shown in `credential`'s monogram. It is derived from
// a passkey's display name if non-empty, otherwise from the username.
std::u16string GetMonogramTextForCredential(
    const ambient_signin::PasskeyOrPasswordCredential& credential);

}  // namespace internal

}  // namespace ambient_login_credential_icon

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_AMBIENT_LOGIN_CREDENTIAL_ICON_H_
