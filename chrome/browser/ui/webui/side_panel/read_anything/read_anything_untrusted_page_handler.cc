// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_page_handler.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/accessibility/phrase_segmentation/dependency_parser_model_loader.h"
#include "chrome/browser/accessibility/phrase_segmentation/dependency_parser_model_loader_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/common/accessibility/read_anything.mojom-forward.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/common/translate_constants.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/web_ui.h"
#include "net/http/http_status_code.h"
#include "pdf/buildflags.h"
#include "services/network/public/cpp/header_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/accessibility/pdf_ocr_controller.h"
#include "chrome/browser/accessibility/pdf_ocr_controller_factory.h"
#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"
#include "components/pdf/common/pdf_util.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/session/session_controller.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"
using ash::language_packs::LanguagePackManager;
using ash::language_packs::PackResult;
#endif

using read_anything::mojom::ErrorCode;
using read_anything::mojom::InstallationState;
using read_anything::mojom::UntrustedPage;
using read_anything::mojom::UntrustedPageHandler;
using read_anything::mojom::VoicePackInstallationState;

namespace {

// All AXMode flags of kAXModeWebContentsOnly are needed. |ui::AXMode::kHTML| is
// needed for retrieveing the `aria-expanded` attribute.
// |ui::AXMode::kScreenReader| is needed for HTML tag, and heading level
// information. |ui::AXMode::kInlineTextBoxes| is needed for complete screen2x
// output -- if excluded, some nodes from the tree will not be identified as
// content nodes.
// TODO(crbug.com/366000250): kHTML is a heavy-handed approach as it copies all
// HTML attributes into the accessibility tree. It should be removed ASAP.
constexpr ui::AXMode kReadAnythingAXMode =
    ui::kAXModeWebContentsOnly | ui::AXMode::kHTML;

int GetNormalizedFontScale(double font_scale) {
  DCHECK(font_scale >= kReadAnythingMinimumFontScale &&
         font_scale <= kReadAnythingMaximumFontScale);
  return (font_scale - kReadAnythingMinimumFontScale) *
         (1 / kReadAnythingFontScaleIncrement);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

InstallationState GetInstallationStateFromStatusCode(
    const PackResult::StatusCode status_code) {
  switch (status_code) {
    case PackResult::StatusCode::kNotInstalled:
      return InstallationState::kNotInstalled;
    case PackResult::StatusCode::kInProgress:
      return InstallationState::kInstalling;
    case PackResult::StatusCode::kInstalled:
      return InstallationState::kInstalled;
    case PackResult::StatusCode::kUnknown:
      return InstallationState::kUnknown;
  }
}

ErrorCode GetMojoErrorFromPackError(const PackResult::ErrorCode pack_error) {
  switch (pack_error) {
    case PackResult::ErrorCode::kNone:
      return ErrorCode::kNone;
    case PackResult::ErrorCode::kOther:
      return ErrorCode::kOther;
    case PackResult::ErrorCode::kWrongId:
      return ErrorCode::kWrongId;
    case PackResult::ErrorCode::kNeedReboot:
      return ErrorCode::kNeedReboot;
    case PackResult::ErrorCode::kAllocation:
      return ErrorCode::kAllocation;
  }
}

// Called when LanguagePackManager::GetPackState or ::InstallPack is complete.
void OnLanguagePackManagerResponse(
    read_anything::mojom::UntrustedPageHandler::GetVoicePackInfoCallback
        mojo_remote_callback,
    const PackResult& pack_result) {
  // Convert the LanguagePackManager's response object into a mojo object
  read_anything::mojom::VoicePackInfoPtr voicePackInfo =
      read_anything::mojom::VoicePackInfo::New();

  if (pack_result.operation_error == PackResult::ErrorCode::kNone) {
    voicePackInfo->pack_state =
        VoicePackInstallationState::NewInstallationState(
            GetInstallationStateFromStatusCode(pack_result.pack_state));
  } else {
    voicePackInfo->pack_state = VoicePackInstallationState::NewErrorCode(
        GetMojoErrorFromPackError(pack_result.operation_error));
  }
  voicePackInfo->language = pack_result.language_code;

  // Call the callback sent from the mojo remote
  std::move(mojo_remote_callback).Run(std::move(voicePackInfo));
}

#endif

class PersistentAccessibilityHelper
    : public content::WebContentsUserData<PersistentAccessibilityHelper> {
 public:
  ~PersistentAccessibilityHelper() override = default;

  // Persists `scoped_accessibility_mode` for `web_contents`.
  static void PersistForWebContents(
      content::WebContents& web_contents,
      std::unique_ptr<content::ScopedAccessibilityMode>
          scoped_accessibility_mode);

 private:
  friend content::WebContentsUserData<PersistentAccessibilityHelper>;

  PersistentAccessibilityHelper(
      content::WebContents& web_contents,
      std::unique_ptr<content::ScopedAccessibilityMode>
          scoped_accessibility_mode)
      : WebContentsUserData(web_contents),
        scoped_accessibility_mode_(std::move(scoped_accessibility_mode)) {}

  WEB_CONTENTS_USER_DATA_KEY_DECL();
  std::unique_ptr<content::ScopedAccessibilityMode> scoped_accessibility_mode_;
};

// static
void PersistentAccessibilityHelper::PersistForWebContents(
    content::WebContents& web_contents,
    std::unique_ptr<content::ScopedAccessibilityMode>
        scoped_accessibility_mode) {
  if (auto* const instance = FromWebContents(&web_contents); instance) {
    instance->scoped_accessibility_mode_ = std::move(scoped_accessibility_mode);
  } else {
    web_contents.SetUserData(
        UserDataKey(),
        base::WrapUnique(new PersistentAccessibilityHelper(
            web_contents, std::move(scoped_accessibility_mode))));
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PersistentAccessibilityHelper);

}  // namespace

ReadAnythingWebContentsObserver::ReadAnythingWebContentsObserver(
    base::SafeRef<ReadAnythingUntrustedPageHandler> page_handler,
    content::WebContents* web_contents,
    ui::AXMode accessibility_mode)
    : page_handler_(page_handler) {
  Observe(web_contents);

  // Enable accessibility for the top level render frame and all descendants.
  // This causes AXTreeSerializer to reset and send accessibility events of
  // the AXTree when it is re-serialized.
  if (!web_contents) {
    return;
  }
  // Force a reset if web accessibility is already enabled to ensure that new
  // observers of accessibility events get the full accessibility tree from
  // scratch.
  const bool need_reset =
      web_contents->GetAccessibilityMode().has_mode(ui::AXMode::kWebContents);

  scoped_accessibility_mode_ =
      content::BrowserAccessibilityState::GetInstance()
          ->CreateScopedModeForWebContents(web_contents, accessibility_mode);

  if (base::FeatureList::IsEnabled(
          features::kReadAnythingPermanentAccessibility)) {
    // If permanent accessibility for Read Anything is enabled, give ownership
    // of the scoper to the WebContents. This ensures that those modes are kept
    // active even when RA is no longer handling events from the WC. This
    // codepath is to be deleted at the conclusion of the study.
    PersistentAccessibilityHelper::PersistForWebContents(
        *web_contents, std::move(scoped_accessibility_mode_));
  }

  if (need_reset) {
    web_contents->ResetAccessibility();
  }
}

ReadAnythingWebContentsObserver::~ReadAnythingWebContentsObserver() = default;

void ReadAnythingWebContentsObserver::AccessibilityEventReceived(
    const ui::AXUpdatesAndEvents& details) {
  page_handler_->AccessibilityEventReceived(details);
}

void ReadAnythingWebContentsObserver::AccessibilityLocationChangesReceived(
    const ui::AXTreeID& tree_id,
    ui::AXLocationAndScrollUpdates& details) {
  page_handler_->AccessibilityLocationChangesReceived(tree_id, details);
}

void ReadAnythingWebContentsObserver::PrimaryPageChanged(content::Page& page) {
  page_handler_->PrimaryPageChanged();
}

void ReadAnythingWebContentsObserver::WebContentsDestroyed() {
  page_handler_->WebContentsDestroyed();
}

ReadAnythingUntrustedPageHandler::ReadAnythingUntrustedPageHandler(
    mojo::PendingRemote<UntrustedPage> page,
    mojo::PendingReceiver<UntrustedPageHandler> receiver,
    content::WebUI* web_ui)
    : profile_(Profile::FromWebUI(web_ui)),
      web_ui_(web_ui),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  ax_action_handler_observer_.Observe(
      ui::AXActionHandlerRegistry::GetInstance());

  side_panel_controller_ = ReadAnythingSidePanelControllerGlue::FromWebContents(
                               web_ui_->GetWebContents())
                               ->controller();
  side_panel_controller_->AddPageHandlerAsObserver(weak_factory_.GetWeakPtr());

  PrefService* prefs = profile_->GetPrefs();
  double speechRate =
      features::IsReadAnythingReadAloudEnabled()
          ? prefs->GetDouble(prefs::kAccessibilityReadAnythingSpeechRate)
          : kReadAnythingDefaultSpeechRate;
  read_anything::mojom::HighlightGranularity highlightGranularity =
      features::IsReadAnythingReadAloudEnabled()
          ? static_cast<read_anything::mojom::HighlightGranularity>(
                prefs->GetDouble(
                    prefs::kAccessibilityReadAnythingHighlightGranularity))
          : read_anything::mojom::HighlightGranularity::kDefaultValue;
  base::Value::Dict voices = base::Value::Dict();
  if (features::IsReadAnythingReadAloudEnabled()) {
    if (features::IsReadAloudAutoVoiceSwitchingEnabled()) {
      voices =
          prefs->GetDict(prefs::kAccessibilityReadAnythingVoiceName).Clone();
    } else {
      std::string voice_name =
          prefs->GetString(prefs::kAccessibilityReadAnythingVoiceName);
      if (!voice_name.empty()) {
        voices.Set("", voice_name);
      }
    }
  }

  page_->OnSettingsRestoredFromPrefs(
      static_cast<read_anything::mojom::LineSpacing>(
          prefs->GetInteger(prefs::kAccessibilityReadAnythingLineSpacing)),
      static_cast<read_anything::mojom::LetterSpacing>(
          prefs->GetInteger(prefs::kAccessibilityReadAnythingLetterSpacing)),
      prefs->GetString(prefs::kAccessibilityReadAnythingFontName),
      prefs->GetDouble(prefs::kAccessibilityReadAnythingFontScale),
      prefs->GetBoolean(prefs::kAccessibilityReadAnythingLinksEnabled),
      prefs->GetBoolean(prefs::kAccessibilityReadAnythingImagesEnabled),
      static_cast<read_anything::mojom::Colors>(
          prefs->GetInteger(prefs::kAccessibilityReadAnythingColorInfo)),
      speechRate, std::move(voices),
      features::IsReadAnythingReadAloudEnabled()
          ? prefs->GetList(prefs::kAccessibilityReadAnythingLanguagesEnabled)
                .Clone()
          : base::Value::List(),
      highlightGranularity);

  // Get user's default language to check for compatible fonts.
  language::LanguageModel* language_model =
      LanguageModelManagerFactory::GetForBrowserContext(profile_)
          ->GetPrimaryModel();
  std::string prefs_lang = language_model->GetLanguages().front().lang_code;
  prefs_lang = language::ExtractBaseLanguage(prefs_lang);
  SetDefaultLanguageCode(prefs_lang);

  if (features::IsReadAnythingWithScreen2xEnabled()) {
    screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(profile_)
        ->GetServiceStateAsync(
            screen_ai::ScreenAIServiceRouter::Service::kMainContentExtraction,
            base::BindOnce(
                &ReadAnythingUntrustedPageHandler::OnScreenAIServiceInitialized,
                weak_factory_.GetWeakPtr()));
  }
  if (features::IsPdfOcrEnabled()) {
    screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(profile_)
        ->GetServiceStateAsync(screen_ai::ScreenAIServiceRouter::Service::kOCR,
                               base::DoNothing());
  }

  // Enable accessibility for the top level render frame and all descendants.
  // This causes AXTreeSerializer to reset and send accessibility events of
  // the AXTree when it is re-serialized.
  main_observer_ = std::make_unique<ReadAnythingWebContentsObserver>(
      weak_factory_.GetSafeRef(), side_panel_controller_->tab()->GetContents(),
      kReadAnythingAXMode);
  SetUpPdfObserver();
  OnActiveAXTreeIDChanged();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* session_controller = ash::SessionController::Get();
  if (session_controller) {
    session_controller->AddObserver(this);
  }
#endif
}

ReadAnythingUntrustedPageHandler::~ReadAnythingUntrustedPageHandler() {
  translate_observation_.Reset();
  web_screenshotter_.reset();
  main_observer_.reset();
  pdf_observer_.reset();
  LogTextStyle();

  if (side_panel_controller_) {
    // If |this| is destroyed before the |ReadAnythingSidePanelController|, then
    // remove |this| from the observer lists. In the cases where the coordinator
    // is destroyed first, these will have been destroyed before this call.
    side_panel_controller_->RemovePageHandlerAsObserver(
        weak_factory_.GetWeakPtr());
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* session_controller = ash::SessionController::Get();
  if (session_controller) {
    session_controller->RemoveObserver(this);
  }
#endif
}

void ReadAnythingUntrustedPageHandler::PrimaryPageChanged() {
  SetUpPdfObserver();
  OnActiveAXTreeIDChanged();
}

void ReadAnythingUntrustedPageHandler::WebContentsDestroyed() {
  translate_observation_.Reset();
}

void ReadAnythingUntrustedPageHandler::AccessibilityEventReceived(
    const ui::AXUpdatesAndEvents& details) {
  page_->AccessibilityEventReceived(details.ax_tree_id, details.updates,
                                    details.events);
}

void ReadAnythingUntrustedPageHandler::AccessibilityLocationChangesReceived(
    const ui::AXTreeID& tree_id,
    ui::AXLocationAndScrollUpdates& details) {
  if (features::IsReadAnythingDocsIntegrationEnabled()) {
    page_->AccessibilityLocationChangesReceived(tree_id, details);
  }
}

///////////////////////////////////////////////////////////////////////////////
// ui::AXActionHandlerObserver:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingUntrustedPageHandler::TreeRemoved(ui::AXTreeID ax_tree_id) {
  page_->OnAXTreeDestroyed(ax_tree_id);
}

///////////////////////////////////////////////////////////////////////////////
// read_anything::mojom::UntrustedPageHandler:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingUntrustedPageHandler::GetDependencyParserModel(
    GetDependencyParserModelCallback callback) {
  DependencyParserModelLoader* loader =
      DependencyParserModelLoaderFactory::GetForProfile(profile_);
  if (!loader) {
    std::move(callback).Run(base::File());
    return;
  }

  // If the model file is unavailable, request the dependency parser loader to
  // notify this instance when it becomes available. The two-step process is to
  // ensure that the model file and callback lifetimes are carefully managed, so
  // they are not freed without being handled on the appropriate thread,
  // particularly for the model file.
  // TODO(b/339037155): Investigate the feasibility of moving this logic into
  // the dependency parser model loader.
  if (!loader->IsModelAvailable()) {
    loader->NotifyOnModelFileAvailable(
        base::BindOnce(&ReadAnythingUntrustedPageHandler::
                           OnDependencyParserModelFileAvailabilityChanged,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  OnDependencyParserModelFileAvailabilityChanged(std::move(callback), true);
}

void ReadAnythingUntrustedPageHandler::GetVoicePackInfo(
    const std::string& language,
    read_anything::mojom::UntrustedPageHandler::GetVoicePackInfoCallback
        mojo_remote_callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  LanguagePackManager::GetPackState(
      ash::language_packs::kTtsFeatureId, language,
      base::BindOnce(&OnLanguagePackManagerResponse,
                     std::move(mojo_remote_callback)));
#else
  //  TODO (b/40927698) Implement high quality voice support for non ChromeOS
  //  platforms. For now, just return that all high quality voices are
  //  unavailable.
  auto voicePackInfo = read_anything::mojom::VoicePackInfo::New();
  voicePackInfo->language = language;
  voicePackInfo->pack_state =
      VoicePackInstallationState::NewErrorCode(ErrorCode::kUnsupportedPlatform);
  std::move(mojo_remote_callback).Run(std::move(voicePackInfo));
#endif
}

void ReadAnythingUntrustedPageHandler::InstallVoicePack(
    const std::string& language,
    read_anything::mojom::UntrustedPageHandler::InstallVoicePackCallback
        mojo_remote_callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  LanguagePackManager::InstallPack(
      ash::language_packs::kTtsFeatureId, language,
      base::BindOnce(&OnLanguagePackManagerResponse,
                     std::move(mojo_remote_callback)));
#else
  //  TODO (b/40927698) Implement high quality voice support for non ChromeOS
  //  platforms. For now, just return that all high quality voices are
  //  unavailable.
  auto voicePackInfo = read_anything::mojom::VoicePackInfo::New();
  voicePackInfo->language = language;
  voicePackInfo->pack_state =
      VoicePackInstallationState::NewErrorCode(ErrorCode::kUnsupportedPlatform);
  std::move(mojo_remote_callback).Run(std::move(voicePackInfo));
#endif
}

void ReadAnythingUntrustedPageHandler::OnCopy() {
  if (main_observer_ && main_observer_->web_contents()) {
    main_observer_->web_contents()->Copy();
  }
}

void ReadAnythingUntrustedPageHandler::OnLineSpaceChange(
    read_anything::mojom::LineSpacing line_spacing) {
  profile_->GetPrefs()->SetInteger(prefs::kAccessibilityReadAnythingLineSpacing,
                                   static_cast<size_t>(line_spacing));
}

void ReadAnythingUntrustedPageHandler::OnLetterSpaceChange(
    read_anything::mojom::LetterSpacing letter_spacing) {
  profile_->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingLetterSpacing,
      static_cast<size_t>(letter_spacing));
}
void ReadAnythingUntrustedPageHandler::OnFontChange(const std::string& font) {
  profile_->GetPrefs()->SetString(prefs::kAccessibilityReadAnythingFontName,
                                  font);
}

void ReadAnythingUntrustedPageHandler::OnFontSizeChange(double font_size) {
  double saved_font_size = std::min(font_size, kReadAnythingMaximumFontScale);
  profile_->GetPrefs()->SetDouble(prefs::kAccessibilityReadAnythingFontScale,
                                  saved_font_size);
}

void ReadAnythingUntrustedPageHandler::OnLinksEnabledChanged(bool enabled) {
  profile_->GetPrefs()->SetBoolean(
      prefs::kAccessibilityReadAnythingLinksEnabled, enabled);
}

void ReadAnythingUntrustedPageHandler::OnImagesEnabledChanged(bool enabled) {
  profile_->GetPrefs()->SetBoolean(
      prefs::kAccessibilityReadAnythingImagesEnabled, enabled);
}

void ReadAnythingUntrustedPageHandler::OnColorChange(
    read_anything::mojom::Colors color) {
  profile_->GetPrefs()->SetInteger(prefs::kAccessibilityReadAnythingColorInfo,
                                   static_cast<size_t>(color));
}

void ReadAnythingUntrustedPageHandler::OnSpeechRateChange(double rate) {
  profile_->GetPrefs()->SetDouble(prefs::kAccessibilityReadAnythingSpeechRate,
                                  rate);
}
void ReadAnythingUntrustedPageHandler::OnVoiceChange(const std::string& voice,
                                                     const std::string& lang) {
  PrefService* prefs = profile_->GetPrefs();
  if (features::IsReadAloudAutoVoiceSwitchingEnabled()) {
    ScopedDictPrefUpdate update(prefs,
                                prefs::kAccessibilityReadAnythingVoiceName);
    update->Set(lang, voice);
  } else {
    prefs->SetString(prefs::kAccessibilityReadAnythingVoiceName, voice);
  }
}

void ReadAnythingUntrustedPageHandler::OnLanguagePrefChange(
    const std::string& lang,
    bool enabled) {
  PrefService* prefs = profile_->GetPrefs();
  ScopedListPrefUpdate update(
      prefs, prefs::kAccessibilityReadAnythingLanguagesEnabled);
  if (enabled) {
    update->Append(lang);
  } else {
    update->EraseValue(base::Value(lang));
  }
}

void ReadAnythingUntrustedPageHandler::OnHighlightGranularityChanged(
    read_anything::mojom::HighlightGranularity granularity) {
  profile_->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingHighlightGranularity,
      static_cast<size_t>(granularity));
}

void ReadAnythingUntrustedPageHandler::OnLinkClicked(
    const ui::AXTreeID& target_tree_id,
    ui::AXNodeID target_node_id) {
  ui::AXActionData action_data;
  action_data.target_tree_id = target_tree_id;
  action_data.action = ax::mojom::Action::kDoDefault;
  action_data.target_node_id = target_node_id;

  PerformActionInTargetTree(action_data);
}

void ReadAnythingUntrustedPageHandler::OnImageDataRequested(
    const ui::AXTreeID& target_tree_id,
    ui::AXNodeID target_node_id) {
  main_observer_->web_contents()->DownloadImageFromAxNode(
      target_tree_id, target_node_id,
      /*preferred_size=*/gfx::Size(),
      /*max_bitmap_size=*/0, /*bypass_cache=*/false,
      base::BindOnce(&ReadAnythingUntrustedPageHandler::OnImageDataDownloaded,
                     weak_factory_.GetWeakPtr(), target_tree_id,
                     target_node_id));
}

void ReadAnythingUntrustedPageHandler::OnImageDataDownloaded(
    const ui::AXTreeID& target_tree_id,
    ui::AXNodeID node_id,
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& sizes) {
  bool download_was_successful =
      network::IsSuccessfulStatus(http_status_code) || http_status_code == 0;

  if (!download_was_successful || bitmaps.empty()) {
    // If there was a failure, leave the canvas empty.
    return;
  }
  // There should be at least one image.
  const auto& bitmap = bitmaps[0];
  if (bitmap.isNull()) {
    // If there was a failure, leave the canvas empty.
    return;
  }
  page_->OnImageDataDownloaded(target_tree_id, node_id, bitmap);
}

void ReadAnythingUntrustedPageHandler::ScrollToTargetNode(
    const ui::AXTreeID& target_tree_id,
    ui::AXNodeID target_node_id) {
  ui::AXActionData action_data;
  action_data.target_tree_id = target_tree_id;
  action_data.target_node_id = target_node_id;
  action_data.vertical_scroll_alignment =
      ax::mojom::ScrollAlignment::kScrollAlignmentTop;
  action_data.scroll_behavior =
      ax::mojom::ScrollBehavior::kDoNotScrollIfVisible;
  action_data.action = ax::mojom::Action::kScrollToMakeVisible;

  PerformActionInTargetTree(action_data);
}

void ReadAnythingUntrustedPageHandler::PerformActionInTargetTree(
    const ui::AXActionData& data) {
  ui::AXActionHandlerBase* handler =
      ui::AXActionHandlerRegistry::GetInstance()->GetActionHandler(
          data.target_tree_id);
  if (!handler) {
    return;
  }
  handler->PerformAction(data);
}

void ReadAnythingUntrustedPageHandler::OnSelectionChange(
    const ui::AXTreeID& target_tree_id,
    ui::AXNodeID anchor_node_id,
    int anchor_offset,
    ui::AXNodeID focus_node_id,
    int focus_offset) {
  ui::AXActionData action_data;
  action_data.target_tree_id = target_tree_id;
  action_data.action = ax::mojom::Action::kSetSelection;
  action_data.anchor_node_id = anchor_node_id;
  action_data.anchor_offset = anchor_offset;
  action_data.focus_node_id = focus_node_id;
  action_data.focus_offset = focus_offset;
  ui::AXActionHandlerBase* handler =
      ui::AXActionHandlerRegistry::GetInstance()->GetActionHandler(
          target_tree_id);
  if (!handler) {
    return;
  }
  handler->PerformAction(action_data);
}

void ReadAnythingUntrustedPageHandler::OnCollapseSelection() {
  if (main_observer_ && main_observer_->web_contents()) {
    main_observer_->web_contents()->CollapseSelection();
  }
}

void ReadAnythingUntrustedPageHandler::OnScreenshotRequested() {
  if (!features::IsDataCollectionModeForScreen2xEnabled()) {
    return;
  }
  if (!main_observer_ || !main_observer_->web_contents()) {
    VLOG(2) << "The main observer didn't observe the main web contents";
    return;
  }

  if (!web_screenshotter_) {
    web_screenshotter_ = std::make_unique<ReadAnythingScreenshotter>();
  }
  VLOG(2) << "Requesting a screenshot for the main web contents";
  web_screenshotter_->RequestScreenshot(main_observer_->web_contents());
}

void ReadAnythingUntrustedPageHandler::SetDefaultLanguageCode(
    const std::string& code) {
  page_->SetLanguageCode(code);
  page_->SetDefaultLanguageCode(code);
}

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingSidePanelController::Observer:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingUntrustedPageHandler::Activate(bool active) {
  active_ = active;
}

void ReadAnythingUntrustedPageHandler::OnSidePanelControllerDestroyed() {
  side_panel_controller_ = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// screen_ai::ScreenAIInstallState::Observer:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingUntrustedPageHandler::OnScreenAIServiceInitialized(
    bool successful) {
  DCHECK(features::IsReadAnythingWithScreen2xEnabled());
  if (successful) {
    page_->ScreenAIServiceReady();
  }
}

void ReadAnythingUntrustedPageHandler::SetUpPdfObserver() {
#if BUILDFLAG(ENABLE_PDF)
  pdf_observer_.reset();
  content::WebContents* main_contents = main_observer_->web_contents();
  // TODO(crbug.com/340272378): When removing this feature flag, delete
  // `pdf_observer_` and integrate ReadAnythingWebContentsObserver with
  // ReadAnythingUntrustedPageHandler.
  if (!chrome_pdf::features::IsOopifPdfEnabled()) {
    std::vector<content::WebContents*> inner_contents =
        main_contents ? main_contents->GetInnerWebContents()
                      : std::vector<content::WebContents*>();
    // Check if this is a pdf.
    if (inner_contents.size() == 1 &&
        IsPdfExtensionOrigin(inner_contents[0]
                                 ->GetPrimaryMainFrame()
                                 ->GetLastCommittedOrigin())) {
      pdf_observer_ = std::make_unique<ReadAnythingWebContentsObserver>(
          weak_factory_.GetSafeRef(), inner_contents[0], kReadAnythingAXMode);
    }
  }
  if (features::IsPdfOcrEnabled()) {
    screen_ai::PdfOcrControllerFactory::GetForProfile(profile_)->Activate();
  }
#endif  // BUILDFLAG(ENABLE_PDF)
}

void ReadAnythingUntrustedPageHandler::OnActiveAXTreeIDChanged() {
  if (!active_) {
    page_->OnActiveAXTreeIDChanged(ui::AXTreeIDUnknown(), ukm::kInvalidSourceId,
                                   /*is_pdf=*/false);
    return;
  }

  content::WebContents* contents = !!pdf_observer_
                                       ? pdf_observer_->web_contents()
                                       : main_observer_->web_contents();
  if (!contents) {
    page_->OnActiveAXTreeIDChanged(ui::AXTreeIDUnknown(), ukm::kInvalidSourceId,
                                   /*is_pdf=*/false);
    return;
  }

  // Observe the new contents so we can get the page language once it's
  // determined.
  if (ChromeTranslateClient* translate_client =
          ChromeTranslateClient::FromWebContents(contents)) {
    translate::TranslateDriver* driver = translate_client->GetTranslateDriver();
    const std::string& source_language =
        translate_client->GetLanguageState().source_language();
    // If we're not already observing these web contents, then observe them so
    // we can get a callback when the language is determined. Otherwise, we
    // just set the language directly.
    if (!translate_observation_.IsObservingSource(driver)) {
      translate_observation_.Reset();
      translate_observation_.Observe(driver);
      // The language may have already been determined before (and then
      // unobserved), so send the language if it's not empty. If the language
      // is outdated, we'll receive a call in OnLanguageDetermined and send
      // the updated lang there.
      if (!source_language.empty()) {
        SetLanguageCode(source_language);
      }
    } else {
      SetLanguageCode(source_language);
    }
  }

#if BUILDFLAG(ENABLE_PDF)
  bool is_pdf = chrome_pdf::features::IsOopifPdfEnabled()
                    ? !!pdf::PdfViewerStreamManager::FromWebContents(contents)
                    : !!pdf_observer_;
  if (is_pdf) {
    // What happens if there are multiple such `rfhs`?
    contents->ForEachRenderFrameHost([this](content::RenderFrameHost* rfh) {
      if (rfh->GetProcess()->IsPdf()) {
        page_->OnActiveAXTreeIDChanged(rfh->GetAXTreeID(),
                                       rfh->GetPageUkmSourceId(),
                                       /*is_pdf=*/true);
      }
    });
    return;
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  page_->OnActiveAXTreeIDChanged(rfh->GetAXTreeID(), rfh->GetPageUkmSourceId(),
                                 /*is_pdf=*/false);
}

void ReadAnythingUntrustedPageHandler::SetLanguageCode(
    const std::string& code) {
  const std::string& language_code =
      (code.empty() || code == translate::kUnknownLanguageCode) ? "" : code;
  // Only send the language code if it's a new language.
  if (language_code != current_language_code_) {
    current_language_code_ = language_code;
    page_->SetLanguageCode(current_language_code_);
  }
}

void ReadAnythingUntrustedPageHandler::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  SetLanguageCode(details.adopted_language);
}

void ReadAnythingUntrustedPageHandler::OnTranslateDriverDestroyed(
    translate::TranslateDriver* driver) {
  translate_observation_.Reset();
}

void ReadAnythingUntrustedPageHandler::LogTextStyle() {
  // This is called when the side panel closes, so retrieving the values from
  // preferences won't happen very often.
  PrefService* prefs = profile_->GetPrefs();
  int maximum_font_scale_logging =
      GetNormalizedFontScale(kReadAnythingMaximumFontScale);
  double font_scale =
      prefs->GetDouble(prefs::kAccessibilityReadAnythingFontScale);
  base::UmaHistogramExactLinear(string_constants::kFontScaleHistogramName,
                                GetNormalizedFontScale(font_scale),
                                maximum_font_scale_logging + 1);
  std::string font_name =
      prefs->GetString(prefs::kAccessibilityReadAnythingFontName);
  if (fonts::kFontInfos.contains(font_name)) {
    base::UmaHistogramEnumeration(
        string_constants::kFontNameHistogramName,
        fonts::kFontInfos.at(font_name).logging_value);
  }
  read_anything::mojom::Colors color =
      static_cast<read_anything::mojom::Colors>(
          prefs->GetInteger(prefs::kAccessibilityReadAnythingColorInfo));
  base::UmaHistogramEnumeration(string_constants::kColorHistogramName, color);
  read_anything::mojom::LineSpacing line_spacing =
      static_cast<read_anything::mojom::LineSpacing>(
          prefs->GetInteger(prefs::kAccessibilityReadAnythingLineSpacing));
  base::UmaHistogramEnumeration(string_constants::kLineSpacingHistogramName,
                                line_spacing);
  read_anything::mojom::LetterSpacing letter_spacing =
      static_cast<read_anything::mojom::LetterSpacing>(
          prefs->GetInteger(prefs::kAccessibilityReadAnythingLetterSpacing));
  base::UmaHistogramEnumeration(string_constants::kLetterSpacingHistogramName,
                                letter_spacing);
}

void ReadAnythingUntrustedPageHandler::
    OnDependencyParserModelFileAvailabilityChanged(
        GetDependencyParserModelCallback callback,
        bool is_available) {
  if (!is_available) {
    std::move(callback).Run(base::File());
    return;
  }

  DependencyParserModelLoader* loader =
      DependencyParserModelLoaderFactory::GetForProfile(profile_);
  std::move(callback).Run(loader->GetDependencyParserModelFile());
}

// ash::SessionObserver
#if BUILDFLAG(IS_CHROMEOS_ASH)
void ReadAnythingUntrustedPageHandler::OnLockStateChanged(bool locked) {
  if (locked) {
    page_->OnDeviceLocked();
  }
}
#endif
