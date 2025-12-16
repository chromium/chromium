// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"

#include <string>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
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
  composebox->set_close_by_escape(kCloseComposeboxByEscape.Get());
  composebox->set_close_by_click_outside(kCloseComposeboxByClickOutside.Get());

  auto* image_upload = composebox->mutable_image_upload();
  image_upload->set_enable_webp_encoding(false);
  image_upload->set_downscale_max_image_size(1500000);
  image_upload->set_downscale_max_image_width(1600);
  image_upload->set_downscale_max_image_height(1600);
  image_upload->set_image_compression_quality(40);
  // The current list of image types that Lens Backend supports
  image_upload->set_mime_types_allowed(
      "image/avif,image/bmp,image/jpeg,image/png,image/webp,image/heif,"
      "image/heic");
  auto* attachment_upload = composebox->mutable_attachment_upload();
  attachment_upload->set_max_size_bytes(200000000);
  attachment_upload->set_mime_types_allowed(".pdf,application/pdf");

  composebox->set_max_num_files(kMaxNumFiles.Get());
  composebox->set_input_placeholder_text(
      l10n_util::GetStringUTF8(IDS_NTP_COMPOSE_PLACEHOLDER_TEXT));
  composebox->set_is_pdf_upload_enabled(true);

  auto* placeholder_config = composebox->mutable_placeholder_config();
  placeholder_config->set_change_text_animation_interval_ms(2000);
  placeholder_config->set_fade_text_animation_duration_ms(250);

  placeholder_config->add_placeholders(
      omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_ASK);
  placeholder_config->add_placeholders(
      omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_PLAN);
  placeholder_config->add_placeholders(
      omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_ASK_TAB);
  placeholder_config->add_placeholders(
      omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_RESEARCH);
  placeholder_config->add_placeholders(
      omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_WRITE);
  placeholder_config->add_placeholders(
      omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_IMAGE);

  // Attempt to parse the config proto from the feature parameter if it is set.
  omnibox::NTPComposeboxConfig fieldtrial_config;
  if (!kConfigParam.Get().empty()) {
    bool parsed =
        ParseProtoFromBase64String(kConfigParam.Get(), fieldtrial_config);
    contextual_search::ContextualSearchMetricsRecorder::
        RecordConfigParseSuccess(
            contextual_search::ContextualSearchSource::kNewTabPage, parsed);
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

}  // namespace

bool IsNtpComposeboxEnabled(Profile* profile) {
  if (!profile) {
    return false;
  }

  // The `AimEligibilityService` depends on the `TemplateURLService`. If the
  // `TemplateURLService` does not exist for this profile, then the
  // `AimEligibilityService` cannot be created.
  if (!TemplateURLServiceFactory::GetForProfile(profile)) {
    return false;
  }

  AimEligibilityService* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  if (!aim_eligibility_service) {
    return false;
  }

  return base::FeatureList::IsEnabled(kNtpComposebox) &&
         aim_eligibility_service->IsAimEligible();
}

bool IsDeepSearchEnabled(Profile* profile) {
  if (!profile) {
    return false;
  }

  if (!IsNtpComposeboxEnabled(profile)) {
    return false;
  }

  if (kShowToolsAndModels.Get() && kForceToolsAndModels.Get()) {
    return true;
  }

  AimEligibilityService* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  return kShowToolsAndModels.Get() && aim_eligibility_service &&
         aim_eligibility_service->IsDeepSearchEligible();
}

bool IsCreateImagesEnabled(Profile* profile) {
  if (!profile) {
    return false;
  }

  if (!IsNtpComposeboxEnabled(profile)) {
    return false;
  }

  if (kShowToolsAndModels.Get() && kShowCreateImageTool.Get() &&
      kForceToolsAndModels.Get()) {
    return true;
  }

  AimEligibilityService* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  return kShowToolsAndModels.Get() && kShowCreateImageTool.Get() &&
         aim_eligibility_service &&
         aim_eligibility_service->IsCreateImagesEligible();
}

std::unique_ptr<
    contextual_search::ContextualSearchContextController::ConfigParams>
