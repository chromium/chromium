// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/google_logo_api.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/google/core/common/google_util.h"
#include "components/search_provider_logos/switches.h"
#include "net/base/data_url.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"

namespace search_provider_logos {

namespace {

const int kDefaultIframeWidthPx = 500;
const int kDefaultIframeHeightPx = 200;

}  // namespace

GURL GetGoogleDoodleURL(const GURL& google_base_url) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kGoogleDoodleUrl)) {
    return GURL(command_line->GetSwitchValueASCII(switches::kGoogleDoodleUrl));
  }

  GURL::Replacements replacements;
  replacements.SetPathStr("async/ddljson");
  // Make sure we use https rather than http (except for .cn).
  if (google_base_url.SchemeIs(url::kHttpScheme) &&
      !base::EndsWith(google_base_url.host(), ".cn",
                      base::CompareCase::INSENSITIVE_ASCII)) {
    replacements.SetSchemeStr(url::kHttpsScheme);
  }
  return google_base_url.ReplaceComponents(replacements);
}

GURL AppendFingerprintParamToDoodleURL(const GURL& logo_url,
                                       const std::string& fingerprint) {
  if (fingerprint.empty()) {
    return logo_url;
  }

  return google_util::AppendToAsyncQueryParam(logo_url, "es_dfp", fingerprint);
}

GURL AppendPreliminaryParamsToDoodleURL(bool gray_background,
                                        bool for_webui_ntp,
                                        bool enable_animated_logo,
                                        const GURL& logo_url) {
  auto url = google_util::AppendToAsyncQueryParam(logo_url, "ntp",
                                                  for_webui_ntp ? "2" : "1");
  if (gray_background) {
    url = google_util::AppendToAsyncQueryParam(url, "graybg", "1");
  }
  if (enable_animated_logo) {
    url = google_util::AppendToAsyncQueryParam(url, "anim", "1");
  }
  return url;
}

namespace {
const char kResponsePreamble[] = ")]}'";

GURL ParseUrl(const base::DictValue& parent_dict,
              const std::string& key,
              const GURL& base_url) {
  const std::string* url_str = parent_dict.FindString(key);
  if (!url_str || url_str->empty()) {
    return GURL();
  }
  GURL result = base_url.Resolve(*url_str);
  // If the base URL is https:// (which should almost always be the case, see
  // above), then we require all other URLs to be https:// too.
  if (base_url.SchemeIs(url::kHttpsScheme) &&
      !result.SchemeIs(url::kHttpsScheme)) {
    return GURL();
  }
  return result;
}

// On success returns a pair of <mime_type, data>.
// On error returns a pair of <string, nullptr>.
// mime_type should be ignored if data is nullptr.
std::pair<std::string, scoped_refptr<base::RefCountedString>>
ParseEncodedImageData(const std::string& encoded_image_data) {
  std::pair<std::string, scoped_refptr<base::RefCountedString>> result;

  GURL encoded_image_uri(encoded_image_data);

  if (!encoded_image_uri.is_valid() ||
      !encoded_image_uri.SchemeIs(url::kDataScheme)) {
    return result;
  }

  std::string mime_type;
  std::string charset;
  std::string decoded_data;
  if (!net::DataURL::Parse(encoded_image_uri, &mime_type, &charset,
                           &decoded_data)) {
    return result;
  }

  result.first = std::move(mime_type);
  result.second =
      base::MakeRefCounted<base::RefCountedString>(std::move(decoded_data));
  return result;
}

/**
 * Helper function to parse the mural metadata information from the Google
 * Doodle Logo response. The Mural image metadata is a non-optional field, and
 * comes in both light and dark variations.
 *
 * Murals are the extended artwork images of the Google Doodles, and can serve
 * as a replacement for the latter on larger surfaces. Unlike Doodles, Mural
 * images are served as urls rather than base64 encoded data.
 */
void ParseMuralMetadata(const base::DictValue* mural,
                        const GURL& base_url,
                        bool is_dark,
                        MuralMetadata& mural_metadata) {
  // While the mural image metadata is non-optional, it is not required to
  // render the Doodle. Users can always fallback to regular the Doodle image.
  if (!mural) {
    DLOG(WARNING)
        << (is_dark ? "Dark " : "")
        << "Mural Image metadata is missing when parsing the Logo response";
    return;
  }
  mural_metadata.mural_url = ParseUrl(*mural, "url", base_url);
  mural_metadata.is_animated_gif =
      mural->FindBool("is_animated_gif").value_or(false);
  mural_metadata.width_px = mural->FindInt("width").value_or(0);
  mural_metadata.height_px = mural->FindInt("height").value_or(0);

  // Similarly, the core content area is non-optional as well, but it is not
  // required to render the Mural itself.
  const base::DictValue* core_content_area =
      mural->FindDict("core_content_area");
  if (!core_content_area) {
    DLOG(WARNING)
        << (is_dark ? "Dark " : "")
        << "Mural Core Content Area is missing when parsing the Logo response";
  } else {
    mural_metadata.core_content_area.height_px =
        core_content_area->FindInt("height").value_or(0);
    mural_metadata.core_content_area.left_px =
        core_content_area->FindInt("left").value_or(0);
    mural_metadata.core_content_area.top_px =
        core_content_area->FindInt("top").value_or(0);
    mural_metadata.core_content_area.width_px =
        core_content_area->FindInt("width").value_or(0);
  }
}

}  // namespace

