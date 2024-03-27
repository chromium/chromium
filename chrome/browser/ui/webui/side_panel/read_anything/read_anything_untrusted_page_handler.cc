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
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "chrome/common/pdf_util.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
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

using read_anything::mojom::ReadAnythingTheme;
using read_anything::mojom::UntrustedPage;
using read_anything::mojom::UntrustedPageHandler;

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
        speechRate,
        features::IsReadAnythingReadAloudEnabled()
            ? prefs->GetDict(prefs::kAccessibilityReadAnythingVoiceName).Clone()
            : base::Value::Dict(),
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

  OnActiveWebContentsChanged();
}

ReadAnythingUntrustedPageHandler::~ReadAnythingUntrustedPageHandler() {
  TabStripModelObserver::StopObservingAll(this);
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
}

void ReadAnythingUntrustedPageHandler::PrimaryPageChanged() {
  SetUpPdfObserver();
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
    // TODO(crbug.com/1513227): Improve PDF OCR support for Reading Mode. Maybe
    // it would make it easy to read and maintain the code if setting the AXMode
    // for PDF OCR (i.e. `ui::AXMode::kPDFOcr`) is handled by
    // `PdfOcrController`. Enable accessibility to receive events (data) from
    // PDF. Set kPDFOcr only when the PDF OCR feature flag is enabled to support
    // inaccessible PDFs. Reset accessibility to get the new updated trees.
    ui::AXMode ax_mode = kReadAnythingAXMode;
    if (features::IsPdfOcrEnabled()) {
      ax_mode |= ui::AXMode::kPDFOcr;
    }
    pdf_observer_ = std::make_unique<ReadAnythingWebContentsObserver>(
        weak_factory_.GetSafeRef(), inner_contents[0], ax_mode);
  }
}

void ReadAnythingUntrustedPageHandler::OnActiveAXTreeIDChanged() {
  ui::AXTreeID tree_id = ui::AXTreeIDUnknown();
  ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
  GURL visible_url;
  bool is_pdf = !!pdf_observer_;
  if (main_observer_ && active_) {
    content::WebContents* contents =
        is_pdf ? pdf_observer_->web_contents() : main_observer_->web_contents();
    if (contents) {
      visible_url = contents->GetVisibleURL();
      content::RenderFrameHost* render_frame_host = nullptr;
      if (is_pdf) {
        contents->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
          if (rfh->GetProcess()->IsPdf()) {
            render_frame_host = rfh;
          }
        });
      } else {
        render_frame_host = contents->GetPrimaryMainFrame();
      }
      if (render_frame_host) {
        tree_id = render_frame_host->GetAXTreeID();
        ukm_source_id = render_frame_host->GetPageUkmSourceId();
      }
    }
  }
  page_->OnActiveAXTreeIDChanged(tree_id, ukm_source_id, visible_url, is_pdf);
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