CreateQueryControllerConfigParams() {
  auto config_params = std::make_unique<
      contextual_search::ContextualSearchContextController::ConfigParams>();
  config_params->send_lns_surface = kSendLnsSurfaceParam.Get();
  config_params->suppress_lns_surface_param_if_no_image =
      kSuppressLnsSurfaceParamIfNoImage.Get();
  config_params->enable_multi_context_input_flow = kMaxNumFiles.Get() > 1;
  config_params->enable_viewport_images = kEnableViewportImages.Get();
  config_params->use_separate_request_ids_for_multi_context_viewport_images =
      kUseSeparateRequestIdsForMultiContextViewportImages.Get();
  config_params->enable_context_id_migration = kEnableContextIdMigration.Get();
  config_params->attach_page_title_and_url_to_suggest_requests =
      kAttachPageTitleAndUrlToSuggestRequest.Get();
  return config_params;
}

BASE_FEATURE(kNtpComposebox, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kConfigParam(&kNtpComposebox,
                                                   "ConfigParam",
                                                   "");

const base::FeatureParam<bool> kSendLnsSurfaceParam(&kNtpComposebox,
                                                    "SendLnsSurfaceParam",
                                                    true);

const base::FeatureParam<bool> kSuppressLnsSurfaceParamIfNoImage(
    &kNtpComposebox,
    "SuppressLnsSurfaceParamIfNoImage",
    true);

const base::FeatureParam<bool>
    kUseSeparateRequestIdsForMultiContextViewportImages(
        &kNtpComposebox,
        "UseSeparateRequestIdsForMultiContextViewportImages",
        false);

const base::FeatureParam<bool> kEnableContextIdMigration(
    &kNtpComposebox,
    "EnableContextIdMigration",
    false);

const base::FeatureParam<bool> kShowComposeboxZps(&kNtpComposebox,
                                                  "ShowComposeboxZps",
                                                  false);

const base::FeatureParam<bool> kShowComposeboxTypedSuggest(
    &kNtpComposebox,
    "ShowComposeboxTypedSuggest",
    false);

const base::FeatureParam<bool> kShowComposeboxImageSuggestions(
    &kNtpComposebox,
    "ShowComposeboxImageSuggestions",
    false);

const base::FeatureParam<bool> kAttachPageTitleAndUrlToSuggestRequest(
    &kNtpComposebox,
    "AttachPageTitleAndUrlToSuggestRequest",
    false);

const base::FeatureParam<bool> kShowContextMenu(&kNtpComposebox,
                                                "ShowContextMenu",
                                                false);
const base::FeatureParam<bool> kShowRecentTabChip(&kNtpComposebox,
                                                  "ShowRecentTabChip",
                                                  false);
const base::FeatureParam<bool> kShowContextMenuTabPreviews(
    &kNtpComposebox,
    "ShowContextMenuTabPreviews",
    false);

const base::FeatureParam<bool> kShowContextMenuDescription(
    &kNtpComposebox,
    "ShowContextMenuDescription",
    true);
const base::FeatureParam<bool> kEnableViewportImages(&kNtpComposebox,
                                                     "EnableViewportImages",
                                                     true);

const base::FeatureParam<bool> kShowToolsAndModels(&kNtpComposebox,
                                                   "ShowToolsAndModels",
                                                   false);

const base::FeatureParam<bool> kShowCreateImageTool(&kNtpComposebox,
                                                    "ShowCreateImageTool",
                                                    false);

const base::FeatureParam<bool> kShowSubmit(&kNtpComposebox, "ShowSubmit", true);

const base::FeatureParam<bool> kShowVoiceSearchInSteadyComposebox(
    &kNtpComposebox,
    "ShowVoiceSearchInSteadyComposebox",
    true);

const base::FeatureParam<bool> kShowVoiceSearchInExpandedComposebox(
    &kNtpComposebox,
    "ShowVoiceSearchInExpandedComposebox",
    true);

const base::FeatureParam<bool> kShowSmartCompose(&kNtpComposebox,
                                                 "ShowSmartCompose",
                                                 true);

const base::FeatureParam<bool> kForceToolsAndModels(&kNtpComposebox,
                                                    "ForceToolsAndModels",
                                                    false);

const base::FeatureParam<int> kContextMenuMaxTabSuggestions(
    &kNtpComposebox,
    "ContextMenuMaxTabSuggestions",
    5);

const base::FeatureParam<bool> kContextMenuEnableMultiTabSelection(
    &kNtpComposebox,
    "ContextMenuEnableMultiTabSelection",
    false);

const base::FeatureParam<int> kMaxNumFiles(&kNtpComposebox, "MaxNumFiles", 1);

const base::FeatureParam<bool> kEnableContextDragAndDrop(
    &kNtpComposebox,
    "EnableContextDragAndDrop",
    true);

const base::FeatureParam<bool>
    kCloseComposeboxByEscape(&kNtpComposebox, "CloseComposeboxByEscape", true);

const base::FeatureParam<bool> kCloseComposeboxByClickOutside(
    &kNtpComposebox,
    "CloseComposeboxByClickOutside",
    true);
const base::FeatureParam<bool> kAddTabUploadDelayOnRecentTabChipClick(
    &kNtpComposebox,
    "AddTabUploadDelayOnRecentTabChipClick",
    true);
const base::FeatureParam<bool> kEnableModalComposebox(&kNtpComposebox,
                                                      "EnableModalComposebox",
                                                      true);

FeatureConfig::FeatureConfig() : config(GetNTPComposeboxConfig()) {}

FeatureConfig::FeatureConfig(const FeatureConfig&) = default;
FeatureConfig::FeatureConfig(FeatureConfig&&) = default;
FeatureConfig& FeatureConfig::operator=(const FeatureConfig&) = default;
FeatureConfig& FeatureConfig::operator=(FeatureConfig&&) = default;
FeatureConfig::~FeatureConfig() = default;

}  // namespace ntp_composebox

