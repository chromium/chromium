// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_evaluator.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "components/security_state/core/security_state.h"
#include "components/webapps/browser/webapps_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"

namespace webapps {

namespace {

using IconPurpose = blink::mojom::ManifestImageResource_Purpose;

// This constant is the icon size on Android (48dp) multiplied by the scale
// factor of a Nexus 5 device (3x). It is the currently advertised minimum icon
// size for triggering banners.
const int kMinimumPrimaryIconSizeInPx = 144;

struct ImageTypeDetails {
  const char* extension;
  const char* mimetype;
};

constexpr ImageTypeDetails kSupportedImageTypes[] = {
    {".png", "image/png"},
    {".svg", "image/svg+xml"},
    {".webp", "image/webp"},
};

bool HasValidStartUrl(const blink::mojom::Manifest& manifest,
                      const mojom::WebPageMetadata& metadata,
                      InstallableCriteria criteria) {
  if (manifest.start_url.is_valid()) {
    // If the start_url is valid, the id must be valid.
    CHECK(manifest.id.is_valid());
    return true;
  }
  return criteria == InstallableCriteria::kImplicitManifestFieldsHTML &&
         metadata.application_url.is_valid();
}

bool IsManifestNameValid(const blink::mojom::Manifest& manifest) {
  return (manifest.name && !manifest.name->empty()) ||
         (manifest.short_name && !manifest.short_name->empty());
}

bool IsWebPageMetadataContainValidName(const mojom::WebPageMetadata& metadata) {
  return !metadata.application_name.empty() || !metadata.title.empty();
}

bool HasValidName(const blink::mojom::Manifest& manifest,
                  const mojom::WebPageMetadata& metadata,
                  InstallableCriteria criteria) {
  if (IsManifestNameValid(manifest)) {
    return true;
  }
  return (criteria == InstallableCriteria::kImplicitManifestFieldsHTML &&
          IsWebPageMetadataContainValidName(metadata));
}

bool IsIconTypeSupported(const blink::Manifest::ImageResource& icon) {
  // The type field is optional. If it isn't present, fall back on checking
  // the src extension.
  if (icon.type.empty()) {
    std::string filename = icon.src.ExtractFileName();
    for (const ImageTypeDetails& details : kSupportedImageTypes) {
      if (base::EndsWith(filename, details.extension,
                         base::CompareCase::INSENSITIVE_ASCII)) {
        return true;
      }
    }
    return false;
  }

  for (const ImageTypeDetails& details : kSupportedImageTypes) {
    if (base::EqualsASCII(icon.type, details.mimetype)) {
      return true;
    }
  }
  return false;
}

// Returns whether |manifest| specifies an SVG or PNG icon that has
// IconPurpose::ANY, with size >= kMinimumPrimaryIconSizeInPx (or size "any").
bool DoesManifestContainRequiredIcon(const blink::mojom::Manifest& manifest) {
  for (const auto& icon : manifest.icons) {
    if (!IsIconTypeSupported(icon)) {
      continue;
    }

    if (!base::Contains(icon.purpose, IconPurpose::ANY)) {
      continue;
    }

    for (const auto& size : icon.sizes) {
      if (size.IsEmpty()) {  // "any"
        return true;
      }
      if (size.width() >= InstallableEvaluator::GetMinimumIconSizeInPx() &&
          size.height() >= InstallableEvaluator::GetMinimumIconSizeInPx() &&
          size.width() <= InstallableEvaluator::kMaximumIconSizeInPx &&
          size.height() <= InstallableEvaluator::kMaximumIconSizeInPx) {
        return true;
      }
    }
  }

  return false;
}

bool HasNonDefaultFavicon(content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }
  for (const auto& favicon_url : web_contents->GetFaviconURLs()) {
    if (!favicon_url->is_default_icon) {
      return true;
    }
  }
  return false;
}

bool HasValidIcon(content::WebContents* web_contents,
                  const blink::mojom::Manifest& manifest,
                  InstallableCriteria criteria) {
  if (DoesManifestContainRequiredIcon(manifest)) {
    return true;
  }
  return (criteria == InstallableCriteria::kImplicitManifestFieldsHTML &&
          HasNonDefaultFavicon(web_contents));
}

bool ShouldRejectDisplayMode(blink::mojom::DisplayMode display_mode,
                             InstallableCriteria criteria) {
  if (criteria == InstallableCriteria::kImplicitManifestFieldsHTML) {
    return display_mode == blink::mojom::DisplayMode::kBrowser;
  }
  return !(
      display_mode == blink::mojom::DisplayMode::kStandalone ||
      display_mode == blink::mojom::DisplayMode::kFullscreen ||
      display_mode == blink::mojom::DisplayMode::kMinimalUi ||
      display_mode == blink::mojom::DisplayMode::kWindowControlsOverlay ||
      (display_mode == blink::mojom::DisplayMode::kBorderless &&
       base::FeatureList::IsEnabled(blink::features::kWebAppBorderless)) ||
      (display_mode == blink::mojom::DisplayMode::kTabbed &&
       base::FeatureList::IsEnabled(blink::features::kDesktopPWAsTabStrip)));
}

}  // namespace