std::unique_ptr<EncodedLogo> ParseDoodleLogoResponse(const GURL& base_url,
                                                     std::string response,
                                                     base::Time response_time,
                                                     bool* parsing_failed) {
  // The response may start with )]}'. Ignore this.
  std::string_view response_sp =
      base::RemovePrefix(response, kResponsePreamble).value_or(response);

  // Default parsing failure to be true.
  *parsing_failed = true;

  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      response_sp, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!parsed_json.has_value()) {
    LOG(WARNING) << parsed_json.error().message << " at "
                 << parsed_json.error().line << ":"
                 << parsed_json.error().column;
    return nullptr;
  }

  if (!parsed_json->is_dict()) {
    return nullptr;
  }

  const base::DictValue* ddljson = parsed_json->GetDict().FindDict("ddljson");
  if (!ddljson) {
    return nullptr;
  }

  // If there is no logo today, the "ddljson" dictionary will be empty.
  if (ddljson->empty()) {
    *parsing_failed = false;
    return nullptr;
  }

  auto logo = std::make_unique<EncodedLogo>();

  // Doodle server team recommends to not rely on the doodle_type metadata
  // field as they are trying to deprecate it. Instead, we should check
  // large_image.is_animated_gif for animated doodles.
  const std::string* doodle_type = ddljson->FindString("doodle_type");
  const bool is_animated_gif =
      ddljson->FindBoolByDottedPath("large_image.is_animated_gif")
          .value_or(false);

  logo->metadata.type = LogoType::SIMPLE;
  if (is_animated_gif) {
    logo->metadata.type = LogoType::ANIMATED;
  } else if (doodle_type) {
    if (*doodle_type == "INTERACTIVE") {
      logo->metadata.type = LogoType::INTERACTIVE;
    } else if (*doodle_type == "VIDEO") {
      logo->metadata.type = LogoType::INTERACTIVE;
    }
  }

  const bool is_simple = (logo->metadata.type == LogoType::SIMPLE);
  const bool is_animated = (logo->metadata.type == LogoType::ANIMATED);
  bool is_interactive = (logo->metadata.type == LogoType::INTERACTIVE);

  // Check if the main image is animated.
  if (is_animated) {
    // If animated, get the URL for the animated image.
    const base::DictValue* image = ddljson->FindDict("large_image");
    if (!image) {
      return nullptr;
    }
    logo->metadata.animated_url = ParseUrl(*image, "url", base_url);
    if (!logo->metadata.animated_url.is_valid()) {
      return nullptr;
    }

    const base::DictValue* dark_image = ddljson->FindDict("dark_large_image");
    if (dark_image) {
      logo->metadata.dark_animated_url = ParseUrl(*dark_image, "url", base_url);
    }
  }

  if (is_simple || is_animated) {
    const base::DictValue* image = ddljson->FindDict("large_image");
    if (image) {
      if (std::optional<int> width_px = image->FindInt("width")) {
        logo->metadata.width_px = *width_px;
      }
      if (std::optional<int> height_px = image->FindInt("height")) {
        logo->metadata.height_px = *height_px;
      }
    }

    const base::DictValue* dark_image = ddljson->FindDict("dark_large_image");
    if (dark_image) {
      if (const std::string* background_color =
              dark_image->FindString("background_color")) {
        logo->metadata.dark_background_color = *background_color;
      }
      if (std::optional<int> width_px = dark_image->FindInt("width")) {
        logo->metadata.dark_width_px = *width_px;
      }
      if (std::optional<int> height_px = dark_image->FindInt("height")) {
        logo->metadata.dark_height_px = *height_px;
      }
    }
  }

  const bool is_eligible_for_share_button = is_simple || is_animated;

  if (is_eligible_for_share_button) {
    const std::string* short_link_ptr = ddljson->FindString("short_link");
    // The short link in the doodle proto is an incomplete URL with the format
    // //g.co/*, //doodle.gle/* or //google.com?doodle=*.
    // Complete the URL if possible.
    if (short_link_ptr && short_link_ptr->find("//") == 0) {
      std::string short_link_str = *short_link_ptr;
      short_link_str.insert(0, "https:");
      logo->metadata.short_link = GURL(std::move(short_link_str));
    }
  }

  // Extract the Doodle Mural information.
  if (is_simple || is_animated) {
    ParseMuralMetadata(ddljson->FindDict("mural_image"), base_url,
                       /*is_dark=*/false, logo->metadata.mural_metadata);
    ParseMuralMetadata(ddljson->FindDict("dark_mural_image"), base_url,
                       /*is_dark=*/true, logo->metadata.dark_mural_metadata);
  }

  logo->metadata.full_page_url =
      ParseUrl(*ddljson, "fullpage_interactive_url", base_url);

  // Data is optional, since we may be revalidating a cached logo.
  // If there is a CTA image, get that; otherwise use the regular image.
  const std::string* encoded_image_data = ddljson->FindString("cta_data_uri");
  if (!encoded_image_data) {
    encoded_image_data = ddljson->FindString("data_uri");
  }
  if (encoded_image_data) {
    auto [mime_type, data] = ParseEncodedImageData(*encoded_image_data);
    if (!data) {
      return nullptr;
    }
    logo->metadata.mime_type = mime_type;
    logo->encoded_image = data;
  }

  const std::string* dark_encoded_image_data =
      ddljson->FindString("dark_cta_data_uri");
  if (!dark_encoded_image_data) {
    dark_encoded_image_data = ddljson->FindString("dark_data_uri");
  }
  if (dark_encoded_image_data) {
    auto [mime_type, data] = ParseEncodedImageData(*dark_encoded_image_data);

    if (data) {
      logo->metadata.dark_mime_type = mime_type;
    }
    logo->dark_encoded_image = data;
  }

  logo->metadata.on_click_url = ParseUrl(*ddljson, "target_url", base_url);
  if (const std::string* alt_text = ddljson->FindString("alt_text")) {
    logo->metadata.alt_text = *alt_text;
  }

  logo->metadata.cta_log_url = ParseUrl(*ddljson, "cta_log_url", base_url);
  logo->metadata.dark_cta_log_url =
      ParseUrl(*ddljson, "dark_cta_log_url", base_url);
  logo->metadata.log_url = ParseUrl(*ddljson, "log_url", base_url);
  logo->metadata.dark_log_url = ParseUrl(*ddljson, "dark_log_url", base_url);

  if (const std::string* fingerprint = ddljson->FindString("fingerprint")) {
    logo->metadata.fingerprint = *fingerprint;
  }

  if (is_interactive) {
    const std::string* behavior =
        ddljson->FindString("launch_interactive_behavior");
    if (behavior && (*behavior == "NEW_WINDOW")) {
      logo->metadata.type = LogoType::SIMPLE;
      logo->metadata.on_click_url = logo->metadata.full_page_url;
      is_interactive = false;
    }
  }

  logo->metadata.iframe_width_px = 0;
  logo->metadata.iframe_height_px = 0;
  if (is_interactive) {
    logo->metadata.iframe_width_px =
        ddljson->FindInt("iframe_width_px").value_or(kDefaultIframeWidthPx);
    logo->metadata.iframe_height_px =
        ddljson->FindInt("iframe_height_px").value_or(kDefaultIframeHeightPx);
  }

  base::TimeDelta time_to_live;
  // The JSON doesn't guarantee the number to fit into an int.
  if (std::optional<double> ttl_ms = ddljson->FindDouble("time_to_live_ms")) {
    time_to_live = base::Milliseconds(*ttl_ms);
    logo->metadata.can_show_after_expiration = false;
  } else {
    time_to_live = base::Milliseconds(kMaxTimeToLiveMS);
    logo->metadata.can_show_after_expiration = true;
  }
  logo->metadata.expiration_time = response_time + time_to_live;

  *parsing_failed = false;
  return logo;
}

}  // namespace search_provider_logos
