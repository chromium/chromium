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
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/accessibility/phrase_segmentation/dependency_parser_model_loader.h"
#include "chrome/browser/accessibility/phrase_segmentation/dependency_parser_model_loader_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/read_anything/read_anything.mojom-shared.h"
#include "chrome/common/read_anything/read_anything.mojom.h"
#include "chrome/common/read_anything/read_anything_util.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/content/browser/distiller_page_web_contents.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/common/locale_util.h"
#include "components/language_detection/core/constants.h"
#include "components/pdf/browser/pdf_frame_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_driver.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
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
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"
#include "components/pdf/common/pdf_util.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/public/cpp/session/session_controller.h"
#include "extensions/browser/process_manager.h"
using ash::language_packs::LanguagePackManager;
#else
#include "chrome/browser/component_updater/wasm_tts_engine_component_installer.h"
#include "chrome/browser/extensions/component_loader.h"
#endif

using content::TtsController;
using read_anything::mojom::ErrorCode;
using read_anything::mojom::InstallationState;
using read_anything::mojom::ReadAnythingDistillationState;
using read_anything::mojom::ReadAnythingPresentationState;
using read_anything::mojom::UntrustedPage;
using read_anything::mojom::UntrustedPageHandler;
using read_anything::mojom::VoicePackInstallationState;

class ReadAnythingUntrustedPageHandler::DistillerDelegate
    : public dom_distiller::ViewRequestDelegate {
 public:
  explicit DistillerDelegate(ReadAnythingUntrustedPageHandler* handler)
      : handler_(handler) {}
  ~DistillerDelegate() override = default;

  void StartDistillation(dom_distiller::DomDistillerService* service,
                         content::WebContents* contents) {
    start_time_ = base::TimeTicks::Now();
    // If existing distillation request, cancel it. This removes delegate as
    // observer of previous request and allow it to observe new request.
    viewer_handle_.reset();
    const GURL& url = contents->GetLastCommittedURL();
    viewer_handle_ = service->ViewUrlIgnoreCache(
        this,
        service->CreateDefaultDistillerPageWithHandle(
            std::make_unique<dom_distiller::SourcePageHandleWebContents>(
                contents, /*owned=*/false)),
        url);
  }

  // dom_distiller::ViewRequestDelegate:
  void OnArticleReady(
      const dom_distiller::DistilledArticleProto* article_proto) override {
    CHECK(!start_time_.is_null());
    base::UmaHistogramMediumTimes(
        "Accessibility.ReadAnything.TimeFromStartDistillationToOnArticleReady",
        base::TimeTicks::Now() - start_time_);
    handler_->ProcessDistilledArticle(article_proto);
    viewer_handle_.reset();
  }

  void OnArticleUpdated(
      dom_distiller::ArticleDistillationUpdate article_update) override {
    // Unused.
  }

 private:
  raw_ptr<ReadAnythingUntrustedPageHandler> handler_;
  std::unique_ptr<dom_distiller::ViewerHandle> viewer_handle_;
  base::TimeTicks start_time_;
};

namespace {

// All AXMode flags of kAXModeWebContentsOnly are needed. |ui::AXMode::kHTML| is
// needed for retrieveing the `aria-expanded` attribute.
// |ui::AXMode::kExtendedProperties| is needed for HTML tag, and heading level
// information. |ui::AXMode::kInlineTextBoxes| is needed for complete screen2x
// output -- if excluded, some nodes from the tree will not be identified as
// content nodes.
// TODO(crbug.com/366000250): kHTML is a heavy-handed approach as it copies all
// HTML attributes into the accessibility tree. It should be removed ASAP.
constexpr ui::AXMode kReadAnythingAXMode =
    ui::kAXModeWebContentsOnly | ui::AXMode::kHTML;

// The amount of time reading mode should wait after getting the DidStopLoading
// callback before checking if the current page is a pdf. It's possible to
// receive the callback for the page before the pdf has finished loading, which
// results in the last committed origin being invalid.
constexpr int PDF_LOAD_DELAY_MS = 1000;

// Prefix definition for logging.
constexpr char kReadAnythingPrefix[] = "Read Anything";

#if BUILDFLAG(IS_CHROMEOS)

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

// Called when LanguagePackManager::GetPackState is complete.
void OnGetPackStateResponse(
    base::OnceCallback<void(read_anything::mojom::VoicePackInfoPtr)> callback,
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

  std::move(callback).Run(std::move(voicePackInfo));
}

#else
constexpr char kReadingModeName[] = "Reading mode";

InstallationState GetInstallationStateFromStatusCode(
    const content::LanguageInstallStatus status_code) {
  switch (status_code) {
    case content::LanguageInstallStatus::NOT_INSTALLED:
      return InstallationState::kNotInstalled;
    case content::LanguageInstallStatus::INSTALLING:
      return InstallationState::kInstalling;
    case content::LanguageInstallStatus::INSTALLED:
      return InstallationState::kInstalled;
    case content::LanguageInstallStatus::FAILED:
    case content::LanguageInstallStatus::UNKNOWN:
      return InstallationState::kUnknown;
  }
}
#endif

}  // namespace

