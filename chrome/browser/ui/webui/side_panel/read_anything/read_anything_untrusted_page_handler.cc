// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_page_handler.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/accessibility/pdf_ocr_controller.h"
#include "chrome/browser/accessibility/pdf_ocr_controller_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/common/accessibility/read_anything.mojom-forward.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "components/language/core/common/locale_util.h"
#include "components/pdf/common/pdf_util.h"
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
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/session/session_controller.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"
using ash::language_packs::LanguagePackManager;
using ash::language_packs::PackResult;
#endif

using read_anything::mojom::ErrorCode;
using read_anything::mojom::InstallationState;
using read_anything::mojom::ReadAnythingTheme;
using read_anything::mojom::UntrustedPage;
using read_anything::mojom::UntrustedPageHandler;
using read_anything::mojom::VoicePackInstallationState;

namespace {

// All components of kAXModeWebContentsOnly are needed. |ui::AXMode::kHTML| is
// needed for URL information. |ui::AXMode::kScreenReader| is needed for heading
// level information. |ui::AXMode::kInlineTextBoxes| is needed for complete
// Screen2x output -- if excluded, some nodes from the tree will not be
// identified as content nodes.
constexpr ui::AXMode kReadAnythingAXMode = ui::kAXModeWebContentsOnly;

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
    : browser_(chrome::FindLastActive()->AsWeakPtr()),
      web_ui_(web_ui),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  DCHECK(browser_);
  browser_->tab_strip_model()->AddObserver(this);
  ax_action_handler_observer_.Observe(
      ui::AXActionHandlerRegistry::GetInstance());

  if (features::IsReadAnythingLocalSidePanelEnabled()) {
    auto* active_web_contents =
        browser_->tab_strip_model()->GetActiveWebContents();
    ObserveWebContentsSidePanelController(active_web_contents);
  } else {
    coordinator_ = ReadAnythingCoordinator::FromBrowser(browser_.get());
    if (coordinator_) {
      coordinator_->AddObserver(this);
      coordinator_->AddModelObserver(this);
    }
  }

  if (features::IsReadAnythingWebUIToolbarEnabled()) {
    PrefService* prefs = browser_->profile()->GetPrefs();
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
        static_cast<read_anything::mojom::Colors>(
            prefs->GetInteger(prefs::kAccessibilityReadAnythingColorInfo)),
        speechRate, std::move(voices),
        features::IsReadAnythingReadAloudEnabled()
            ? prefs->GetList(prefs::kAccessibilityReadAnythingLanguagesEnabled)
                  .Clone()
            : base::Value::List(),
        highlightGranularity);
  }

  if (features::IsReadAnythingWithScreen2xEnabled()) {
    screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(
        browser_->profile())
        ->GetServiceStateAsync(
            screen_ai::ScreenAIServiceRouter::Service::kMainContentExtraction,
            base::BindOnce(
                &ReadAnythingUntrustedPageHandler::OnScreenAIServiceInitialized,
                weak_factory_.GetWeakPtr()));
  }
  if (features::IsPdfOcrEnabled()) {
    screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(
        browser_->profile())
        ->GetServiceStateAsync(screen_ai::ScreenAIServiceRouter::Service::kOCR,
                               base::DoNothing());
  }

  OnActiveWebContentsChanged();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* session_controller = ash::SessionController::Get();
  if (session_controller) {
    session_controller->AddObserver(this);
  }
#endif
}

ReadAnythingUntrustedPageHandler::~ReadAnythingUntrustedPageHandler() {
  TabStripModelObserver::StopObservingAll(this);
  translate_observation_.Reset();
  web_snapshotter_.reset();
  main_observer_.reset();
  pdf_observer_.reset();
  LogTextStyle();

  if (features::IsReadAnythingLocalSidePanelEnabled() && tab_helper_) {
    // If |this| is destroyed before the |ReadAnythingSidePanelController|, then
    // remove |this| from the observer lists. In the cases where the coordinator
    // is destroyed first, these will have been destroyed before this call.
    tab_helper_->RemovePageHandlerAsObserver(weak_factory_.GetWeakPtr());
  } else if (coordinator_) {
    // If |this| is destroyed before the |ReadAnythingCoordinator|, then remove
    // |this| from the observer lists. In the cases where the coordinator is
    // destroyed first, these will have been destroyed before this call.
    coordinator_->RemoveObserver(this);
    coordinator_->RemoveModelObserver(this);
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

///////////////////////////////////////////////////////////////////////////////
// ui::AXActionHandlerObserver:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingUntrustedPageHandler::TreeRemoved(ui::AXTreeID ax_tree_id) {
  page_->OnAXTreeDestroyed(ax_tree_id);
}

///////////////////////////////////////////////////////////////////////////////
// read_anything::mojom::UntrustedPageHandler:
///////////////////////////////////////////////////////////////////////////////

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
  if (browser_) {
    browser_->profile()->GetPrefs()->SetInteger(
        prefs::kAccessibilityReadAnythingLineSpacing,
        static_cast<size_t>(line_spacing));
  }
}