namespace ntp_realbox {

bool IsNtpRealboxNextEnabled(Profile* profile) {
  if (!profile) {
    return false;
  }

  if (!ntp_composebox::IsNtpComposeboxEnabled(profile)) {
    return false;
  }

  // The `AimEligibilityService` depends on the `TemplateURLService`. If the
  // `TemplateURLService` does not exist for this profile, then the
  // `AimEligibilityService` cannot be created.
  if (!TemplateURLServiceFactory::GetForProfile(profile)) {
    return false;
  }

  AimEligibilityService* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  if (!aim_eligibility_service) {
    return false;
  }

  return base::FeatureList::IsEnabled(kNtpRealboxNext) &&
         aim_eligibility_service->IsAimEligible();
}

BASE_FEATURE(kNtpRealboxNext, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<PlaceholderText>::Option kSteadyPlaceholderOptions[] =
    {
        {PlaceholderText::ASK_OR_TYPE, "AskOrType"},
        {PlaceholderText::ASK, "Ask"},
};

const base::FeatureParam<PlaceholderText> kSteadyPlaceholder(
    &kNtpRealboxNext,
    "SteadyPlaceholder",
    PlaceholderText::ASK_OR_TYPE,
    &kSteadyPlaceholderOptions);

const base::FeatureParam<bool> kCyclingPlaceholders(&kNtpRealboxNext,
                                                    "CyclingPlaceholders",
                                                    false);

const base::FeatureParam<bool> kShowVoiceSearchInExpandedRealbox(
    &kNtpRealboxNext,
    "ShowVoiceSearchInExpandedRealbox",
    false);

const base::FeatureParam<RealboxLayoutMode>::Option
    kRealboxLayoutModeOptions[] = {
        {RealboxLayoutMode::kTallBottomContext,
         kRealboxLayoutModeTallBottomContext},
        {RealboxLayoutMode::kTallTopContext, kRealboxLayoutModeTallTopContext},
        {RealboxLayoutMode::kCompact, kRealboxLayoutModeCompact}};

const base::FeatureParam<RealboxLayoutMode> kRealboxLayoutMode(
    &kNtpRealboxNext,
    "RealboxLayoutMode",
    RealboxLayoutMode::kCompact,
    &kRealboxLayoutModeOptions);

std::string_view RealboxLayoutModeToString(
    RealboxLayoutMode realbox_layout_mode) {
  switch (realbox_layout_mode) {
    case RealboxLayoutMode::kTallBottomContext:
      return kRealboxLayoutModeTallBottomContext;
    case RealboxLayoutMode::kTallTopContext:
      return kRealboxLayoutModeTallTopContext;
    case RealboxLayoutMode::kCompact:
      return kRealboxLayoutModeCompact;
    default:
      NOTREACHED();
  }
}

}  // namespace ntp_realbox
