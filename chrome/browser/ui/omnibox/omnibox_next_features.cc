// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_next_features.h"

#include <string>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr base::FeatureState DISABLED = base::FEATURE_DISABLED_BY_DEFAULT;
}  // namespace

namespace omnibox {

namespace internal {

// If enabled, Omnibox popup will transition to AI-Mode with the compose-box
// panel taking up the whole of the popup, covering the location bar completely.
BASE_FEATURE(kWebUIOmniboxAimPopup, DISABLED);

}  // namespace internal

constexpr base::FeatureParam<AddContextButtonVariant>::Option
    kAddContextButtonVariantOptions[] = {
        {AddContextButtonVariant::kNone, "none"},
        {AddContextButtonVariant::kBelowResults, "below_results"},
        {AddContextButtonVariant::kAboveResults, "above_results"},
        {AddContextButtonVariant::kInline, "inline"}};

// Configures the placement of the "Add Context" button in the Omnibox popup.
const base::FeatureParam<AddContextButtonVariant>
    kWebUIOmniboxAimPopupAddContextButtonVariantParam{
        &internal::kWebUIOmniboxAimPopup, "AddContextButtonVariant",
        AddContextButtonVariant::kNone, &kAddContextButtonVariantOptions};
// If enabled, removes the cutout for the location bar and fills the entire
// popup content with the WebUI WebView.
BASE_FEATURE(kWebUIOmniboxFullPopup, DISABLED);
// If enabled, shows the omnibox suggestions in the popup in WebUI.
BASE_FEATURE(kWebUIOmniboxPopup, DISABLED);
// Enables the WebUI for omnibox suggestions without modifying the popup UI.
BASE_FEATURE(kWebUIOmniboxPopupDebug, DISABLED);
// Enables side-by-side comparison omnibox suggestions in WebUI and Views.
const base::FeatureParam<bool> kWebUIOmniboxPopupDebugSxSParam{
    &kWebUIOmniboxPopupDebug, "SxS", false};

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

// Populates and returns the Composebox configuration proto.
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
  image_upload->set_mime_types_allowed(
      "image/avif,image/bmp,image/jpeg,image/png,image/webp,image/heif,image/"
      "heic");

  auto* attachment_upload = composebox->mutable_attachment_upload();
  attachment_upload->set_max_size_bytes(200000000);
  attachment_upload->set_mime_types_allowed(".pdf,application/pdf");

  composebox->set_max_num_files(kMaxNumFiles.Get());
  composebox->set_input_placeholder_text(
      l10n_util::GetStringUTF8(IDS_NTP_COMPOSE_PLACEHOLDER_TEXT));
  composebox->set_is_pdf_upload_enabled(true);

  auto* placeholder_config = composebox->mutable_placeholder_config();
  placeholder_config->set_change_text_animation_interval_ms(4000);
  placeholder_config->set_fade_text_animation_duration_ms(250);

  placeholder_config->add_placeholders(
      omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_ASK);
  placeholder_config->add_placeholders(
      omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_PLAN);
  placeholder_config->add_placeholders(
      omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_COMPARE);
  placeholder_config->add_placeholders(
      omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_RESEARCH);
  placeholder_config->add_placeholders(
      omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_TEACH);
  placeholder_config->add_placeholders(
      omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_WRITE);