void ReadAnythingUntrustedPageHandler::OnLetterSpaceChange(
    read_anything::mojom::LetterSpacing letter_spacing) {
  if (browser_) {
    browser_->profile()->GetPrefs()->SetInteger(
        prefs::kAccessibilityReadAnythingLetterSpacing,
        static_cast<size_t>(letter_spacing));
  }
}
void ReadAnythingUntrustedPageHandler::OnFontChange(const std::string& font) {
  if (browser_) {
    browser_->profile()->GetPrefs()->SetString(
        prefs::kAccessibilityReadAnythingFontName, font);
  }
}
void ReadAnythingUntrustedPageHandler::OnFontSizeChange(double font_size) {
  double saved_font_size = std::min(font_size, kReadAnythingMaximumFontScale);
  if (browser_) {
    browser_->profile()->GetPrefs()->SetDouble(
        prefs::kAccessibilityReadAnythingFontScale, saved_font_size);
  }
}
void ReadAnythingUntrustedPageHandler::OnLinksEnabledChanged(bool enabled) {
  if (browser_) {
    browser_->profile()->GetPrefs()->SetBoolean(
        prefs::kAccessibilityReadAnythingLinksEnabled, enabled);
  }
}
void ReadAnythingUntrustedPageHandler::OnColorChange(
    read_anything::mojom::Colors color) {
  if (browser_) {
    browser_->profile()->GetPrefs()->SetInteger(
        prefs::kAccessibilityReadAnythingColorInfo, static_cast<size_t>(color));
  }
}
void ReadAnythingUntrustedPageHandler::OnSpeechRateChange(double rate) {
  if (browser_) {
    browser_->profile()->GetPrefs()->SetDouble(
        prefs::kAccessibilityReadAnythingSpeechRate, rate);
  }
}
void ReadAnythingUntrustedPageHandler::OnVoiceChange(const std::string& voice,
                                                     const std::string& lang) {
  if (browser_) {
    PrefService* prefs = browser_->profile()->GetPrefs();
    if (features::IsReadAloudAutoVoiceSwitchingEnabled()) {
      ScopedDictPrefUpdate update(prefs,
                                  prefs::kAccessibilityReadAnythingVoiceName);
      update->Set(lang, voice);
    } else {
      prefs->SetString(prefs::kAccessibilityReadAnythingVoiceName, voice);
    }
  }
}

void ReadAnythingUntrustedPageHandler::OnLanguagePrefChange(
    const std::string& lang,
    bool enabled) {
  if (browser_) {
    PrefService* prefs = browser_->profile()->GetPrefs();
    ScopedListPrefUpdate update(
        prefs, prefs::kAccessibilityReadAnythingLanguagesEnabled);
    if (enabled) {
      update->Append(lang);
    } else {
      update->EraseValue(base::Value(lang));
    }
  }
}

void ReadAnythingUntrustedPageHandler::OnHighlightGranularityChanged(
    read_anything::mojom::HighlightGranularity granularity) {
  if (browser_) {
    browser_->profile()->GetPrefs()->SetInteger(
        prefs::kAccessibilityReadAnythingHighlightGranularity,
        static_cast<size_t>(granularity));
  }
}

void ReadAnythingUntrustedPageHandler::OnLinkClicked(
    const ui::AXTreeID& target_tree_id,
    ui::AXNodeID target_node_id) {
  ui::AXActionData action_data;
  action_data.target_tree_id = target_tree_id;
  action_data.action = ax::mojom::Action::kDoDefault;
  action_data.target_node_id = target_node_id;

  PerformActionInTargetTree(target_tree_id, action_data);
}

void ReadAnythingUntrustedPageHandler::OnImageDataRequested(
    const ui::AXTreeID& target_tree_id,
    ui::AXNodeID target_node_id) {
  ui::AXActionData action_data;
  action_data.target_tree_id = target_tree_id;
  action_data.action = ax::mojom::Action::kGetImageData;
  action_data.target_node_id = target_node_id;
  // The rect size is the max size of the image;
  action_data.target_rect = gfx::Rect(gfx::Size(INT_MAX, INT_MAX));

  PerformActionInTargetTree(target_tree_id, action_data);
}

