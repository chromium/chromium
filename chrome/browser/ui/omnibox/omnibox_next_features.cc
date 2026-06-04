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
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr base::FeatureState DISABLED = base::FEATURE_DISABLED_BY_DEFAULT;
constexpr base::FeatureState ENABLED = base::FEATURE_ENABLED_BY_DEFAULT;
}  // namespace

namespace omnibox {

namespace internal {

// If enabled, shows the omnibox suggestions in the popup in WebUI.
BASE_FEATURE(kWebUIOmniboxPopup, DISABLED);

// If enabled, Omnibox popup will transition to AI-Mode with the compose-box
// panel taking up the whole of the popup, covering the location bar completely.
BASE_FEATURE(kWebUIOmniboxAimPopup, DISABLED);

// If enabled, the Omnibox Popup will enable a different UI state when on a
// webpage.
BASE_FEATURE(kWebUIOmniboxSimplification, DISABLED);

// If enabled, the "Ask Google about this page" action will route to cobrowse.
BASE_FEATURE(kWebUIOmniboxAskGAboutThisPage, DISABLED);

}  // namespace internal

constexpr base::FeatureParam<AddContextButtonVariant>::Option
    kAddContextButtonVariantOptions[] = {
        {AddContextButtonVariant::kBelowResults, "below_results"},
        {AddContextButtonVariant::kInline, "inline"}};

// Configures the placement of the "Add Context" button in the Omnibox popup.
const base::FeatureParam<AddContextButtonVariant>
    kWebUIOmniboxAimPopupAddContextButtonVariantParam{
        &internal::kWebUIOmniboxSimplification, "Omnibox_AddContextButtonVariant",
        AddContextButtonVariant::kBelowResults,
        &kAddContextButtonVariantOptions};
// If true, hides the "Add Context" button in the "classic" popup.
const base::FeatureParam<bool> kHideClassicContextButton{
    &internal::kWebUIOmniboxSimplification, "Omnibox_HideClassicContextButton",
    true};

// When enabled, clicking aim button in omnibox always navigates directly to
// g.com/aimode, e.g. instead of opening the AI Mode popup
// (`omnibox::internal::kWebUIOmniboxAimPopup`).
BASE_FEATURE(kAiModeEntryPointAlwaysNavigates, DISABLED);
// If enabled, pressing space when the AI mode button has fake focus will
// insert a space into the omnibox and restore focus to the omnibox instead of
// interacting with the button.
BASE_FEATURE(kAiModeSpaceDoesNotActivate, DISABLED);
// If enabled, disables caret color animation for the WebUI Omnibox AIM popup.
BASE_FEATURE(kWebUIOmniboxDisableCaretColorAnimation, ENABLED);
// If enabled, there will no longer be animation when opening the WebUI Omnibox
// AIM popup.
BASE_FEATURE(kWebUIOmniboxAimPopupDisableAnimation, DISABLED);
// If enabled, removes the cutout for the location bar and fills the entire
// popup content with the WebUI WebView.
BASE_FEATURE(kWebUIOmniboxFullPopup, DISABLED);
// If enabled, then both the input row and suggestions dropdown (in the Omnibox)
// will be rendered using the WebUI stack (i.e. the cutout for the location bar
// will be removed).
//
// NOTE: This flag is intended to control the next-gen Omnibox experience and
// will eventually supersede the `kWebUIOmniboxFullPopup` feature flag.
BASE_FEATURE(kWebUIOmniboxFullPopupV2, DISABLED);
// If enabled, enables EverywhereOmnibox popup triggered by shortcut.
BASE_FEATURE(kEverywhereOmnibox, DISABLED);
// Enables the WebUI for omnibox suggestions without modifying the popup UI.
BASE_FEATURE(kWebUIOmniboxPopupDebug, DISABLED);
// Enables side-by-side comparison omnibox suggestions in WebUI and Views.
const base::FeatureParam<bool> kWebUIOmniboxPopupDebugSxSParam{
    &kWebUIOmniboxPopupDebug, "SxS", false};
// If enabled, the WebUIOmniboxPopup controls its own selection state instead of
// following that of the OmniboxEditModel.
BASE_FEATURE(kWebUIOmniboxPopupSelectionControl, DISABLED);

// If enabled, animates the caret in the omnibox.
BASE_FEATURE(kOmniboxAnimatedCaret, ENABLED);

// If enabled, enables energy effect in the omnibox.
BASE_FEATURE(kEnergyEffectInOmnibox, ENABLED);

// If enabled, the Ai Mode button will be dynamically shown in the omnibox.
BASE_FEATURE(kWebUIOmniboxDynamicAiModeButton, DISABLED);

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
  // File upload size limit: 100 MiB.
  attachment_upload->set_max_size_bytes(100 * 1024 * 1024);
  attachment_upload->set_mime_types_allowed(".pdf,application/pdf");

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

bool ShouldShowAimContextMenuOption(Profile* profile) {
  const auto* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  const bool is_aim_entrypoint_enabled =
      OmniboxFieldTrial::IsAimOmniboxEntrypointEnabled(aim_eligibility_service);

  if (is_aim_entrypoint_enabled) {
    return true;
  }

  const bool is_aim_context_entrypoint_enabled =
      omnibox::IsAimPopupEnabled(profile);

  return is_aim_context_entrypoint_enabled;
}

bool IsWebUIOmniboxPopupEnabled() {
  return base::FeatureList::IsEnabled(internal::kWebUIOmniboxPopup);
}

bool IsWebUIOmniboxFullPopupEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup) ||
         base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopupV2);
}

bool IsWebUIOmniboxInBrowserViewEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopupV2) &&
         kWebUIOmniboxFullPopupV2UseBrowserView.Get();
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
  return aim_service && aim_service->IsAimEligible() &&
         aim_service->IsFuseboxEligible();
}

bool IsContentSharingEnabled(
    Profile* profile,
    contextual_search::ContextualSearchSessionHandle* session_handle) {
  if (!profile || !session_handle) {
    return false;
  }
  return session_handle->CheckSearchContentSharingSettings(profile->GetPrefs());
}

bool IsCreateImagesEnabled(Profile* profile) {
  if (!profile) {
    return false;
  }

  if (!IsAimPopupFeatureEnabled()) {
    return false;
  }

  AimEligibilityService* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  return kShowToolsAndModels.Get() && aim_eligibility_service &&
         aim_eligibility_service->IsCreateImagesEligible();
}

bool IsDeepSearchEnabled(Profile* profile) {
  if (!profile) {
    return false;
  }

  if (!IsAimPopupFeatureEnabled()) {
    return false;
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
  config_params->enable_viewport_images = true;
  config_params->attach_page_title_and_url_to_suggest_requests = false;
  return config_params;
}

const base::FeatureParam<std::string>
    kConfigParam(&internal::kWebUIOmniboxAimPopup, "Omnibox_ConfigParam", "");
const base::FeatureParam<bool> kContextMenuEnableMultiTabSelection(
    &internal::kWebUIOmniboxAimPopup,
    "Omnibox_ContextMenuEnableMultiTabSelection",
    false);
const base::FeatureParam<int> kContextMenuMaxTabSuggestions(
    &internal::kWebUIOmniboxAimPopup,
    "Omnibox_ContextMenuMaxTabSuggestions",
    3);
const base::FeatureParam<bool> kShowComposeboxImageSuggestions(
    &internal::kWebUIOmniboxAimPopup,
    "Omnibox_ShowComposeboxImageSuggestions",
    true);
const base::FeatureParam<bool> kShowComposeboxTypedSuggest(
    &internal::kWebUIOmniboxAimPopup,
    "Omnibox_ShowComposeboxTypedSuggest",
    true);
const base::FeatureParam<bool> kShowComposeboxZps(
    &internal::kWebUIOmniboxAimPopup,
    "Omnibox_ShowComposeboxZps",
    true);
const base::FeatureParam<bool> kShowContextMenu(
    &internal::kWebUIOmniboxAimPopup,
    "Omnibox_ShowContextMenu",
    true);
const base::FeatureParam<bool> kShowContextMenuDescription(
    &internal::kWebUIOmniboxAimPopup,
    "Omnibox_ShowContextMenuDescription",
    false);
const base::FeatureParam<bool> kShowContextMenuTabPreviews(
    &internal::kWebUIOmniboxAimPopup,
    "Omnibox_ShowContextMenuTabPreviews",
    true);
const base::FeatureParam<bool> kShowLensSearchChip(
    &internal::kWebUIOmniboxSimplification,
    "Omnibox_ShowLensSearchChip",
    false);
const base::FeatureParam<bool> kShowSmartCompose(
    &internal::kWebUIOmniboxAimPopup,
    "Omnibox_ShowSmartCompose",
    true);
const base::FeatureParam<bool> kShowToolsAndModels(
    &internal::kWebUIOmniboxAimPopup,
    "Omnibox_ShowToolsAndModels",
    true);
const base::FeatureParam<bool> kShowContextMenuHeaders(
    &internal::kWebUIOmniboxAimPopup,
    "Omnibox_ShowContextMenuHeaders",
    true);
const base::FeatureParam<bool> kUseComposeboxFork(
    &internal::kWebUIOmniboxAimPopup,
    "Omnibox_UseComposeboxFork",
    true);
const base::FeatureParam<bool> kContextButtonHasBackground{
    &internal::kWebUIOmniboxSimplification,
    "Omnibox_ContextButtonHasBackground", false};
const base::FeatureParam<bool> kContextButtonShapeIsOblong{
    &internal::kWebUIOmniboxSimplification,
    "Omnibox_ContextButtonShapeIsOblong", false};
const base::FeatureParam<bool> kContextButtonShowSuggestionLabel{
    &internal::kWebUIOmniboxSimplification,
    "Omnibox_ContextButtonShowSuggestionLabel", false};
const base::FeatureParam<bool> kWebUIOmniboxFullPopupV2UseBrowserView{
    &kWebUIOmniboxFullPopupV2, "Omnibox_UseBrowserView", false};

const base::FeatureParam<bool> kAskGCoBrowse{
    &internal::kWebUIOmniboxAskGAboutThisPage, "Omnibox_AskGCoBrowse", false};
const base::FeatureParam<bool> kAskGCoBrowseWithVisualSelection{
    &internal::kWebUIOmniboxAskGAboutThisPage,
    "Omnibox_AskGCoBrowseWithVisualSelection", false};

FeatureConfig::FeatureConfig() : config(GetNTPComposeboxConfig()) {}


FeatureConfig::FeatureConfig(const FeatureConfig&) = default;
FeatureConfig::FeatureConfig(FeatureConfig&&) = default;
FeatureConfig& FeatureConfig::operator=(const FeatureConfig&) = default;
FeatureConfig& FeatureConfig::operator=(FeatureConfig&&) = default;
FeatureConfig::~FeatureConfig() = default;
}  // namespace omnibox
