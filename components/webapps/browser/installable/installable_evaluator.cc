// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_evaluator.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/types/optional_ref.h"
#include "components/security_state/core/security_state.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/webapps_client.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace webapps {

namespace {

using IconPurpose = blink::mojom::ManifestImageResource_Purpose;

// This constant is the icon size on Android (48dp) multiplied by the scale
// factor of a Nexus 5 device (3x). It is the currently advertised minimum icon
// size for triggering banners.
const int kMinimumPrimaryIconSizeInPx = 144;

InstallableStatusCode HasManifestOrAtRootScope(
    InstallableCriteria criteria,
    base::optional_ref<const blink::Manifest> manifest,
    const GURL& manifest_url,
    const GURL& site_url) {
  switch (criteria) {
    case InstallableCriteria::kDoNotCheck:
      return InstallableStatusCode::NO_ERROR_DETECTED;
    case InstallableCriteria::kNoManifestAtRootScope:
      if (site_url.GetWithoutFilename().GetPath().length() <= 1) {
        return InstallableStatusCode::NO_ERROR_DETECTED;
      }
      break;
    case InstallableCriteria::kImplicitManifestFieldsHTML:
    case InstallableCriteria::kValidManifestIgnoreDisplay:
    case InstallableCriteria::kValidManifestWithIcons:
      break;
  }
  // This occurs when there is an error parsing the manifest, a network issue,
  // or a CORS / opaque origin issue.
  if (!manifest) {
    return InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR;
  }

  if (manifest_url.is_empty()) {
    return InstallableStatusCode::NO_MANIFEST;
  }

  return InstallableStatusCode::NO_ERROR_DETECTED;
}

bool HasValidStartUrl(base::optional_ref<const blink::Manifest> manifest,
                      const mojom::WebPageMetadata& metadata,
                      const GURL& site_url,
                      InstallableCriteria criteria) {
  if (manifest) {
    // Since the id is generated from the start_url, both are valid.
    CHECK(manifest->start_url().is_valid() && manifest->id().is_valid());
  }
  switch (criteria) {
    case InstallableCriteria::kValidManifestIgnoreDisplay:
    case InstallableCriteria::kValidManifestWithIcons:
      return manifest && manifest->has_valid_specified_start_url();
    case InstallableCriteria::kDoNotCheck:
      return true;
    case InstallableCriteria::kImplicitManifestFieldsHTML:
      return manifest || metadata.application_url.is_valid();
    case InstallableCriteria::kNoManifestAtRootScope:
      return manifest || metadata.application_url.is_valid() ||
             site_url.GetWithoutFilename().GetPath().length() <= 1;
  }
}

bool IsManifestNameValid(const blink::Manifest& manifest) {
  return (manifest.name() && !manifest.name()->empty()) ||
         (manifest.short_name() && !manifest.short_name()->empty());
}

bool IsWebPageMetadataContainValidName(const mojom::WebPageMetadata& metadata) {
  return !metadata.application_name.empty() || !metadata.title.empty();
}

bool HasValidName(base::optional_ref<const blink::Manifest> manifest,
                  const mojom::WebPageMetadata& metadata,
                  InstallableCriteria criteria) {
  switch (criteria) {
    case InstallableCriteria::kDoNotCheck:
      return true;
    case InstallableCriteria::kValidManifestWithIcons:
    case InstallableCriteria::kValidManifestIgnoreDisplay:
      return manifest && IsManifestNameValid(*manifest);
    case InstallableCriteria::kImplicitManifestFieldsHTML:
    case InstallableCriteria::kNoManifestAtRootScope:
      return (manifest && IsManifestNameValid(*manifest)) ||
             IsWebPageMetadataContainValidName(metadata);
  }
}

// Returns whether |manifest| specifies a supported icon that has
// IconPurpose::ANY, with size >= kMinimumPrimaryIconSizeInPx (or size "any"),
// and is square.
bool DoesManifestContainRequiredIcon(const blink::Manifest& manifest) {
  blink::ManifestIconSelectorParams params;
  params.purpose = IconPurpose::ANY;
  params.minimum_icon_size_in_px =
      InstallableEvaluator::GetMinimumIconSizeInPx();
  params.maximum_icon_size_in_px = InstallableEvaluator::kMaximumIconSizeInPx;
  params.max_width_to_height_ratio = 1.0f;
  params.limited_image_types_for_installable_icon = true;
  return blink::ManifestIconSelector::FindBestMatchingIcon(manifest.icons(),
                                                           params)
      .has_value();
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
                  base::optional_ref<const blink::Manifest> manifest,
                  InstallableCriteria criteria) {
  switch (criteria) {
    case webapps::InstallableCriteria::kDoNotCheck:
      return true;
    case webapps::InstallableCriteria::kValidManifestWithIcons:
    case webapps::InstallableCriteria::kValidManifestIgnoreDisplay:
      return manifest && DoesManifestContainRequiredIcon(*manifest);
    case webapps::InstallableCriteria::kImplicitManifestFieldsHTML:
    case webapps::InstallableCriteria::kNoManifestAtRootScope:
      return (manifest && DoesManifestContainRequiredIcon(*manifest)) ||
             HasNonDefaultFavicon(web_contents);
  }
}

bool IsInstallableDisplayMode(blink::mojom::DisplayMode display_mode) {
  // Note: The 'enabling' of these display modes is checked in the
  // manifest_parser.cc, as that contains checks for origin trials etc.
  return display_mode == blink::mojom::DisplayMode::kStandalone ||
         display_mode == blink::mojom::DisplayMode::kFullscreen ||
         display_mode == blink::mojom::DisplayMode::kMinimalUi ||
         display_mode == blink::mojom::DisplayMode::kWindowControlsOverlay ||
         display_mode == blink::mojom::DisplayMode::kUnframed ||
         display_mode == blink::mojom::DisplayMode::kTabbed;
}

}  // namespace

