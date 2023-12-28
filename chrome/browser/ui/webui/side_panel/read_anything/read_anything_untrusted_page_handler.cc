// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_page_handler.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "chrome/common/pdf_util.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_ui.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_tree_update.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#endif

using read_anything::mojom::ReadAnythingTheme;
using read_anything::mojom::UntrustedPage;
using read_anything::mojom::UntrustedPageHandler;

namespace {

int GetNormalizedFontScale(double font_scale) {
  DCHECK(font_scale >= kReadAnythingMinimumFontScale &&
         font_scale <= kReadAnythingMaximumFontScale);
  return (font_scale - kReadAnythingMinimumFontScale) *
         (1 / kReadAnythingFontScaleIncrement);
}

}  // namespace

ReadAnythingWebContentsObserver::ReadAnythingWebContentsObserver(
    base::SafeRef<ReadAnythingUntrustedPageHandler> page_handler,
    content::WebContents* web_contents)
    : page_handler_(page_handler) {
  Observe(web_contents);
}

ReadAnythingWebContentsObserver::~ReadAnythingWebContentsObserver() = default;

void ReadAnythingWebContentsObserver::AccessibilityEventReceived(
    const content::AXEventNotificationDetails& details) {
  page_handler_->AccessibilityEventReceived(details);
}

void ReadAnythingWebContentsObserver::PrimaryPageChanged(content::Page& page) {
  page_handler_->PrimaryPageChanged();
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

  coordinator_ = ReadAnythingCoordinator::FromBrowser(browser_.get());
  if (coordinator_) {
    coordinator_->AddObserver(this);
    coordinator_->AddModelObserver(this);
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
    page_->OnSettingsRestoredFromPrefs(
        static_cast<read_anything::mojom::LineSpacing>(
            prefs->GetInteger(prefs::kAccessibilityReadAnythingLineSpacing)),
        static_cast<read_anything::mojom::LetterSpacing>(
            prefs->GetInteger(prefs::kAccessibilityReadAnythingLetterSpacing)),
        prefs->GetString(prefs::kAccessibilityReadAnythingFontName),
        prefs->GetDouble(prefs::kAccessibilityReadAnythingFontScale),
        static_cast<read_anything::mojom::Colors>(
            prefs->GetInteger(prefs::kAccessibilityReadAnythingColorInfo)),
        speechRate,
        features::IsReadAnythingReadAloudEnabled()
            ? prefs->GetDict(prefs::kAccessibilityReadAnythingVoiceName).Clone()
            : base::Value::Dict(),
        highlightGranularity);
  }

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (features::IsReadAnythingWithScreen2xEnabled()) {
    if (screen_ai::ScreenAIInstallState::GetInstance()->get_state() ==
        screen_ai::ScreenAIInstallState::State::kReady) {
      // Notify that the screen ai service is already ready so we can bind to
      // the content extractor.
      page_->ScreenAIServiceReady();
    } else if (!component_ready_observer_.IsObserving()) {
      component_ready_observer_.Observe(
          screen_ai::ScreenAIInstallState::GetInstance());
    }
  }
#endif
  OnActiveWebContentsChanged();
}

ReadAnythingUntrustedPageHandler::~ReadAnythingUntrustedPageHandler() {
  TabStripModelObserver::StopObservingAll(this);
  main_observer_.reset();
  pdf_observer_.reset();
  LogTextStyle();

  if (!coordinator_) {
    return;
  }

  // If |this| is destroyed before the |ReadAnythingCoordinator|, then remove
  // |this| from the observer lists. In the cases where the coordinator is
  // destroyed first, these will have been destroyed before this call.
  coordinator_->RemoveObserver(this);
  coordinator_->RemoveModelObserver(this);
}

void ReadAnythingUntrustedPageHandler::PrimaryPageChanged() {
  OnActiveAXTreeIDChanged();
}

