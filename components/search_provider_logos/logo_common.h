// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_PROVIDER_LOGOS_LOGO_COMMON_H_
#define COMPONENTS_SEARCH_PROVIDER_LOGOS_LOGO_COMMON_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace search_provider_logos {

// The maximum number of milliseconds that a logo can be cached.
extern const int64_t kMaxTimeToLiveMS;

enum class LogoType {
  SIMPLE,
  ANIMATED,
  INTERACTIVE,
};

// Note: whenever a new field is added here, LogoCache must be updated to
// serialize and deserialize that field.
struct LogoMetadata {
  LogoMetadata();
  LogoMetadata(const LogoMetadata&);
  LogoMetadata(LogoMetadata&&) noexcept;
  LogoMetadata& operator=(const LogoMetadata&);
  LogoMetadata& operator=(LogoMetadata&&) noexcept;
  ~LogoMetadata();

  // For use by the client ----------------------------------------------------

  // SIMPLE, ANIMATED, or INTERACTIVE.
  LogoType type = LogoType::SIMPLE;

  // SIMPLE, ANIMATED: The URL to load when the logo is clicked.
  // INTERACTIVE: Unused, on Desktop. Same as SIMPLE/ANIMATED on Mobile.
  GURL on_click_url;

  // INTERACTIVE: The URL of a full-page version of the logo. On Desktop, logo
  // is embedded into an <iframe> on the NTP.
  // SIMPLE, ANIMATED: not used.
  GURL full_page_url;

  // SIMPLE: The accessibility text for the logo.
  // ANIMATED: The accessibility text for the CTA and animated logos.
  // INTERACTIVE: The accessibility text for the iframe, on Desktop.
  std::string alt_text;

  // SIMPLE: The mime type of the logo image.
  // ANIMATED: The mime type of the CTA image.
  std::string mime_type;

  // SIMPLE: The mime type of the dark logo image. May be empty.
  // ANIMATED: The mime type of the dark CTA image. May be empty.
  std::string dark_mime_type;

  // SIMPLE, ANIMATED: The background color to use in dark mode.
  // INTERACTIVE: not used.
  std::string dark_background_color;

  // ANIMATED: The URL for an animated image to display when the call to action
  // logo is clicked. If |animated_url| is not empty, |encoded_image| refers to
  // a call to action image.
  // SIMPLE, INTERACTIVE: not used.
  GURL animated_url;
  GURL dark_animated_url;

  // The URL to ping when the CTA image is clicked. May be empty.
  GURL cta_log_url;
  // The URL to ping when the main image is clicked (i.e. the animated image if
  // there is one, or the only image otherwise). May be empty.
  GURL log_url;

  // The URL used for sharing doodles.
  GURL short_link;

  // SIMPLE, ANIMATED: ignored
  // INTERACTIVE: appropriate dimensions for the iframe.
  int iframe_width_px = 0;
  int iframe_height_px = 0;

  // For use by LogoService ---------------------------------------------------

  // The URL from which the logo was downloaded (without the fingerprint param).
  GURL source_url;
  // A fingerprint (i.e. hash) identifying the logo. Used when revalidating the
  // logo with the server.
  std::string fingerprint;
  // Whether the logo can be shown optimistically after it's expired while a
  // fresh logo is being downloaded.
  bool can_show_after_expiration = false;
  // When the logo expires. After this time, the logo will not be used and will
  // be deleted.
  base::Time expiration_time;

  // Used by the Optional Doodle Share Button ---------------------------------

  // Share button x position
  int share_button_x = -1;
  int dark_share_button_x = -1;

  // Share button y position
  int share_button_y = -1;
  int dark_share_button_y = -1;

  // Share button opacity
  double share_button_opacity = 0;
  double dark_share_button_opacity = 0;

  // Share button icon image, uses Data URI format.
  std::string share_button_icon;
  std::string dark_share_button_icon;

  // Share button background color, uses hex format.
  std::string share_button_bg;
  std::string dark_share_button_bg;
};

enum class LogoCallbackReason {
  // The default search engine does not support logos.
  // |logo| is nullopt. No logo should be displayed.
  DISABLED,

  // The logo was successfully determined.
  // If |logo| is non-nullopt, it should be displayed. If nullopt, then any
  // visible logo should be cleared.
  DETERMINED,

  // The fresh logo is the same as the cached logo. Only used for fresh logos.
  // |logo| is non-nullopt, and the cached logo should be kept.
  REVALIDATED,

  // The default search engine could not be contacted, or provided invalid logo
  // data. Only used for fresh logos.
  // |logo| is non-nullopt, and the cached logo should be kept.
  FAILED,

  // The default search engine was changed while fetching the logo.
  // |logo| is nullopt. No logo should be displayed.
  CANCELED,
};

struct EncodedLogo {
  EncodedLogo();
  EncodedLogo(const EncodedLogo&);
  EncodedLogo(EncodedLogo&&) noexcept;
  EncodedLogo& operator=(const EncodedLogo& other);
  EncodedLogo& operator=(EncodedLogo&& other) noexcept;
  ~EncodedLogo();

  // The jpeg- or png-encoded image.
  scoped_refptr<base::RefCountedString> encoded_image;
  // The jpeg- or png-encoded dark image. May be null.
  scoped_refptr<base::RefCountedString> dark_encoded_image;
  // Metadata about the logo.
  LogoMetadata metadata;
};
using EncodedLogoCallback =
    base::OnceCallback<void(LogoCallbackReason type,
                            const base::Optional<EncodedLogo>& logo)>;

struct Logo {
  Logo();
  ~Logo();

  // The light mode logo image.
  SkBitmap image;
  // The dark mode logo image.
  SkBitmap dark_image;
  // Metadata about the logo.
  LogoMetadata metadata;
};
using LogoCallback = base::OnceCallback<void(LogoCallbackReason type,
                                             const base::Optional<Logo>& logo)>;

struct LogoCallbacks {
  EncodedLogoCallback on_cached_encoded_logo_available;
  LogoCallback on_cached_decoded_logo_available;
  EncodedLogoCallback on_fresh_encoded_logo_available;
  LogoCallback on_fresh_decoded_logo_available;

  LogoCallbacks();
  LogoCallbacks(LogoCallbacks&&) noexcept;
  LogoCallbacks& operator=(LogoCallbacks&&) noexcept;
  ~LogoCallbacks();
};

// Parses the response from the server and returns it as an EncodedLogo. Returns
// null if the response is invalid.
using ParseLogoResponse = base::Callback<std::unique_ptr<EncodedLogo>(
    std::unique_ptr<std::string> response,
    base::Time response_time,
    bool* parsing_failed)>;

// Encodes the fingerprint of the cached logo in the logo URL. This enables the
// server to verify whether the cached logo is up to date.
using AppendQueryparamsToLogoURL =
    base::Callback<GURL(const GURL& logo_url, const std::string& fingerprint)>;

}  // namespace search_provider_logos

#endif  // COMPONENTS_SEARCH_PROVIDER_LOGOS_LOGO_COMMON_H_