InstallableEvaluator::InstallableEvaluator(content::WebContents* web_contents,
                                           const InstallablePageData& data,
                                           InstallableCriteria criteria,
                                           bool check_display)
    : web_contents_(web_contents->GetWeakPtr()),
      page_data_(data),
      criteria_(criteria),
      check_display_(check_display) {}

InstallableEvaluator::~InstallableEvaluator() = default;

// static
int InstallableEvaluator::GetMinimumIconSizeInPx() {
  return kMinimumPrimaryIconSizeInPx;
}

absl::optional<std::vector<InstallableStatusCode>>
InstallableEvaluator::CheckInstallability() const {
  if (criteria_ == InstallableCriteria::kDoNotCheck) {
    return absl::nullopt;
  }

  std::vector<InstallableStatusCode> errors;
  if (blink::IsEmptyManifest(page_data_->GetManifest())) {
    errors.push_back(MANIFEST_EMPTY);
    return errors;
  }

  if (!HasValidStartUrl(page_data_->GetManifest(),
                        page_data_->WebPageMetadata(), criteria_)) {
    errors.push_back(START_URL_NOT_VALID);
  }

  if (!HasValidName(page_data_->GetManifest(), page_data_->WebPageMetadata(),
                    criteria_)) {
    errors.push_back(MANIFEST_MISSING_NAME_OR_SHORT_NAME);
  }

  if (check_display_) {
    blink::mojom::DisplayMode display_mode_to_evaluate =
        page_data_->GetManifest().display;
    InstallableStatusCode manifest_error = MANIFEST_DISPLAY_NOT_SUPPORTED;

    // Unsupported values are ignored when we parse the manifest, and
    // consequently aren't in the manifest.display_override array.
    // If this array is not empty, the first value will "win", so validate
    // this value is installable.
    if (!page_data_->GetManifest().display_override.empty()) {
      display_mode_to_evaluate = page_data_->GetManifest().display_override[0];
      manifest_error = MANIFEST_DISPLAY_OVERRIDE_NOT_SUPPORTED;
    }

    if (ShouldRejectDisplayMode(display_mode_to_evaluate, criteria_)) {
      errors.push_back(manifest_error);
    }
  }

  if (!HasValidIcon(web_contents_.get(), page_data_->GetManifest(),
                    criteria_)) {
    errors.push_back(MANIFEST_MISSING_SUITABLE_ICON);
  }

  return errors;
}

std::vector<InstallableStatusCode> InstallableEvaluator::CheckEligiblity(
    content::WebContents* web_contents) const {
  std::vector<InstallableStatusCode> errors;
  if (web_contents->GetBrowserContext()->IsOffTheRecord()) {
    errors.push_back(IN_INCOGNITO);
  }
  if (!IsContentSecure(web_contents)) {
    errors.push_back(NOT_FROM_SECURE_ORIGIN);
  }
  return errors;
}

// static
bool InstallableEvaluator::IsContentSecure(content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }

  // chrome:// URLs are considered secure.
  const GURL& url = web_contents->GetLastCommittedURL();
  if (url.scheme() == content::kChromeUIScheme) {
    return true;
  }

  // chrome-untrusted:// URLs are shipped with Chrome, so they are considered
  // secure in this context.
  if (url.scheme() == content::kChromeUIUntrustedScheme) {
    return true;
  }

  if (IsOriginConsideredSecure(url)) {
    return true;
  }

  // This can be null in unit tests but should be non-null in production.
  if (!webapps::WebappsClient::Get()) {
    return false;
  }

  return security_state::IsSslCertificateValid(
      WebappsClient::Get()->GetSecurityLevelForWebContents(web_contents));
}

// static
bool InstallableEvaluator::IsOriginConsideredSecure(const GURL& url) {
  auto origin = url::Origin::Create(url);
  auto* webapps_client = webapps::WebappsClient::Get();
  return (webapps_client && webapps_client->IsOriginConsideredSecure(origin)) ||
         net::IsLocalhost(url) ||
         network::SecureOriginAllowlist::GetInstance().IsOriginAllowlisted(
             origin);
}

}  // namespace webapps
