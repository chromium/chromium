// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_UNTRUSTED_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_UNTRUSTED_PAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_tab_helper.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_snapshotter.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "components/translate/core/browser/translate_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_updates_and_events.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/session/session_observer.h"
#endif

namespace content {
class ScopedAccessibilityMode;
}

class ReadAnythingUntrustedPageHandler;

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
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;

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
//  (chrome/browser/resources/side_panel/read_anything/app.ts).
//  This class is created and owned by ReadAnythingUntrustedUI and has the same
//  lifetime as the Side Panel view.
//
class ReadAnythingUntrustedPageHandler :
#if BUILDFLAG(IS_CHROMEOS_ASH)
    public ash::SessionObserver,
#endif
    public ui::AXActionHandlerObserver,
    public read_anything::mojom::UntrustedPageHandler,
    public ReadAnythingCoordinator::Observer,
    public ReadAnythingSidePanelController::Observer,
    public translate::TranslateDriver::LanguageDetectionObserver,
    public TabStripModelObserver {
 public:
  ReadAnythingUntrustedPageHandler(
      mojo::PendingRemote<read_anything::mojom::UntrustedPage> page,
      mojo::PendingReceiver<read_anything::mojom::UntrustedPageHandler>
          receiver,
      content::WebUI* web_ui);
  ReadAnythingUntrustedPageHandler(const ReadAnythingUntrustedPageHandler&) =
      delete;
  ReadAnythingUntrustedPageHandler& operator=(
      const ReadAnythingUntrustedPageHandler&) = delete;
  ~ReadAnythingUntrustedPageHandler() override;

  void AccessibilityEventReceived(const ui::AXUpdatesAndEvents& details);
  void PrimaryPageChanged();
  void WebContentsDestroyed();

  // read_anything::mojom::UntrustedPageHandler:
  void OnVoiceChange(const std::string& voice,
                     const std::string& lang) override;
  void OnLanguagePrefChange(const std::string& lang, bool enabled) override;

  // ash::SessionObserver
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void OnLockStateChanged(bool locked) override;
#endif

 private:
  // TranslateDriver::LanguageDetectionObserver:
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;
  void OnTranslateDriverDestroyed(translate::TranslateDriver* driver) override;

  // ui::AXActionHandlerObserver:
  void TreeRemoved(ui::AXTreeID ax_tree_id) override;

  // read_anything::mojom::UntrustedPageHandler:
  void GetVoicePackInfo(const std::string& language,
                        GetVoicePackInfoCallback mojo_remote_callback) override;
  void InstallVoicePack(const std::string& language,
                        InstallVoicePackCallback mojo_remote_callback) override;
  void OnCopy() override;
  void OnLineSpaceChange(
      read_anything::mojom::LineSpacing line_spacing) override;
  void OnLetterSpaceChange(
      read_anything::mojom::LetterSpacing letter_spacing) override;
  void OnFontChange(const std::string& font) override;
  void OnFontSizeChange(double font_size) override;
  void OnLinksEnabledChanged(bool enabled) override;
  void OnImagesEnabledChanged(bool enabled) override;
  void OnColorChange(read_anything::mojom::Colors color) override;
  void OnSpeechRateChange(double rate) override;
  void OnHighlightGranularityChanged(
      read_anything::mojom::HighlightGranularity granularity) override;
  void OnLinkClicked(const ui::AXTreeID& target_tree_id,
                     ui::AXNodeID target_node_id) override;
  void OnImageDataRequested(const ui::AXTreeID& target_tree_id,
                            ui::AXNodeID target_node_id) override;
  void OnSelectionChange(const ui::AXTreeID& target_tree_id,
                         ui::AXNodeID anchor_node_id,
                         int anchor_offset,
                         ui::AXNodeID focus_node_id,
                         int focus_offset) override;
  void OnCollapseSelection() override;
  void OnSnapshotRequested() override;

  // ReadAnythingCoordinator::Observer:
  void Activate(bool active) override;
  void OnCoordinatorDestroyed() override;

  void SetDefaultLanguageCode(const std::string& code);

  // Sends the language code of the new page, or the default if a language can't
  // be determined.
  void SetLanguageCode(const std::string& code);

  // ReadAnythingSidePanelController::Observer:
  void OnSidePanelControllerDestroyed() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabStripModelDestroyed(TabStripModel* tab_strip_model) override;

  // When the active web contents changes (or the UI becomes active):
  // 1. Begins observing the web contents of the active tab and enables web
  //    contents-only accessibility on that web contents. This causes
  //    AXTreeSerializer to reset and send accessibility events of the AXTree
  //    when it is re-serialized. The WebUI receives these events and stores a
  //    copy of the web contents' AXTree.
  // 2. Notifies the model that the AXTreeID has changed.
  void OnActiveWebContentsChanged();

  void SetUpPdfObserver();

  void OnActiveAXTreeIDChanged();

  // Logs the current visual settings values.
  void LogTextStyle();

  // Adds this as an observer of the ReadAnythingSidePanelController tied to a
  // WebContents.
  void ObserveWebContentsSidePanelController(
      content::WebContents* web_contents);

  void PerformActionInTargetTree(const ui::AXTreeID& target_tree_id,
                                 const ui::AXActionData& data);

  raw_ptr<ReadAnythingCoordinator> coordinator_;
  raw_ptr<ReadAnythingTabHelper> tab_helper_;
  const base::WeakPtr<Browser> browser_;
  const raw_ptr<content::WebUI> web_ui_;

  std::unique_ptr<ReadAnythingWebContentsObserver> main_observer_;

  // This observer is used when the current page is a pdf. It observes a child
  // (iframe) of the main web contents since that is where the pdf contents is
  // contained.
  std::unique_ptr<ReadAnythingWebContentsObserver> pdf_observer_;

  // `web_snapshotter_` is used to capture a screenshot of the main web
  // contents requested.
  std::unique_ptr<ReadAnythingSnapshotter> web_snapshotter_;

  const mojo::Receiver<read_anything::mojom::UntrustedPageHandler> receiver_;
  const mojo::Remote<read_anything::mojom::UntrustedPage> page_;

  // Whether the Read Anything feature is currently active. The feature is
  // active when it is currently shown in the Side Panel.
  bool active_ = true;

  // The current language being used in the app.
  std::string current_language_code_ = "en-US";

  // Observes the AXActionHandlerRegistry for AXTree removals.
  base::ScopedObservation<ui::AXActionHandlerRegistry,
                          ui::AXActionHandlerObserver>
      ax_action_handler_observer_{this};

  void OnScreenAIServiceInitialized(bool successful);

  // Observes LanguageDetectionObserver, which notifies us when the language of
  // the contents of the current page has been determined.
  base::ScopedObservation<translate::TranslateDriver,
                          translate::TranslateDriver::LanguageDetectionObserver>
      translate_observation_{this};

  base::WeakPtrFactory<ReadAnythingUntrustedPageHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_UNTRUSTED_PAGE_HANDLER_H_