void ReadAnythingUntrustedPageHandler::AccessibilityEventReceived(
    const content::AXEventNotificationDetails& details) {
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
    ScopedDictPrefUpdate update(prefs,
                                prefs::kAccessibilityReadAnythingVoiceName);
    update->Set(lang, voice);
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
  ui::AXActionHandlerBase* handler =
      ui::AXActionHandlerRegistry::GetInstance()->GetActionHandler(
          target_tree_id);
  if (!handler) {
    return;
  }
  handler->PerformAction(action_data);
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

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingModel::Observer:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingUntrustedPageHandler::OnReadAnythingThemeChanged(
    const std::string& font_name,
    double font_scale,
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

  page_->OnThemeChanged(
      ReadAnythingTheme::New(font_name, font_scale, foreground_skcolor,
                             background_skcolor, line_spacing, letter_spacing));
}

void ReadAnythingUntrustedPageHandler::SetDefaultLanguageCode(
    const std::string& code) {
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

///////////////////////////////////////////////////////////////////////////////
// screen_ai::ScreenAIInstallState::Observer:
///////////////////////////////////////////////////////////////////////////////

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void ReadAnythingUntrustedPageHandler::StateChanged(
    screen_ai::ScreenAIInstallState::State state) {
  DCHECK(features::IsReadAnythingWithScreen2xEnabled());
  // If Screen AI library is downloaded but not initialized yet, ensure it is
  // loadable and initializes without any problems.
  if (state == screen_ai::ScreenAIInstallState::State::kDownloaded &&
      browser_) {
    screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(
        browser_->profile())
        ->InitializeMainContentExtractionIfNeeded();
    return;
  }
  if (state == screen_ai::ScreenAIInstallState::State::kReady) {
    page_->ScreenAIServiceReady();
  }
}
#endif

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
  DCHECK(browser_);
  tab_strip_model->RemoveObserver(this);
}

///////////////////////////////////////////////////////////////////////////////

void ReadAnythingUntrustedPageHandler::OnActiveWebContentsChanged() {
  // TODO(crbug.com/1266555): Disable accessibility.and stop observing events
  // on the now inactive tab. But make sure that we don't disable it for
  // assistive technology users. Some options here are:
  // 1. Cache the current AXMode of the active web contents before enabling
  //    accessibility, and reset the mode to that mode when the tab becomes
  //    inactive.
  // 2. Set an AXContext on the web contents with web contents only mode
  //    enabled.
  content::WebContents* web_contents = nullptr;
  if (active_ && browser_) {
    web_contents = browser_->tab_strip_model()->GetActiveWebContents();
  }

  main_observer_ = std::make_unique<ReadAnythingWebContentsObserver>(
      weak_factory_.GetSafeRef(), web_contents);
  pdf_observer_.reset();

  // Enable accessibility for the top level render frame and all descendants.
  // This causes AXTreeSerializer to reset and send accessibility events of
  // the AXTree when it is re-serialized.
  // TODO(crbug.com/1266555): Only enable kReadAnythingAXMode while still
  // causing the reset.
  if (web_contents) {
    web_contents->EnableAccessibilityMode(ui::kAXModeWebContentsOnly);
  }
  OnActiveAXTreeIDChanged();
}

void ReadAnythingUntrustedPageHandler::OnActiveAXTreeIDChanged(
    bool force_update_state) {
  ui::AXTreeID tree_id = ui::AXTreeIDUnknown();
  ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
  GURL visible_url;
  if (active_ && main_observer_ && main_observer_->web_contents()) {
    visible_url = main_observer_->web_contents()->GetVisibleURL();
    content::RenderFrameHost* render_frame_host =
        main_observer_->web_contents()->GetPrimaryMainFrame();
    if (render_frame_host) {
      tree_id = render_frame_host->GetAXTreeID();
      ukm_source_id = render_frame_host->GetPageUkmSourceId();
    }
  }

  page_->OnActiveAXTreeIDChanged(tree_id, ukm_source_id, visible_url,
                                 force_update_state);
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

void ReadAnythingUntrustedPageHandler::EnablePDFContentAccessibility(
    const ui::AXTreeID& ax_tree_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromAXTreeID(ax_tree_id);
  if (!render_frame_host) {
    return;
  }

  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (contents == main_observer_->web_contents()) {
    return;
  }

  CHECK(IsPdfExtensionOrigin(
      contents->GetPrimaryMainFrame()->GetLastCommittedOrigin()));
  pdf_observer_ = std::make_unique<ReadAnythingWebContentsObserver>(
      weak_factory_.GetSafeRef(), contents);

  // TODO(crbug.com/1513227): Improve PDF OCR support for Reading Mode. Maybe
  // it would make it easy to read and maintain the code if setting the AXMode
  // for PDF OCR (i.e. `ui::AXMode::kPDFOcr`) is handled by `PdfOcrController`.
  // Enable accessibility to receive events (data) from PDF. Set kPDFOcr only
  // when the PDF OCR feature flag is enabled to support inaccessible PDFs.
  // Reset accessibility to get the new updated trees.
  ui::AXMode ax_mode = ui::kAXModeWebContentsOnly;
  if (features::IsPdfOcrEnabled()) {
    ax_mode |= ui::AXMode::kPDFOcr;
  }
  contents->EnableAccessibilityMode(ax_mode);

  // Trigger distillation.
  OnActiveAXTreeIDChanged(true);
}