ReadAnythingWebContentsObserver::ReadAnythingWebContentsObserver(
    base::SafeRef<ReadAnythingUntrustedPageHandler> page_handler,
    content::WebContents* web_contents,
    ui::AXMode accessibility_mode)
    : page_handler_(page_handler) {
  Observe(web_contents);

  if (!web_contents) {
    return;
  }

  // Enable accessibility for the top level render frame and all descendants.
  // This causes AXTreeSerializer to reset and send accessibility events of the
  // AXTree when it is re-serialized.

  // Force a reset if web accessibility is already enabled to ensure that new
  // observers of accessibility events get the full accessibility tree from
  // scratch.
  const bool need_reset =
      web_contents->GetAccessibilityMode().has_mode(ui::AXMode::kWebContents);

  scoped_accessibility_mode_ =
      content::BrowserAccessibilityState::GetInstance()
          ->CreateScopedModeForWebContents(web_contents, accessibility_mode);

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

void ReadAnythingWebContentsObserver::DidStopLoading() {
  page_handler_->DidStopLoading();
}

void ReadAnythingWebContentsObserver::DidUpdateAudioMutingState(bool muted) {
  page_handler_->DidUpdateAudioMutingState(muted);
}

void ReadAnythingWebContentsObserver::WebContentsDestroyed() {
  page_handler_->WebContentsDestroyed();
}

void ReadAnythingUntrustedPageHandler::MaybeUpdateImmersivePinStatus() {
  if (!features::IsImmersiveReadAnythingEnabled()) {
    return;
  }
  CHECK(pinned_toolbar_);
  const bool is_pinned_in_toolbar =
      pinned_toolbar_->Contains(kActionSidePanelShowReadAnything);
  if (is_pinned_in_toolbar != immersive_read_anything_pin_state_) {
    immersive_read_anything_pin_state_ = is_pinned_in_toolbar;
    page_->OnPinStatusReceived(immersive_read_anything_pin_state_);
  }
}

void ReadAnythingUntrustedPageHandler::OnActionsChanged() {
  MaybeUpdateImmersivePinStatus();
}

ReadAnythingUntrustedPageHandler::ReadAnythingUntrustedPageHandler(
    mojo::PendingRemote<UntrustedPage> page,
    mojo::PendingReceiver<UntrustedPageHandler> receiver,
    content::WebUI* web_ui,
    bool use_screen_ai_service
#if BUILDFLAG(IS_CHROMEOS)
    ,
    std::unique_ptr<ChromeOsExtensionWrapper> extension_wrapper
#endif
    )
    : profile_(Profile::FromWebUI(web_ui)),
      web_ui_(web_ui),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      use_screen_ai_service_(use_screen_ai_service)
#if BUILDFLAG(IS_CHROMEOS)
      ,
      extension_wrapper_(std::move(extension_wrapper))
#endif
{
  ax_action_handler_observer_.Observe(
      ui::AXActionHandlerRegistry::GetInstance());

#if !BUILDFLAG(IS_CHROMEOS)
  content::TtsController::GetInstance()->AddUpdateLanguageStatusDelegate(this);

  extensions::ExtensionRegistry::Get(profile_)->AddObserver(this);
#else
  extension_wrapper_->ActivateSpeechEngine(profile_);
#endif
  if (features::IsImmersiveReadAnythingEnabled()) {
    read_anything_controller_ =
        ReadAnythingControllerGlue::FromWebContents(web_ui_->GetWebContents())
            ->controller();
    CHECK(read_anything_controller_);
    read_anything_controller_->AddObserver(this);
    tab_ = read_anything_controller_->tab();
    pinned_toolbar_ =
        PinnedToolbarActionsModel::Get(Profile::FromWebUI(web_ui));
    pinned_toolbar_actions_observation_.Observe(pinned_toolbar_);
    MaybeUpdateImmersivePinStatus();
  } else {
    side_panel_controller_ =
        ReadAnythingSidePanelControllerGlue::FromWebContents(
            web_ui_->GetWebContents())
            ->controller();
    side_panel_controller_->AddPageHandlerAsObserver(
        weak_factory_.GetWeakPtr());
    tab_ = side_panel_controller_->tab();
  }

  PrefService* prefs = profile_->GetPrefs();
  base::DictValue voices = base::DictValue();
  voices = prefs->GetDict(prefs::kAccessibilityReadAnythingVoiceName).Clone();
  read_anything::mojom::LineFocus line_focus =
      features::IsReadAnythingLineFocusEnabled()
          ? static_cast<read_anything::mojom::LineFocus>(
                prefs->GetInteger(prefs::kAccessibilityReadAnythingLineFocus))
          : read_anything::mojom::LineFocus::kDefaultValue;
  bool line_focus_enabled = line_focus != read_anything::mojom::LineFocus::kOff;
  read_anything::mojom::LineFocus last_non_disabled_line_focus =
      features::IsReadAnythingLineFocusEnabled()
          ? static_cast<read_anything::mojom::LineFocus>(prefs->GetInteger(
                prefs::kAccessibilityReadAnythingLastNonDisabledLineFocus))
          : read_anything::mojom::LineFocus::kDefaultValue;

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
      prefs->GetDouble(prefs::kAccessibilityReadAnythingSpeechRate),
      std::move(voices),
      prefs->GetList(prefs::kAccessibilityReadAnythingLanguagesEnabled).Clone(),
      static_cast<read_anything::mojom::HighlightGranularity>(prefs->GetDouble(
          prefs::kAccessibilityReadAnythingHighlightGranularity)),
      last_non_disabled_line_focus, line_focus_enabled);

  // Get user's default language to check for compatible fonts.
  language::LanguageModel* language_model =
      LanguageModelManagerFactory::GetForBrowserContext(profile_)
          ->GetPrimaryModel();
  std::string prefs_lang = language_model->GetLanguages().front().lang_code;
  prefs_lang = language::ExtractBaseLanguage(prefs_lang);
  SetDefaultLanguageCode(prefs_lang);

  if (use_screen_ai_service_) {
    screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(profile_)
        ->GetServiceStateAsync(
            screen_ai::ScreenAIServiceRouter::Service::kMainContentExtraction,
            base::BindOnce(
                &ReadAnythingUntrustedPageHandler::OnScreenAIServiceInitialized,
                weak_factory_.GetWeakPtr()));
  }

  if (features::IsReadAnythingWithReadabilityEnabled()) {
    // Set the JavaScript world ID.
    if (!dom_distiller::DistillerJavaScriptWorldIdIsSet()) {
      dom_distiller::SetDistillerJavaScriptWorldId(
          ISOLATED_WORLD_ID_CHROME_INTERNAL);
    }

    distiller_delegate_ = std::make_unique<DistillerDelegate>(this);
  }

  // Enable accessibility for the top level render frame and all descendants.
  // This causes AXTreeSerializer to reset and send accessibility events of
  // the AXTree when it is re-serialized.
  main_observer_ = std::make_unique<ReadAnythingWebContentsObserver>(
      weak_factory_.GetSafeRef(), tab_->GetContents(), kReadAnythingAXMode);
  SetUpPdfObserver();
  OnActiveAXTreeIDChanged();

#if BUILDFLAG(IS_CHROMEOS)
  auto* session_controller = ash::SessionController::Get();
  if (session_controller) {
    session_controller->AddObserver(this);
  }
#endif
}