  // Attempt to parse the config proto from the feature parameter if it is set.
  omnibox::NTPComposeboxConfig fieldtrial_config;
  if (!kConfigParam.Get().empty()) {
    bool parsed =
        ParseProtoFromBase64String(kConfigParam.Get(), fieldtrial_config);
    contextual_search::ContextualSearchMetricsRecorder::
        RecordConfigParseSuccess(
            contextual_search::ContextualSearchSource::kOmnibox, parsed);
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

bool IsAimPopupFeatureEnabled() {
  return base::FeatureList::IsEnabled(internal::kWebUIOmniboxAimPopup);
}

bool IsAimPopupEnabled(Profile* profile) {
  if (!profile) {
    return false;
  }

  if (!IsAimPopupFeatureEnabled()) {
    return false;
  }

  auto* aim_service = AimEligibilityServiceFactory::GetForProfile(profile);
  return aim_service && aim_service->IsAimEligible();
}

bool IsCreateImagesEnabled(Profile* profile) {
  if (!profile) {
    return false;
  }

  if (!IsAimPopupFeatureEnabled()) {
    return false;
  }

  if (kShowToolsAndModels.Get() && kShowCreateImageTool.Get()) {
    return true;
  }

  AimEligibilityService* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  return kShowToolsAndModels.Get() && kShowCreateImageTool.Get() &&
         aim_eligibility_service &&
         aim_eligibility_service->IsCreateImagesEligible();
}

bool IsDeepSearchEnabled(Profile* profile) {
  if (!profile) {
    return false;
  }

  if (!IsAimPopupFeatureEnabled()) {
    return false;
  }

  if (kShowToolsAndModels.Get()) {
    return true;
  }

  AimEligibilityService* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  return kShowToolsAndModels.Get() && aim_eligibility_service &&
         aim_eligibility_service->IsDeepSearchEligible();
}

std::unique_ptr<
    contextual_search::ContextualSearchContextController::ConfigParams>
CreateQueryControllerConfigParams() {
  auto config_params = std::make_unique<
      contextual_search::ContextualSearchContextController::ConfigParams>();
  config_params->send_lns_surface = true;
  config_params->enable_multi_context_input_flow = kMaxNumFiles.Get() > 1;
  config_params->enable_viewport_images = kEnableViewportImages.Get();
  config_params->use_separate_request_ids_for_multi_context_viewport_images =
      kUseSeparateRequestIdsForMultiContextViewportImages.Get();
  config_params->attach_page_title_and_url_to_suggest_requests =
      kAttachPageTitleAndUrlToSuggestRequest.Get();
  return config_params;
}

const base::FeatureParam<bool> kCloseComposeboxByClickOutside(
    &internal::kWebUIOmniboxAimPopup,
    "CloseComposeboxByClickOutside",
    true);
const base::FeatureParam<bool> kCloseComposeboxByEscape(
    &internal::kWebUIOmniboxAimPopup,
    "CloseComposeboxByEscape",
    true);
const base::FeatureParam<std::string>
    kConfigParam(&internal::kWebUIOmniboxAimPopup, "ConfigParam", "");
const base::FeatureParam<bool> kContextMenuEnableMultiTabSelection(
    &internal::kWebUIOmniboxAimPopup,
    "ContextMenuEnableMultiTabSelection",
    false);
const base::FeatureParam<int> kContextMenuMaxTabSuggestions(
    &internal::kWebUIOmniboxAimPopup,
    "ContextMenuMaxTabSuggestions",
    5);
const base::FeatureParam<bool> kEnableViewportImages(
    &internal::kWebUIOmniboxAimPopup,
    "EnableViewportImages",
    true);
const base::FeatureParam<int> kMaxNumFiles(&internal::kWebUIOmniboxAimPopup,
                                           "MaxNumFiles",
                                           1);
const base::FeatureParam<bool> kShowComposeboxImageSuggestions(
    &internal::kWebUIOmniboxAimPopup,
    "ShowComposeboxImageSuggestions",
    true);
const base::FeatureParam<bool> kShowComposeboxTypedSuggest(
    &internal::kWebUIOmniboxAimPopup,
    "ShowComposeboxTypedSuggest",
    true);
const base::FeatureParam<bool> kShowComposeboxZps(
    &internal::kWebUIOmniboxAimPopup,
    "ShowComposeboxZps",
    true);
const base::FeatureParam<bool>
    kShowContextMenu(&internal::kWebUIOmniboxAimPopup, "ShowContextMenu", true);
const base::FeatureParam<bool> kShowContextMenuDescription(
    &internal::kWebUIOmniboxAimPopup,
    "ShowContextMenuDescription",
    true);
const base::FeatureParam<bool> kShowContextMenuTabPreviews(
    &internal::kWebUIOmniboxAimPopup,
    "ShowContextMenuTabPreviews",
    true);
const base::FeatureParam<bool> kShowCreateImageTool(
    &internal::kWebUIOmniboxAimPopup,
    "ShowCreateImageTool",
    true);
// TODO(crbug.com/462739330): Enable lens chip.
const base::FeatureParam<bool> kShowLensSearchChip(
    &internal::kWebUIOmniboxAimPopup,
    "ShowLensSearchChip",
    false);
const base::FeatureParam<bool> kAddTabUploadDelayOnRecentTabChipClick(
    &internal::kWebUIOmniboxAimPopup,
    "AddTabUploadDelayOnRecentTabChipClick",
    true);
const base::FeatureParam<bool> kShowRecentTabChip(
    &internal::kWebUIOmniboxAimPopup,
    "ShowRecentTabChip",
    false);
const base::FeatureParam<bool> kShowSmartCompose(
    &internal::kWebUIOmniboxAimPopup,
    "ShowSmartCompose",
    true);
const base::FeatureParam<bool> kShowSubmit(&internal::kWebUIOmniboxAimPopup,
                                           "ShowSubmit",
                                           true);
const base::FeatureParam<bool> kShowToolsAndModels(
    &internal::kWebUIOmniboxAimPopup,
    "ShowToolsAndModels",
    true);
const base::FeatureParam<bool> kShowVoiceSearchInSteadyComposebox(
    &internal::kWebUIOmniboxAimPopup,
    "ShowVoiceSearchInSteadyComposebox",
    false);
const base::FeatureParam<bool> kShowVoiceSearchInExpandedComposebox(
    &internal::kWebUIOmniboxAimPopup,
    "ShowVoiceSearchInExpandedComposebox",
    false);
const base::FeatureParam<bool> kEnableContextDragAndDrop(&internal::kWebUIOmniboxAimPopup,
                                                  "EnableContextDragAndDrop",
                                                  false);
const base::FeatureParam<bool>
    kUseSeparateRequestIdsForMultiContextViewportImages(
        &internal::kWebUIOmniboxAimPopup,
        "UseSeparateRequestIdsForMultiContextViewportImages",
        false);
const base::FeatureParam<bool> kAttachPageTitleAndUrlToSuggestRequest(
    &internal::kWebUIOmniboxAimPopup,
    "AttachPageTitleAndUrlToSuggestRequest",
    false);

FeatureConfig::FeatureConfig() : config(GetNTPComposeboxConfig()) {}

FeatureConfig::FeatureConfig(const FeatureConfig&) = default;
FeatureConfig::FeatureConfig(FeatureConfig&&) = default;
FeatureConfig& FeatureConfig::operator=(const FeatureConfig&) = default;
FeatureConfig& FeatureConfig::operator=(FeatureConfig&&) = default;
FeatureConfig::~FeatureConfig() = default;
}  // namespace omnibox