InstallableStatusCode InstallableEvaluator::GetDisplayError(
    const blink::Manifest& manifest,
    InstallableCriteria criteria) {
  blink::mojom::DisplayMode display_mode_to_evaluate = manifest.display();
  InstallableStatusCode error_type_if_invalid =
      InstallableStatusCode::MANIFEST_DISPLAY_NOT_SUPPORTED;

  // Unsupported values are ignored when we parse the manifest, and
  // consequently aren't in the manifest.display_override array.
  // If this array is not empty, the first value will "win", so validate
  // this value is installable.
  if (!manifest.display_override().empty()) {
    display_mode_to_evaluate = manifest.display_override()[0].display();
    error_type_if_invalid =
        InstallableStatusCode::MANIFEST_DISPLAY_OVERRIDE_NOT_SUPPORTED;
  }

  switch (criteria) {
    case InstallableCriteria::kValidManifestWithIcons:
      if (!IsInstallableDisplayMode(display_mode_to_evaluate)) {
        return error_type_if_invalid;
      }
      break;
    case InstallableCriteria::kImplicitManifestFieldsHTML:
    case InstallableCriteria::kNoManifestAtRootScope:
      if (display_mode_to_evaluate == blink::mojom::DisplayMode::kBrowser) {
        return error_type_if_invalid;
      }
      break;
    case InstallableCriteria::kValidManifestIgnoreDisplay:
      break;
    case InstallableCriteria::kDoNotCheck:
      NOTREACHED();
  }
  return InstallableStatusCode::NO_ERROR_DETECTED;
}