ReadAnythingUntrustedPageHandler::~ReadAnythingUntrustedPageHandler() {
  OnReadAloudAudioStateChange(false);
#if !BUILDFLAG(IS_CHROMEOS)
  content::TtsController::GetInstance()->RemoveUpdateLanguageStatusDelegate(
      this);
  extensions::ExtensionRegistry::Get(profile_)->RemoveObserver(this);
#endif
  translate_observation_.Reset();
  web_screenshotter_.reset();
  main_observer_.reset();
  pdf_observer_.reset();
  LogTextStyle();

  if (read_anything_controller_) {
    read_anything_controller_->RemoveObserver(this);
  }
  if (side_panel_controller_) {
    // If |this| is destroyed before the |ReadAnythingSidePanelController|, then
    // remove |this| from the observer lists. In the cases where the coordinator
    // is destroyed first, these will have been destroyed before this call.
    side_panel_controller_->RemovePageHandlerAsObserver(
        weak_factory_.GetWeakPtr());
  }

#if BUILDFLAG(IS_CHROMEOS)
  auto* session_controller = ash::SessionController::Get();
  if (session_controller) {
    session_controller->RemoveObserver(this);
  }
  extension_wrapper_->ReleaseSpeechEngine(profile_);
  extension_wrapper_.reset();
#endif
}

void ReadAnythingUntrustedPageHandler::PrimaryPageChanged() {
  SetUpPdfObserver();
  OnActiveAXTreeIDChanged();
}

void ReadAnythingUntrustedPageHandler::DidStopLoading() {
  // It's possible for the value of GetLastCommittedOrigin to be invalid when
  // DidStopLoading is first received, but because of how rapidly the last
  // committed origin changes, reading mode would never receive the correct
  // callback from WebContentsObserver, even if it listened for
  // LastCommittedOrigin change events. Therefore, if the main page is not
  // recognized as a pdf after the page finishes loading, check again after
  // a small delay. This will allow PDFs to be more reliably distilled when
  // they're opened while reading mode is already opened.
  if (!CheckForPdfContentAfterLoad()) {
    timer_.Start(
        FROM_HERE, base::Milliseconds(PDF_LOAD_DELAY_MS),
        base::BindOnce(
            base::IgnoreResult(
                &ReadAnythingUntrustedPageHandler::CheckForPdfContentAfterLoad),
            base::Unretained(this)));
  }
}

bool ReadAnythingUntrustedPageHandler::CheckForPdfContentAfterLoad() {
  // If this page was previously recognized as not a pdf from the original
  // call to PrimaryPageChanged() but it's now recognized as a PDF after the
  // page has finished loaded, notify the page of the new tree as a PDF.
  if (!is_pdf_with_frame_) {
    SetUpPdfObserver();
    CheckIfActiveAXTreeChangedToPdf();
  }
  return is_pdf_with_frame_;
}

void ReadAnythingUntrustedPageHandler::DidUpdateAudioMutingState(bool muted) {
  page_->OnTabMuteStateChange(muted);
}