void ReadAnythingUntrustedPageHandler::PerformActionInTargetTree(
    const ui::AXTreeID& target_tree_id,
    const ui::AXActionData& data) {
  CHECK_EQ(target_tree_id, data.target_tree_id);
  ui::AXActionHandlerBase* handler =
      ui::AXActionHandlerRegistry::GetInstance()->GetActionHandler(
          target_tree_id);
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

void ReadAnythingUntrustedPageHandler::OnSnapshotRequested() {
  if (!features::IsDataCollectionModeForScreen2xEnabled()) {
    return;
  }
  if (!main_observer_ || !main_observer_->web_contents()) {
    VLOG(2) << "The main observer didn't observe the main web contents";
    return;
  }

  if (!web_snapshotter_) {
    web_snapshotter_ = std::make_unique<ReadAnythingSnapshotter>();
  }
  VLOG(2) << "Requesting a snapshot for the main web contents";
  web_snapshotter_->RequestSnapshot(main_observer_->web_contents());
}

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingModel::Observer:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingUntrustedPageHandler::OnReadAnythingThemeChanged(
    const std::string& font_name,
    double font_scale,
    bool links_enabled,
    ui::ColorId foreground_color_id,
    ui::ColorId background_color_id,
    ui::ColorId separator_color_id,
    ui::ColorId dropdown_color_id,
    ui::ColorId selected_dropdown_color_id,
    ui::ColorId focus_ring_color_id,
    read_anything::mojom::LineSpacing line_spacing,
    read_anything::mojom::LetterSpacing letter_spacing) {
  // Elsewhere in this file, `web_contents` refers to the active web contents
  // in the tab strip. In this case, `web_contents` refers to the web contents
  // hosting the WebUI.
  content::WebContents* web_contents = web_ui_->GetWebContents();
  SkColor foreground_skcolor =
      web_contents->GetColorProvider().GetColor(foreground_color_id);
  SkColor background_skcolor =
      web_contents->GetColorProvider().GetColor(background_color_id);

  page_->OnThemeChanged(ReadAnythingTheme::New(
      font_name, font_scale, links_enabled, foreground_skcolor,
      background_skcolor, line_spacing, letter_spacing));
}

void ReadAnythingUntrustedPageHandler::SetDefaultLanguageCode(
    const std::string& code) {
  default_language_code_ = code;
  page_->SetLanguageCode(code);
  page_->SetDefaultLanguageCode(code);
}

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingCoordinator::Observer:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingUntrustedPageHandler::Activate(bool active) {
  active_ = active;
  OnActiveWebContentsChanged();
}

void ReadAnythingUntrustedPageHandler::OnCoordinatorDestroyed() {
  coordinator_ = nullptr;
}

void ReadAnythingUntrustedPageHandler::OnSidePanelControllerDestroyed() {
  tab_helper_ = nullptr;
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

///////////////////////////////////////////////////////////////////////////////
// TabStripModelObserver:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingUntrustedPageHandler::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    OnActiveWebContentsChanged();
  }
}

void ReadAnythingUntrustedPageHandler::OnTabStripModelDestroyed(
    TabStripModel* tab_strip_model) {
  // If the TabStripModel is destroyed before |this|, remove |this| as an
  // observer.
  tab_strip_model->RemoveObserver(this);
}

///////////////////////////////////////////////////////////////////////////////

void ReadAnythingUntrustedPageHandler::OnActiveWebContentsChanged() {
  content::WebContents* const web_contents =
      active_ && browser_ ? browser_->tab_strip_model()->GetActiveWebContents()
                          : nullptr;

  if (features::IsReadAnythingLocalSidePanelEnabled()) {
    if (!tab_helper_ && web_contents) {
      ObserveWebContentsSidePanelController(web_contents);
    }
  }

  // Enable accessibility for the top level render frame and all descendants.
  // This causes AXTreeSerializer to reset and send accessibility events of
  // the AXTree when it is re-serialized.
  main_observer_ = std::make_unique<ReadAnythingWebContentsObserver>(
      weak_factory_.GetSafeRef(), web_contents, kReadAnythingAXMode);
  SetUpPdfObserver();
  OnActiveAXTreeIDChanged();
}