InstallableStatusCode InstallableEvaluator::GetDisplayError(
    const blink::mojom::Manifest& manifest,
    InstallableCriteria criteria) {
  blink::mojom::DisplayMode display_mode_to_evaluate = manifest.display;
  InstallableStatusCode error_type_if_invalid =
      InstallableStatusCode::MANIFEST_DISPLAY_NOT_SUPPORTED;

  if (!manifest.display_override.empty()) {
    display_mode_to_evaluate = manifest.display_override[0].display();
    error_type_if_invalid =
        InstallableStatusCode::MANIFEST_DISPLAY_OVERRIDE_NOT_SUPPORTED;
  }

  switch (criteria) {
    case InstallableCriteria::kValidManifestWithIcons:
      if (!IsInstallableDisplayMode(display_mode_to_evaluate)) {
        return error_type_if_invalid;
      }
      break;
    case InstallableCriteria::kImplicitManifestFieldsHTML:
    case InstallableCriteria::kNoManifestAtRootScope:
      if (display_mode_to_evaluate == blink::mojom::DisplayMode::kBrowser) {
        return error_type_if_invalid;
      }
      break;
    case InstallableCriteria::kValidManifestIgnoreDisplay:
      break;
    case InstallableCriteria::kDoNotCheck:
      NOTREACHED();
  }
  return InstallableStatusCode::NO_ERROR_DETECTED;
}

InstallableEvaluator::InstallableEvaluator(content::WebContents* web_contents,
                                           const InstallablePageData& data,
                                           InstallableCriteria criteria)
    : web_contents_(web_contents->GetWeakPtr()),
      page_data_(data),
      criteria_(criteria) {}

InstallableEvaluator::~InstallableEvaluator() = default;

// static
int InstallableEvaluator::GetMinimumIconSizeInPx() {
  return kMinimumPrimaryIconSizeInPx;
}

std::vector<InstallableStatusCode> InstallableEvaluator::CheckInstallability()
    const {
  base::optional_ref<const blink::Manifest> manifest =
      page_data_->GetManifest();
  CHECK(!manifest ||
        (manifest->start_url().is_valid() && manifest->scope().is_valid() &&
         manifest->id().is_valid()));

  std::vector<InstallableStatusCode> errors;
  if (criteria_ == InstallableCriteria::kDoNotCheck) {
    return errors;
  }

  InstallableStatusCode error =
      HasManifestOrAtRootScope(criteria_, manifest, page_data_->manifest_url(),
                               web_contents_->GetLastCommittedURL());
  if (error != InstallableStatusCode::NO_ERROR_DETECTED) {
    errors.push_back(error);
    return errors;
  }

  if (!HasValidStartUrl(manifest, page_data_->WebPageMetadata(),
                        web_contents_->GetLastCommittedURL(), criteria_)) {
    errors.push_back(InstallableStatusCode::START_URL_NOT_VALID);
  }

  if (!HasValidName(manifest, page_data_->WebPageMetadata(), criteria_)) {
    errors.push_back(
        InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME);
  }

  if (manifest) {
    InstallableStatusCode display_error =
        InstallableEvaluator::GetDisplayError(*manifest, criteria_);
    if (display_error != InstallableStatusCode::NO_ERROR_DETECTED) {
      errors.push_back(display_error);
    }
  }

  if (!HasValidIcon(web_contents_.get(), manifest, criteria_)) {
    errors.push_back(InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON);
  }

  return errors;
}

std::vector<InstallableStatusCode> InstallableEvaluator::CheckEligibility(
    content::WebContents* web_contents) const {
  std::vector<InstallableStatusCode> errors;
  if (web_contents->GetBrowserContext()->IsOffTheRecord()) {
    // TODO(http://crbug.com/452122299): Have this not fail if the we are in
    // CrOS + guest mode (either by not adding this error, or filtering it out
    // later).
    errors.push_back(InstallableStatusCode::IN_INCOGNITO);
  }
  if (!IsContentSecure(web_contents)) {
    errors.push_back(InstallableStatusCode::NOT_FROM_SECURE_ORIGIN);
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
  if (url.GetScheme() == content::kChromeUIScheme) {
    return true;
  }

  // chrome-untrusted:// URLs are shipped with Chrome, so they are considered
  // secure in this context.
  if (url.GetScheme() == content::kChromeUIUntrustedScheme) {
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