bool ReadAnythingUntrustedPageHandler::AreInnerContentsPdfContent(
    std::vector<content::WebContents*> inner_contents) {
#if BUILDFLAG(ENABLE_PDF)
  return inner_contents.size() == 1 &&
         IsPdfExtensionOrigin(inner_contents[0]
                                  ->GetPrimaryMainFrame()
                                  ->GetLastCommittedOrigin());
#else
  return false;
#endif
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

#if !BUILDFLAG(IS_CHROMEOS)
void ReadAnythingUntrustedPageHandler::OnUpdateLanguageStatus(
    content::BrowserContext* browser_context,
    const std::string& language,
    content::LanguageInstallStatus install_status,
    const std::string& error) {
  // Language status is profile-dependent so only send the update if the status
  // is for this profile. Incognito profiles download the language to the main
  // profile, so we need to always send the language updates for incognito.
  // Guest profiles don't have matching IDs, so if this profile is a guest and
  // the profile sending the language status is a guest, then we do send the
  // status update.
  Profile* statusProfile = Profile::FromBrowserContext(browser_context);
  const bool shouldSendGuestStatus =
      statusProfile->IsGuestSession() && profile_->IsGuestSession();
  if (!shouldSendGuestStatus && !profile_->IsIncognitoProfile() &&
      statusProfile->UniqueId() != profile_->UniqueId()) {
    return;
  }
  auto voicePackInfo = read_anything::mojom::VoicePackInfo::New();
  voicePackInfo->language = language;
  voicePackInfo->pack_state = VoicePackInstallationState::NewInstallationState(
      GetInstallationStateFromStatusCode(install_status));
  OnGetVoicePackInfo(std::move(voicePackInfo));
}

void ReadAnythingUntrustedPageHandler::OnExtensionReady(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  const auto& extensionId =
      extension_misc::kComponentUpdaterTTSEngineExtensionId;
  if (extension->id() != extensionId) {
    return;
  }
  VLOG(1) << "TTS component extension ready";
  page_->OnTtsEngineInstalled();
}

#else

// Called when LanguagePackManager::InstallPack is complete.
void ReadAnythingUntrustedPageHandler::OnInstallPackResponse(
    const PackResult& pack_result) {
  // Convert the LanguagePackManager's response object into a mojo object
  read_anything::mojom::VoicePackInfoPtr voicePackInfo =
      read_anything::mojom::VoicePackInfo::New();

  // TODO(crbug.com/40927698): Investigate the fact that VoicePackManager
  // doesn't return the expected pack_state. Even when a voice is unavailable
  // and not installed, it responds "INSTALLED" in the InstallVoicePackCallback.
  // So we probably need to rely on GetVoicePackInfo for the pack_state.
  if (pack_result.operation_error == PackResult::ErrorCode::kNone) {
    LanguageRequest request;
    request.language = pack_result.language_code;
    request.type = LanguageRequestType::kInfo;
    // Put this request at the front since it's a continuation of the current
    // request.
    queued_language_requests_.emplace_front(request);
    has_pending_language_request_ = false;
    SendNextLanguageRequest();
    return;
  }

  voicePackInfo->pack_state = VoicePackInstallationState::NewErrorCode(
      GetMojoErrorFromPackError(pack_result.operation_error));
  voicePackInfo->language = pack_result.language_code;
  OnGetVoicePackInfo(std::move(voicePackInfo));
}

void ReadAnythingUntrustedPageHandler::SendOrQueueLanguageRequest(
    LanguageRequest request) {
  queued_language_requests_.emplace_back(request);
  if (!has_pending_language_request_) {
    SendNextLanguageRequest();
  }
}

void ReadAnythingUntrustedPageHandler::SendNextLanguageRequest() {
  // If we're already waiting for a response for another language, do nothing.
  // The next language will be queued up once this one is complete.
  if (has_pending_language_request_ || queued_language_requests_.empty()) {
    return;
  }

  // Otherwise send the corresponding request for the next language in the
  // queue. The pending language will be cleared once we receive the response
  // in OnGetVoicePackInfo.
  has_pending_language_request_ = true;
  LanguageRequest request = queued_language_requests_.front();
  queued_language_requests_.pop_front();
  if (request.type == LanguageRequestType::kInfo) {
    extension_wrapper_->RequestLanguageInfo(
        request.language,
        base::BindOnce(
            &OnGetPackStateResponse,
            base::BindOnce(
                &ReadAnythingUntrustedPageHandler::OnGetVoicePackInfo,
                weak_factory_.GetWeakPtr())));
  } else if (request.type == LanguageRequestType::kInstall) {
    extension_wrapper_->RequestLanguageInstall(
        request.language,
        base::BindOnce(&ReadAnythingUntrustedPageHandler::OnInstallPackResponse,
                       weak_factory_.GetWeakPtr()));
  }
}
#endif

// Will only return a valid state if IsImmersiveReadAnythingEnabled() is true,
// otherwise do nothing.
// TODO(crbug.com/463728166): Remove IsImmersiveReadAnythingEnabled flag when no
// longer flag-guarded code.
ReadAnythingController*
ReadAnythingUntrustedPageHandler::GetReadAnythingController() {
  if (features::IsImmersiveReadAnythingEnabled()) {
    content::WebContents* main_web_contents = main_observer_->web_contents();
    CHECK(main_web_contents);

    tabs::TabInterface* tab =
        tabs::TabInterface::GetFromContents(main_web_contents);
    CHECK(tab);

    auto* ra_controller = ReadAnythingController::From(tab);
    return ra_controller;
  }
  return nullptr;
}

void ReadAnythingUntrustedPageHandler::OnGetPresentationState() {
  if (features::IsImmersiveReadAnythingEnabled()) {
    auto* ra_controller = GetReadAnythingController();
    CHECK(ra_controller);

    page_->OnGetPresentationState(ra_controller->GetPresentationState());
  }
}

void ReadAnythingUntrustedPageHandler::GetPresentationState() {
  OnGetPresentationState();
}

void ReadAnythingUntrustedPageHandler::OnDistillationStateChanged(
    read_anything::mojom::ReadAnythingDistillationState new_state) {
  if (features::IsImmersiveReadAnythingEnabled()) {
    // Distillation state transitions to kNotAttempted are only valid during
    // initialization (i.e. when the current state is kUndefined).
    if (distillation_state_ !=
            read_anything::mojom::ReadAnythingDistillationState::kUndefined &&
        new_state == read_anything::mojom::ReadAnythingDistillationState::
                         kNotAttempted) {
      mojo::ReportBadMessage("Invalid distillation state transition");
      return;
    }

    // Distillation state transitions to kUndefined are not valid, regardless of
    // what the current state is.
    if (new_state ==
        read_anything::mojom::ReadAnythingDistillationState::kUndefined) {
      mojo::ReportBadMessage("Invalid distillation state transition");
      return;
    }

    distillation_state_ = new_state;
    auto* ra_controller = GetReadAnythingController();
    CHECK(ra_controller);

    ra_controller->OnDistillationStateChanged(new_state);
  }
}

void ReadAnythingUntrustedPageHandler::OnGetVoicePackInfo(
    read_anything::mojom::VoicePackInfoPtr info) {
#if BUILDFLAG(IS_CHROMEOS)
  has_pending_language_request_ = false;
  if (!queued_language_requests_.empty()) {
    SendNextLanguageRequest();
  }
#endif
  page_->OnGetVoicePackInfo(std::move(info));
}

void ReadAnythingUntrustedPageHandler::GetVoicePackInfo(
    const std::string& language) {
#if BUILDFLAG(IS_CHROMEOS)
  LanguageRequest request;
  request.language = language;
  request.type = LanguageRequestType::kInfo;
  SendOrQueueLanguageRequest(request);
#else
  TtsController::GetInstance()->LanguageStatusRequest(
      profile_, language, kReadingModeName,
      static_cast<int>(tts_engine_events::TtsClientSource::CHROMEFEATURE));
#endif
}

void ReadAnythingUntrustedPageHandler::InstallVoicePack(
    const std::string& language) {
#if BUILDFLAG(IS_CHROMEOS)
  LanguageRequest request;
  request.language = language;
  request.type = LanguageRequestType::kInstall;
  SendOrQueueLanguageRequest(request);
#else
  TtsController::GetInstance()->InstallLanguageRequest(
      profile_, language, kReadingModeName,
      static_cast<int>(tts_engine_events::TtsClientSource::CHROMEFEATURE));
#endif
}

void ReadAnythingUntrustedPageHandler::UninstallVoice(
    const std::string& language) {
#if !BUILDFLAG(IS_CHROMEOS)
  TtsController::GetInstance()->UninstallLanguageRequest(
      profile_, language, kReadingModeName,
      static_cast<int>(tts_engine_events::TtsClientSource::CHROMEFEATURE),
      /*uninstall_immediately=*/false);
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
                                   std::to_underlying(line_spacing));
}

