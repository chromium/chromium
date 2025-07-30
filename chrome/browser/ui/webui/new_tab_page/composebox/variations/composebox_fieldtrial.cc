// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"

#include <string>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/variations/service/variations_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace ntp_composebox {

namespace {

// Decodes a proto object from its serialized Base64 string representation.
// Returns true if decoding and parsing succeed, false otherwise.
bool ParseProtoFromBase64String(const std::string& input,
                                google::protobuf::MessageLite& output) {
  if (input.empty()) {
    return false;
  }

  std::string decoded_input;
  // Decode the Base64-encoded input string into decoded_input.
  if (!base::Base64Decode(input, &decoded_input)) {
    return false;
  }

  if (decoded_input.empty()) {
    return false;
  }

  // Parse the decoded string into the proto object.
  return output.ParseFromString(decoded_input);
}

// Populates and returns the NTP Composebox configuration proto.
omnibox::NTPComposeboxConfig GetNTPComposeboxConfig() {
  // Initialize the default config.
  omnibox::NTPComposeboxConfig default_config;
  default_config.mutable_entry_point()->set_num_page_load_animations(3);

  auto* composebox = default_config.mutable_composebox();
  composebox->set_close_by_escape(true);
  composebox->set_close_by_click_outside(true);

  auto* image_upload = composebox->mutable_image_upload();
  image_upload->set_enable_webp_encoding(false);
  image_upload->set_downscale_max_image_size(1500000);
  image_upload->set_downscale_max_image_width(1600);
  image_upload->set_downscale_max_image_height(1600);
  image_upload->set_image_compression_quality(40);
  image_upload->set_mime_types_allowed("image/*");

  auto* attachment_upload = composebox->mutable_attachment_upload();
  attachment_upload->set_max_size_bytes(200000000);
  attachment_upload->set_mime_types_allowed(".pdf,application/pdf");

  composebox->set_max_num_files(1);
  composebox->set_input_placeholder_text(
      l10n_util::GetStringUTF8(IDS_NTP_COMPOSE_PLACEHOLDER_TEXT));

  // Attempt to parse the config proto from the feature parameter if it is set.
  omnibox::NTPComposeboxConfig fieldtrial_config;
  if (!kConfigParam.Get().empty()) {
    bool parsed =
        ParseProtoFromBase64String(kConfigParam.Get(), fieldtrial_config);
    base::UmaHistogramBoolean(kConfigParamParseSuccessHistogram, parsed);
    if (!parsed) {
      return default_config;
    }
    // A present `MimeTypesAllowed` message will clear the image and attachment
    // `mime_types` value.
    if (fieldtrial_config.composebox()
            .image_upload()
            .has_mime_types_allowed()) {
      image_upload->clear_mime_types_allowed();
    }
    if (fieldtrial_config.composebox()
            .attachment_upload()
            .has_mime_types_allowed()) {
      attachment_upload->clear_mime_types_allowed();
    }
  }

  // Merge the fieldtrial config into the default config.
  //
  // Note: The `MergeFrom()` method will append repeated fields from
  // `fieldtrial_config` to `default_config`. Since the intent is to override
  // the values of repeated fields in `default_config` with the values from
  // `fieldtrial_config`, the repeated fields in `default_config` must be
  // cleared before calling `MergeFrom()` iff the repeated fields have been set
  // in `fieldtrial_config`.
  default_config.MergeFrom(fieldtrial_config);
  return default_config;
}

std::string GetCountryCode(variations::VariationsService* variations_service) {
  std::string country_code;
  if (variations_service) {
    country_code = variations_service->GetStoredPermanentCountry();
    if (country_code.empty()) {
      country_code = variations_service->GetLatestCountry();
    }
  }
  return country_code;
}

bool IsUSCountry(const std::string& country) {
  return country == "us";
}

bool IsEnglishLocale(const std::string& locale) {
  return base::StartsWith(locale, "en", base::CompareCase::SENSITIVE);
}
}  // namespace

// If enabled, the Compose entrypoint will appear in the NTP Searchbox.
BASE_FEATURE(kNtpSearchboxComposeEntrypoint,
             "NtpSearchboxComposeEntrypoint",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNtpSearchboxComposeEntrypointEnglishUS,
             "NtpSearchboxComposeEntrypointEnglishUS",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsNtpSearchboxComposeEntrypointEnabled(BrowserProcess* browser_process) {
  auto* feature_list = base::FeatureList::GetInstance();
  if (feature_list &&
      feature_list->IsFeatureOverridden(kNtpSearchboxComposeEntrypoint.name)) {
    return base::FeatureList::IsEnabled(kNtpSearchboxComposeEntrypoint);
  }

  auto locale =
      browser_process->GetFeatures()->application_locale_storage()->Get();
  auto country = GetCountryCode(browser_process->variations_service());

  if (IsEnglishLocale(locale) && IsUSCountry(country)) {
    return base::FeatureList::IsEnabled(
        kNtpSearchboxComposeEntrypointEnglishUS);
  }
  return base::FeatureList::IsEnabled(kNtpSearchboxComposeEntrypoint);
}

// If enabled, the Composebox will appear upon clicking the NTP Compose
// entrypoint and will be configured based on the supplied configuration param.
BASE_FEATURE(kNtpComposebox,
             "NtpComposebox",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kConfigParam(&kNtpComposebox,
                                                   "ConfigParam",
                                                   "");

const base::FeatureParam<bool> kSendLnsSurfaceParam(&kNtpComposebox,
                                                    "SendLnsSurfaceParam",
                                                    false);

const base::FeatureParam<bool> kShowComposeboxZps(&kNtpComposebox,
                                                  "ShowComposeboxZps",
                                                  false);

FeatureConfig::FeatureConfig()
    : enabled(base::FeatureList::IsEnabled(kNtpComposebox)),
      config(GetNTPComposeboxConfig()) {}

}  // namespace ntp_composebox
