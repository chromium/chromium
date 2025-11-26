// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_UNTRUSTED_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_UNTRUSTED_PAGE_HANDLER_H_

#include <deque>
#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_screenshotter.h"
#include "chrome/common/read_anything/read_anything.mojom.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_driver.h"
#include "content/public/browser/tts_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_action_handler_registry.h"
#include "ui/accessibility/ax_updates_and_events.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/public/cpp/session/session_observer.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/chrome_os_extension_wrapper.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"
using ash::language_packs::PackResult;
#else
#include "extensions/browser/extension_registry_observer.h"
#endif

namespace content {
class ScopedAccessibilityMode;
}

class ReadAnythingUntrustedPageHandler;

// LINT.IfChange(EngineInstallationState)
// Installation state of the TTS engine extension
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EngineInstallationState {
  kEnabled = 0,
  kDisabled = 1,
  kTerminated = 2,
  kBlocked = 3,
  kReady = 4,
  kInstalling = 5,
  kUnknown = 6,
  kMaxValue = kUnknown,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:ReadAnythingExtensionInstallationState)

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingWebContentsObserver
//
//  This class allows the ReadAnythingUntrustedPageHandler to observe multiple
//  web contents at once.
//
class ReadAnythingWebContentsObserver : public content::WebContentsObserver {
 public:
  ReadAnythingWebContentsObserver(
      base::SafeRef<ReadAnythingUntrustedPageHandler> page_handler,
      content::WebContents* web_contents,
      ui::AXMode accessibility_mode);
  ReadAnythingWebContentsObserver(const ReadAnythingWebContentsObserver&) =
      delete;
  ReadAnythingWebContentsObserver& operator=(
      const ReadAnythingWebContentsObserver&) = delete;
  ~ReadAnythingWebContentsObserver() override;

  // content::WebContentsObserver:
  void AccessibilityEventReceived(
      const ui::AXUpdatesAndEvents& details) override;
  void AccessibilityLocationChangesReceived(
      const ui::AXTreeID& tree_id,
      ui::AXLocationAndScrollUpdates& details) override;
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;
  void DidStopLoading() override;
  void DidUpdateAudioMutingState(bool muted) override;

  // base::SafeRef used since the lifetime of ReadAnythingWebContentsObserver is
  // completely contained by page_handler_. See
  // ReadAnythingUntrustedPageHandler's destructor.
  base::SafeRef<ReadAnythingUntrustedPageHandler> page_handler_;

 private:
  // Enables the kReadAnythingAXMode accessibility mode flags for the
  // WebContents.
  std::unique_ptr<content::ScopedAccessibilityMode> scoped_accessibility_mode_;
};

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingUntrustedPageHandler
//
//  A handler of the Read Anything app
//  (chrome/browser/resources/side_panel/read_anything/app/app.ts).
//  This class is created and owned by ReadAnythingUntrustedUI and has the same
//  lifetime as the Side Panel view.
//
class ReadAnythingUntrustedPageHandler :
#if BUILDFLAG(IS_CHROMEOS)
    public ash::SessionObserver,
#else
    public content::UpdateLanguageStatusDelegate,
    public extensions::ExtensionRegistryObserver,