void ReadAnythingUntrustedPageHandler::SetUpPdfObserver() {
  pdf_observer_.reset();
  content::WebContents* main_contents = main_observer_->web_contents();
  std::vector<content::WebContents*> inner_contents =
      main_contents ? main_contents->GetInnerWebContents()
                    : std::vector<content::WebContents*>();
  // Check if this is a pdf.
  if (inner_contents.size() == 1 &&
      IsPdfExtensionOrigin(
          inner_contents[0]->GetPrimaryMainFrame()->GetLastCommittedOrigin())) {
    pdf_observer_ = std::make_unique<ReadAnythingWebContentsObserver>(
        weak_factory_.GetSafeRef(), inner_contents[0], kReadAnythingAXMode);
    screen_ai::PdfOcrControllerFactory::GetForProfile(browser_->profile())
        ->Activate();
  }
}

void ReadAnythingUntrustedPageHandler::OnActiveAXTreeIDChanged() {
  bool is_pdf = !!pdf_observer_;
  if (!main_observer_ || !active_) {
    page_->OnActiveAXTreeIDChanged(ui::AXTreeIDUnknown(), ukm::kInvalidSourceId,
                                   is_pdf);
    return;
  }

  content::WebContents* contents =
      is_pdf ? pdf_observer_->web_contents() : main_observer_->web_contents();
  if (!contents) {
    page_->OnActiveAXTreeIDChanged(ui::AXTreeIDUnknown(), ukm::kInvalidSourceId,
                                   is_pdf);
    return;
  }

  // Observe the new contents so we can get the page language once it's
  // determined.
  if (ChromeTranslateClient* translate_client =
          ChromeTranslateClient::FromWebContents(contents)) {
    translate::TranslateDriver* driver = translate_client->GetTranslateDriver();
    const std::string& source_language =
        translate_client->GetLanguageState().source_language();
    // If the language is empty and we're not already observing these web
    // contents, then observe them so we can get a callback when the language is
    // determined. If we are already observing them, then the language couldn't
    // be determined, so pass the empty code to SetLanguageCode. If the language
    // is not empty then the language was already determined so we pass that to
    // SetLanguageCode.
    if (source_language.empty() &&
        !translate_observation_.IsObservingSource(driver)) {
      translate_observation_.Reset();
      translate_observation_.Observe(driver);
    } else {
      SetLanguageCode(source_language);
    }
  }

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

  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  if (!rfh) {
    // THis case doesn't seem possible.
    page_->OnActiveAXTreeIDChanged(ui::AXTreeIDUnknown(), ukm::kInvalidSourceId,
                                   /*is_pdf=*/false);
    return;
  }

  page_->OnActiveAXTreeIDChanged(rfh->GetAXTreeID(), rfh->GetPageUkmSourceId(),
                                 /*is_pdf=*/false);
}

void ReadAnythingUntrustedPageHandler::SetLanguageCode(
    const std::string& code) {
  const std::string& language_code =
      (code.empty() || code == translate::kUnknownLanguageCode)
          ? default_language_code_
          : code;
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
  if (!browser_) {
    return;
  }

  // This is called when the side panel closes, so retrieving the values from
  // preferences won't happen very often.
  PrefService* prefs = browser_->profile()->GetPrefs();
  int maximum_font_scale_logging =
      GetNormalizedFontScale(kReadAnythingMaximumFontScale);
  double font_scale =
      prefs->GetDouble(prefs::kAccessibilityReadAnythingFontScale);
  base::UmaHistogramExactLinear(string_constants::kFontScaleHistogramName,
                                GetNormalizedFontScale(font_scale),
                                maximum_font_scale_logging + 1);
  std::string font_name =
      prefs->GetString(prefs::kAccessibilityReadAnythingFontName);
  if (font_map_.find(font_name) != font_map_.end()) {
    ReadAnythingFont font = font_map_.at(font_name);
    base::UmaHistogramEnumeration(string_constants::kFontNameHistogramName,
                                  font);
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

void ReadAnythingUntrustedPageHandler::ObserveWebContentsSidePanelController(
    content::WebContents* web_contents) {
  tab_helper_ = ReadAnythingTabHelper::FromWebContents(web_contents);
  if (tab_helper_) {
    tab_helper_->AddPageHandlerAsObserver(weak_factory_.GetWeakPtr());
  }
}

// ash::SessionObserver
#if BUILDFLAG(IS_CHROMEOS_ASH)
void ReadAnythingUntrustedPageHandler::OnLockStateChanged(bool locked) {
  if (locked) {
    page_->OnDeviceLocked();
  }
}
#endif