void ReadAnythingUntrustedPageHandler::OnLetterSpaceChange(
    read_anything::mojom::LetterSpacing letter_spacing) {
  profile_->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingLetterSpacing,
      std::to_underlying(letter_spacing));
}
void ReadAnythingUntrustedPageHandler::OnFontChange(const std::string& font) {
  profile_->GetPrefs()->SetString(prefs::kAccessibilityReadAnythingFontName,
                                  font);
}

void ReadAnythingUntrustedPageHandler::OnFontSizeChange(double font_size) {
  profile_->GetPrefs()->SetDouble(prefs::kAccessibilityReadAnythingFontScale,
                                  AdjustFontScale(font_size, 0));
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
                                   std::to_underlying(color));
}

void ReadAnythingUntrustedPageHandler::OnSpeechRateChange(double rate) {
  profile_->GetPrefs()->SetDouble(prefs::kAccessibilityReadAnythingSpeechRate,
                                  rate);
}
void ReadAnythingUntrustedPageHandler::OnVoiceChange(const std::string& voice,
                                                     const std::string& lang) {
  PrefService* prefs = profile_->GetPrefs();
  ScopedDictPrefUpdate update(prefs,
                              prefs::kAccessibilityReadAnythingVoiceName);
  update->Set(lang, voice);
}