#endif
    public ui::AXActionHandlerObserver,
    public read_anything::mojom::UntrustedPageHandler,
    public ReadAnythingSidePanelController::Observer,
    public translate::TranslateDriver::LanguageDetectionObserver {
 public:
  ReadAnythingUntrustedPageHandler(
      mojo::PendingRemote<read_anything::mojom::UntrustedPage> page,
      mojo::PendingReceiver<read_anything::mojom::UntrustedPageHandler>
          receiver,
      content::WebUI* web_ui,
      bool use_screen_ai_service
#if BUILDFLAG(IS_CHROMEOS)
      ,
      std::unique_ptr<ChromeOsExtensionWrapper> extension_wrapper =
          std::make_unique<ChromeOsExtensionWrapper>()
#endif
  );
  ReadAnythingUntrustedPageHandler(const ReadAnythingUntrustedPageHandler&) =
      delete;
  ReadAnythingUntrustedPageHandler& operator=(
      const ReadAnythingUntrustedPageHandler&) = delete;
  ~ReadAnythingUntrustedPageHandler() override;

  static const int kMaxWordsDistilled = 25000;
  static const int kWordsDistilledBuckets = 100;

  void AccessibilityEventReceived(const ui::AXUpdatesAndEvents& details);
  void AccessibilityLocationChangesReceived(
      const ui::AXTreeID& tree_id,
      ui::AXLocationAndScrollUpdates& details);
  void PrimaryPageChanged();
  void DidStopLoading();
  void DidUpdateAudioMutingState(bool muted);
  void WebContentsDestroyed();
  void OnActiveAXTreeIDChanged();
  bool CheckForPdfContentAfterLoad();

  // read_anything::mojom::UntrustedPageHandler:
  void OnVoiceChange(const std::string& voice,
                     const std::string& lang) override;
  void OnLanguagePrefChange(const std::string& lang, bool enabled) override;
  void OnReadAloudAudioStateChange(bool playing) override;
  void OnSpeechRateChange(double rate) override;
  void OnImageDataRequested(const ui::AXTreeID& target_tree_id,
                            ui::AXNodeID target_node_id) override;
  void OnLineSpaceChange(
      read_anything::mojom::LineSpacing line_spacing) override;
  void OnLetterSpaceChange(
      read_anything::mojom::LetterSpacing letter_spacing) override;
  void OnFontChange(const std::string& font) override;
  void OnFontSizeChange(double font_size) override;
  void OnLinksEnabledChanged(bool enabled) override;
  void OnImagesEnabledChanged(bool enabled) override;
  void OnColorChange(read_anything::mojom::Colors color) override;
  void OnHighlightGranularityChanged(
      read_anything::mojom::HighlightGranularity granularity) override;
  void GetVoicePackInfo(const std::string& language) override;
  void InstallVoicePack(const std::string& language) override;
  void UninstallVoice(const std::string& language) override;
  void OnDistillationStatus(read_anything::mojom::DistillationStatus status,
                            int word_count) override;

  // TranslateDriver::LanguageDetectionObserver:
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;
  void OnTranslateDriverDestroyed(translate::TranslateDriver* driver) override;

  // ReadAnythingSidePanelController::Observer:
  void OnTabWillDetach() override;

  // Logs the extension installation state. Intended to get more information
  // on system voice usage.
  void LogExtensionState() override;

#if BUILDFLAG(IS_CHROMEOS)
  // ash::SessionObserver
  void OnLockStateChanged(bool locked) override;
#endif

 protected:
  void OnImageDataDownloaded(const ui::AXTreeID& target_tree_id,
                             ui::AXNodeID,
                             int id,
                             int http_status_code,
                             const GURL& image_url,
                             const std::vector<SkBitmap>& bitmaps,
                             const std::vector<gfx::Size>& sizes);

 private:
#if !BUILDFLAG(IS_CHROMEOS)
  // content::UpdateLanguageStatusDelegate:
  void OnUpdateLanguageStatus(content::BrowserContext* browser_context,
                              const std::string& lang,
                              content::LanguageInstallStatus install_status,
                              const std::string& error) override;
  // extensions::ExtensionRegistryObserver implementation.

  // OnExtensionReady is called even if the TTS engine was previously installed,
  // which read anything needs to know about to access the new voices.
  void OnExtensionReady(content::BrowserContext* browser_context,
                        const extensions::Extension* extension) override;
#else
  enum LanguageRequestType {
    kInstall,
    kInfo,
  };

  struct LanguageRequest {
    std::string language;
    LanguageRequestType type;
  };

  void SendOrQueueLanguageRequest(LanguageRequest request);
  void SendNextLanguageRequest();
  void OnInstallPackResponse(const PackResult& pack_result);
#endif

  // ui::AXActionHandlerObserver:
  void TreeRemoved(ui::AXTreeID ax_tree_id) override;

  // read_anything::mojom::UntrustedPageHandler:
  void GetDependencyParserModel(
      GetDependencyParserModelCallback callback) override;
  void OnCopy() override;

  void OnLinkClicked(const ui::AXTreeID& target_tree_id,
                     ui::AXNodeID target_node_id) override;
  void ScrollToTargetNode(const ui::AXTreeID& target_tree_id,
                          ui::AXNodeID target_node_id) override;
  void OnSelectionChange(const ui::AXTreeID& target_tree_id,
                         ui::AXNodeID anchor_node_id,
                         int anchor_offset,
                         ui::AXNodeID focus_node_id,
                         int focus_offset) override;
  void OnCollapseSelection() override;
  void OnScreenshotRequested() override;

  // ReadAnythingSidePanelController::Observer:
  void Activate(bool active,
                std::optional<ReadAnythingOpenTrigger> open_trigger) override;
  void OnSidePanelControllerDestroyed() override;

  void SetDefaultLanguageCode(const std::string& code);

  // Sends the language code of the new page, or the default if a language can't
  // be determined.
  void SetLanguageCode(const std::string& code);

  void SetUpPdfObserver();

  void OnGetVoicePackInfo(read_anything::mojom::VoicePackInfoPtr info);

  // Logs the current visual settings values.
  void LogTextStyle();

  // Adds this as an observer of the ReadAnythingSidePanelController tied to a
  // tab.
  void ObserveWebContentsSidePanelController(tabs::TabInterface* tab);

  void PerformActionInTargetTree(const ui::AXActionData& data);

  bool AreInnerContentsPdfContent(
      std::vector<content::WebContents*> inner_contents);

  void OnScreenAIServiceInitialized(bool successful);

  // Called to notify this instance that the dependency parser loader
  // is available for model requests or is invalidating existing requests
  // specified by "is_available". The "callback" will be either forwarded to a
  // request to get the actual model file or will be run with an empty file if
  // the dependency parser loader is rejecting requests because the pending
  // model request queue is already full (100 requests maximum).
  void OnDependencyParserModelFileAvailabilityChanged(
      GetDependencyParserModelCallback callback,
      bool is_available);

  raw_ptr<ReadAnythingSidePanelController> side_panel_controller_;
  const raw_ptr<Profile> profile_;
  const raw_ptr<content::WebUI> web_ui_;

  std::unique_ptr<ReadAnythingWebContentsObserver> main_observer_;

  // This observer is used when the current page is a pdf. It observes a child
  // (iframe) of the main web contents since that is where the pdf contents is
  // contained.
  std::unique_ptr<ReadAnythingWebContentsObserver> pdf_observer_;

  // `web_screenshotter_` is used to capture a screenshot of the main web
  // contents requested.
  std::unique_ptr<ReadAnythingScreenshotter> web_screenshotter_;

  const mojo::Receiver<read_anything::mojom::UntrustedPageHandler> receiver_;
  const mojo::Remote<read_anything::mojom::UntrustedPage> page_;

  std::optional<ReadAnythingOpenTrigger> last_open_trigger_;

  // Whether the Read Anything feature is currently active. The feature is
  // active when it is currently shown in the Side Panel.
  bool active_ = true;
  // Whether the tab is going to detach soon.
  bool tab_will_detach_ = false;

  // The current language being used in the app.
  std::string current_language_code_ = "en-US";
  const bool use_screen_ai_service_;

#if BUILDFLAG(IS_CHROMEOS)
  // The ChromeOS language pack manager can't handle more than one language
  // request at a time. When we receive requests from the page, queue them up
  // here and only request the next one when we receive the callback for the
  // previous one.
  std::deque<LanguageRequest> queued_language_requests_;
  bool has_pending_language_request_ = false;
  std::unique_ptr<ChromeOsExtensionWrapper> extension_wrapper_;
#endif

  // Observes the AXActionHandlerRegistry for AXTree removals.
  base::ScopedObservation<ui::AXActionHandlerRegistry,
                          ui::AXActionHandlerObserver>
      ax_action_handler_observer_{this};

  // Whether the currently distilled page is recognized as a pdf. This allows
  // the page handler to trigger distillation if the page would now be
  // recognized as a pdf after it finishes loading.
  bool is_pdf_ = false;

  base::ScopedClosureRunner audible_closure_;

  // Observes LanguageDetectionObserver, which notifies us when the language of
  // the contents of the current page has been determined.
  base::ScopedObservation<translate::TranslateDriver,
                          translate::TranslateDriver::LanguageDetectionObserver>
      translate_observation_{this};

  // Timer used for checking for pdf contents after the page has loaded.
  // Otherwise, it may incorrectly return that the page is not a pdf if
  // reading mode checks if a page is a pdf immediately after loading.
  base::OneShotTimer timer_;

  base::WeakPtrFactory<ReadAnythingUntrustedPageHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_UNTRUSTED_PAGE_HANDLER_H_