void ReadAnythingUntrustedPageHandler::OnLanguagePrefChange(
    const std::string& lang,
    bool enabled) {
  PrefService* prefs = profile_->GetPrefs();
  ScopedListPrefUpdate update(
      prefs, prefs::kAccessibilityReadAnythingLanguagesEnabled);
  if (enabled) {
    if (!update.Get().contains(lang)) {
      update->Append(lang);
    }
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

void ReadAnythingUntrustedPageHandler::OnLineFocusChanged(
    read_anything::mojom::LineFocus line_focus) {
  if (features::IsReadAnythingLineFocusEnabled()) {
    profile_->GetPrefs()->SetInteger(prefs::kAccessibilityReadAnythingLineFocus,
                                     static_cast<size_t>(line_focus));
    if (line_focus != read_anything::mojom::LineFocus::kOff) {
      profile_->GetPrefs()->SetInteger(
          prefs::kAccessibilityReadAnythingLastNonDisabledLineFocus,
          static_cast<size_t>(line_focus));
    }
  }
}

void ReadAnythingUntrustedPageHandler::OnReadAloudAudioStateChange(
    bool playing) {
  // Show the tab audio icon when read aloud is playing, and hide it when it
  // stops playing.
  content::WebContents* contents = !!pdf_observer_
                                       ? pdf_observer_->web_contents()
                                       : main_observer_->web_contents();
  if (contents) {
    if (playing) {
      audible_closure_ = contents->MarkAudible();
    } else {
      audible_closure_.RunAndReset();
    }
  }
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

void ReadAnythingUntrustedPageHandler::CloseUI() {
  if (!features::IsImmersiveReadAnythingEnabled()) {
    return;
  }
  CHECK(read_anything_controller_);
  DCHECK(read_anything_controller_->GetPresentationState() ==
         ReadAnythingController::PresentationState::kInImmersiveOverlay);
  read_anything_controller_->CloseImmersiveUI();
}

void ReadAnythingUntrustedPageHandler::TogglePinState() {
  if (!features::IsImmersiveReadAnythingEnabled()) {
    return;
  }
  CHECK(pinned_toolbar_);
  immersive_read_anything_pin_state_ = !immersive_read_anything_pin_state_;
  pinned_toolbar_->UpdatePinnedState(kActionSidePanelShowReadAnything,
                                     immersive_read_anything_pin_state_);
}

void ReadAnythingUntrustedPageHandler::SendPinStateRequest() {
  page_->OnPinStatusReceived(immersive_read_anything_pin_state_);
}

void ReadAnythingUntrustedPageHandler::TogglePresentation() {
  if (features::IsImmersiveReadAnythingEnabled()) {
    CHECK(read_anything_controller_);
    read_anything_controller_->TogglePresentation();
  }
}

void ReadAnythingUntrustedPageHandler::AckReadingModeHidden() {
  if (features::IsImmersiveReadAnythingEnabled()) {
    ack_timed_out_for_testing_ = false;
    reading_mode_hidden_ack_timer_.Stop();
  }
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

void ReadAnythingUntrustedPageHandler::OnDistillationStatus(
    read_anything::mojom::DistillationStatus status,
    int word_count) {
  if (last_open_trigger_.has_value() &&
      last_open_trigger_.value() == ReadAnythingOpenTrigger::kOmniboxChip) {
    last_open_trigger_.reset();
    base::UmaHistogramEnumeration(
        "Accessibility.ReadAnything.DistillationStatusAfterOmnibox", status);
    base::UmaHistogramCustomCounts(
        "Accessibility.ReadAnything.WordsDistilledAfterOmnibox", word_count, 1,
        kMaxWordsDistilled, kWordsDistilledBuckets);
  }
}

void ReadAnythingUntrustedPageHandler::SetDefaultLanguageCode(
    const std::string& code) {
  page_->SetLanguageCode(code);
  page_->SetDefaultLanguageCode(code);
}

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingLifecycleObserver:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingUntrustedPageHandler::Activate(
    bool active,
    std::optional<ReadAnythingOpenTrigger> open_trigger) {
  active_ = active;
  if (active_) {
    last_open_trigger_ = open_trigger;
    tab_will_detach_ = false;
  }
  if (!active && !tab_will_detach_) {
    page_->OnReadingModeHidden(tab_->IsActivated());

    // When Reading mode is hidden (with immersive flag enabled), we need to
    // verify that the renderer is still responsive. Waiting until the mojo
    // disconnects is slow and would cause a crash. If the renderer is
    // unresponsive, then notify the controller that it should recreate the
    // WebUI wrapper, otherwise it's never torn down once it's created, and
    // we'll be stuck in an unresponsive state. We do this when Reading mode is
    // hidden because if the user notices a crash they will likely try to close
    // and reopen RM. Detecting a crash programmatically is often slower than
    // the user noticing, so this handles that case.
    if (features::IsImmersiveReadAnythingEnabled()) {
      reading_mode_hidden_ack_timer_.Start(
          FROM_HERE, kReadingModeHiddenAckTimeout,
          base::BindOnce(
              &ReadAnythingUntrustedPageHandler::OnReadingModeHiddenAckTimeout,
              base::Unretained(this)));
    }
  }
}

void ReadAnythingUntrustedPageHandler::OnReadingModeHiddenAckTimeout() {
  if (!features::IsImmersiveReadAnythingEnabled()) {
    return;
  }

  ack_timed_out_for_testing_ = true;
  CHECK(read_anything_controller_);
  read_anything_controller_->RecreateWebUIWrapper();
}

void ReadAnythingUntrustedPageHandler::OnReadingModePresenterChanged() {
  OnGetPresentationState();
}

void ReadAnythingUntrustedPageHandler::OnDestroyed() {
  side_panel_controller_ = nullptr;
  read_anything_controller_ = nullptr;
}

void ReadAnythingUntrustedPageHandler::OnTabWillDetach() {
  OnReadAloudAudioStateChange(false);

  // When multiple tabs are open, we receive this call multiple times, so only
  // inform once.
  if (!tab_will_detach_) {
    tab_will_detach_ = true;
    page_->OnTabWillDetach();
  }
}

///////////////////////////////////////////////////////////////////////////////
// screen_ai::ScreenAIInstallState::Observer:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingUntrustedPageHandler::OnScreenAIServiceInitialized(
    bool successful) {
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
    if (AreInnerContentsPdfContent(inner_contents)) {
      pdf_observer_ = std::make_unique<ReadAnythingWebContentsObserver>(
          weak_factory_.GetSafeRef(), inner_contents[0], kReadAnythingAXMode);
    }
  }
#endif  // BUILDFLAG(ENABLE_PDF)
}

void ReadAnythingUntrustedPageHandler::CheckIfActiveAXTreeChangedToPdf() {
#if BUILDFLAG(ENABLE_PDF)
  content::WebContents* contents = !!pdf_observer_
                                       ? pdf_observer_->web_contents()
                                       : main_observer_->web_contents();
  bool are_contents_pdf =
      chrome_pdf::features::IsOopifPdfEnabled()
          ? !!pdf::PdfViewerStreamManager::FromWebContents(contents)
          : !!pdf_observer_;
  if (!are_contents_pdf) {
    return;
  }

  content::RenderFrameHost* pdf_rfh =
      chrome_pdf::features::IsOopifPdfEnabled()
          ? pdf_frame_util::FindFullPagePdfExtensionHost(contents)
          : pdf_frame_util::FindPdfChildFrame(contents->GetPrimaryMainFrame());
  if (pdf_rfh) {
    is_pdf_with_frame_ = true;
    is_waiting_for_pdf_frame_ = false;
    VLOG(1) << "Sending pdf tree with id " << pdf_rfh->GetAXTreeID();
    page_->OnActiveAXTreeIDChanged(
        pdf_rfh->GetAXTreeID(), pdf_rfh->GetPageUkmSourceId(), /*is_pdf=*/true);
  } else {
    VLOG(1) << "Page is a pdf, but has no pdf frame yet";
    is_waiting_for_pdf_frame_ = true;
  }
#endif  // BUILDFLAG(ENABLE_PDF)
}

void ReadAnythingUntrustedPageHandler::OnActiveAXTreeIDChanged() {
  is_pdf_with_frame_ = false;
  // If the side panel is not active, we should not send the active tree id.
  // This check is skipped when immersive read anything is enabled because
  // there are times when the side panel is inactive but the Reading Mode
  // application is still running, so we do need to send the active tree id.
  if (!active_ && !features::IsImmersiveReadAnythingEnabled()) {
    VLOG(1) << "Sending unknown tree because not active";
    page_->OnActiveAXTreeIDChanged(ui::AXTreeIDUnknown(), ukm::kInvalidSourceId,
                                   /*is_pdf=*/false);
    return;
  }

  content::WebContents* contents = !!pdf_observer_
                                       ? pdf_observer_->web_contents()
                                       : main_observer_->web_contents();
  if (!contents) {
    VLOG(1) << "Sending unknown tree because no contents. Used pdf: "
            << !!pdf_observer_;
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
  CheckIfActiveAXTreeChangedToPdf();
  // If is_waiting_for_pdf_frame_ is true, we know the current page is a pdf,
  // but we don't have the necessary info to call OnActiveAXTreeIDChanged
  // accurately, so wait until the pdf frame is loaded.
  if (is_pdf_with_frame_ || is_waiting_for_pdf_frame_) {
    return;
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  // When IsReadAnythingWithReadabilityEnabled is true, we still send AX tree
  // for text selection.
  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  VLOG(1) << "Sending non-pdf tree with id " << rfh->GetAXTreeID();
  page_->OnActiveAXTreeIDChanged(rfh->GetAXTreeID(), rfh->GetPageUkmSourceId(),
                                 /*is_pdf=*/false);

  RequestDomDistillerDistillation(contents);
}

void ReadAnythingUntrustedPageHandler::RequestDomDistillerDistillation(
    content::WebContents* content) {
  if (!features::IsReadAnythingWithReadabilityEnabled() || is_pdf_with_frame_) {
    return;
  }

  // Don't attempt Readability distillation in automated tests. This is to prevent internal
  // scripts from leaking.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAutomation)) {
    page_->OnReadabilityDistillationStateChanged(
        read_anything::mojom::ReadAnythingDistillationState::
            kDistillationEmpty);
    page_->UpdateContent("", "");
    return;
  }

  const GURL& url = content->GetLastCommittedURL();
  RecordDistillationSchemeHistogram(url);

  if (!url.SchemeIsHTTPOrHTTPS()) {
    VLOG(1) << kReadAnythingPrefix << ": URL is not HTTP/HTTPS, skipping for "
            << url.spec();
    // Call UpdateContent with empty values so status can be updated from show
    // loading to no content.
    page_->OnReadabilityDistillationStateChanged(
        read_anything::mojom::ReadAnythingDistillationState::
            kDistillationEmpty);
    page_->UpdateContent("", "");
    return;
  }

  dom_distiller::DomDistillerService* dom_distiller_service =
      dom_distiller::DomDistillerServiceFactory::GetForBrowserContext(
          content->GetBrowserContext());
  DCHECK(dom_distiller_service);
  page_->OnReadabilityDistillationStateChanged(
      read_anything::mojom::ReadAnythingDistillationState::
          kDistillationInProgress);
  distiller_delegate_->StartDistillation(dom_distiller_service, content);
}

void ReadAnythingUntrustedPageHandler::RecordDistillationSchemeHistogram(
    const GURL& url) const {
  ReadAnythingDistillationScheme scheme =
      ReadAnythingDistillationScheme::kOther;

  if (url.SchemeIsHTTPOrHTTPS()) {
    scheme = ReadAnythingDistillationScheme::kHttpOrHttps;
  } else if (url.SchemeIsFile()) {
    scheme = ReadAnythingDistillationScheme::kFile;
  } else if (url.SchemeIs(url::kDataScheme)) {
    scheme = ReadAnythingDistillationScheme::kData;
  } else if (url.SchemeIs(extensions::kExtensionScheme)) {
    scheme = ReadAnythingDistillationScheme::kExtension;
  } else if (url.IsAboutBlank() || url.IsAboutSrcdoc()) {
    scheme = ReadAnythingDistillationScheme::kAbout;
  } else if (url.SchemeIsBlob()) {
    scheme = ReadAnythingDistillationScheme::kBlob;
  } else if (url.SchemeIs(content::kChromeUIScheme)) {
    scheme = ReadAnythingDistillationScheme::kInternal;
  }

  base::UmaHistogramEnumeration("Accessibility.ReadAnything.DistillationScheme",
                                scheme);
}

void ReadAnythingUntrustedPageHandler::ProcessDistilledArticle(
    const dom_distiller::DistilledArticleProto* article_proto) {
  CHECK(features::IsReadAnythingWithReadabilityEnabled() &&
        !is_pdf_with_frame_);
  if (article_proto && article_proto->pages_size() > 0) {
    dom_distiller_title_ = article_proto->title();

    std::string full_html;
    for (const auto& page : article_proto->pages()) {
      full_html.append(page.html());
    }
    dom_distiller_content_ = full_html;

    // If distillation successfully produced content, update the distillation
    // state and notify the renderer.
    if (dom_distiller_content()) {
      page_->OnReadabilityDistillationStateChanged(
          read_anything::mojom::ReadAnythingDistillationState::
              kDistillationWithContent);
      page_->UpdateContent(dom_distiller_title().value_or(""),
                           dom_distiller_content().value());
    }
  } else {
    page_->OnReadabilityDistillationStateChanged(
        read_anything::mojom::ReadAnythingDistillationState::
            kDistillationEmpty);
    page_->UpdateContent(/*title=*/"", /*content=*/"");
  }
}

void ReadAnythingUntrustedPageHandler::SetLanguageCode(
    const std::string& code) {
  const std::string& language_code =
      (code.empty() || code == language_detection::kUnknownLanguageCode) ? ""
                                                                         : code;
  // Only send the language code if it's a new language, unless it's an empty
  // code. Always send an empty code so we know to use the tree language.
  if (language_code.empty() || (language_code != current_language_code_)) {
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

void ReadAnythingUntrustedPageHandler::LogExtensionState() {
#if !BUILDFLAG(IS_CHROMEOS)
  // A system voice.
  EngineInstallationState installation_state;
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  if (registry->enabled_extensions().Contains(
          extension_misc::kComponentUpdaterTTSEngineExtensionId)) {
    installation_state = EngineInstallationState::kEnabled;
  } else if (registry->disabled_extensions().Contains(
                 extension_misc::kComponentUpdaterTTSEngineExtensionId)) {
    installation_state = EngineInstallationState::kDisabled;
  } else if (registry->terminated_extensions().Contains(
                 extension_misc::kComponentUpdaterTTSEngineExtensionId)) {
    installation_state = EngineInstallationState::kTerminated;
  } else if (registry->blocked_extensions().Contains(
                 extension_misc::kComponentUpdaterTTSEngineExtensionId)) {
    installation_state = EngineInstallationState::kBlocked;
  } else if (registry->ready_extensions().Contains(
                 extension_misc::kComponentUpdaterTTSEngineExtensionId)) {
    installation_state = EngineInstallationState::kReady;
  } else if (component_updater::WasmTtsEngineComponentInstallerPolicy::
                 IsWasmTTSEngineDirectorySet()) {
    installation_state = EngineInstallationState::kInstalling;
  } else {
    installation_state = EngineInstallationState::kUnknown;
  }

  base::UmaHistogramEnumeration(
      "Accessibility.ReadAnything."
      "SystemVoiceExtensionInstallationState",
      installation_state);
#endif
}

void ReadAnythingUntrustedPageHandler::LogTextStyle() {
  // This is called when the side panel closes, so retrieving the values from
  // preferences won't happen very often.
  PrefService* prefs = profile_->GetPrefs();
  LogFontScale(prefs->GetDouble(prefs::kAccessibilityReadAnythingFontScale));
  LogFontName(prefs->GetString(prefs::kAccessibilityReadAnythingFontName));
  read_anything::mojom::Colors color =
      static_cast<read_anything::mojom::Colors>(
          prefs->GetInteger(prefs::kAccessibilityReadAnythingColorInfo));
  base::UmaHistogramEnumeration("Accessibility.ReadAnything.Color", color);
  read_anything::mojom::LineSpacing line_spacing =
      static_cast<read_anything::mojom::LineSpacing>(
          prefs->GetInteger(prefs::kAccessibilityReadAnythingLineSpacing));
  base::UmaHistogramEnumeration("Accessibility.ReadAnything.LineSpacing",
                                line_spacing);
  read_anything::mojom::LetterSpacing letter_spacing =
      static_cast<read_anything::mojom::LetterSpacing>(
          prefs->GetInteger(prefs::kAccessibilityReadAnythingLetterSpacing));
  base::UmaHistogramEnumeration("Accessibility.ReadAnything.LetterSpacing",
                                letter_spacing);
  if (features::IsReadAnythingLineFocusEnabled()) {
    auto line_focus = static_cast<read_anything::mojom::LineFocus>(
        prefs->GetInteger(prefs::kAccessibilityReadAnythingLineFocus));
    base::UmaHistogramEnumeration("Accessibility.ReadAnything.LineFocus",
                                  line_focus);
  }
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
#if BUILDFLAG(IS_CHROMEOS)
void ReadAnythingUntrustedPageHandler::OnLockStateChanged(bool locked) {
  if (locked) {
    page_->OnDeviceLocked();
  }
}
#endif
