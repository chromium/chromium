// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_entry_point_controller.h"

#include <string_view>
#include <type_traits>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/dom_distiller/tab_utils.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/optimization_guide/core/filters/optimization_hints_component_update_listener.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_filter.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "pdf/buildflags.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(ENABLE_PDF)
#include "base/strings/string_util.h"
#include "components/pdf/browser/pdf_document_helper.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace {

constexpr int kMaxChipIgnoredCount = 5;
constexpr const char* kDenyList[] = {
    "mail.google.com",
    "whatsapp.com",
    "chatgpt.com",
    "docs.google.com",
    "docs.sandbox.google.com",
    "calendar.google.com",
    "drive.google.com",
    "meet.google.com",
    "instagram.com",
    "tiktok.com",
    "youtube.com",
    "photos.google.com",
};
constexpr std::string_view kOmniboxDecisionHistogram =
    "Accessibility.ReadAnything.OmniboxChipDecision";

int GetOmniboxChipIgnoredCount(PrefService* prefs) {
  return prefs->GetInteger(
      prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount);
}

bool ShouldShowOmniboxChip(BrowserWindowInterface* bwi) {
  return GetOmniboxChipIgnoredCount(bwi->GetProfile()->GetPrefs()) <=
         kMaxChipIgnoredCount;
}

bool IsTriggeredByOmnibox(const actions::ActionInvocationContext& context) {
  std::underlying_type_t<page_actions::PageActionTrigger> page_action_trigger =
      context.GetProperty(page_actions::kPageActionTriggerKey);
  return (page_action_trigger != page_actions::kInvalidPageActionTrigger) &&
         features::IsReadAnythingOmniboxChipEnabled() &&
         base::FeatureList::IsEnabled(features::kPageActionsMigration);
}

void LogDecision(ReadAnythingOmniboxChipDecision decision) {
  base::UmaHistogramEnumeration(kOmniboxDecisionHistogram, decision);
}

#if BUILDFLAG(ENABLE_PDF)
size_t g_min_pdf_text_length_for_omnibox = 1100;
constexpr float kMaxNonAlphaFraction = 0.33;

bool IsMostlyAlphaChars(const std::u16string& text) {
  size_t non_alpha_chars =
      std::ranges::count_if(text, [](char16_t c) {
        // Check specifically for certain non-alphabetic characters rather than
        // for alphabetic characters. IsAsciiAlpha is only true for certain
        // scripts, so this avoids excluding other languages.
        return base::IsAsciiPunctuation(c) || base::IsAsciiDigit(c) ||
               base::IsWhitespace(c) || base::IsUnicodeControl(c);
      });

  return (static_cast<float>(non_alpha_chars) / text.size()) <
         kMaxNonAlphaFraction;
}

void OnPdfTextReceived(base::OnceCallback<void(bool)> result_callback,
                       const std::u16string& text) {
  // Show the omnibox on PDFs above a certain length, with a high percentage of
  // alphabetic characters. In this case, it is likely going to distill well in
  // Reading mode.
  bool long_enough = text.size() > g_min_pdf_text_length_for_omnibox;
  bool should_show = long_enough && IsMostlyAlphaChars(text);
  std::move(result_callback).Run(should_show);

  ReadAnythingOmniboxChipDecision decision;
  if (should_show) {
    decision = ReadAnythingOmniboxChipDecision::kShowPdf;
  } else if (!long_enough) {
    decision = ReadAnythingOmniboxChipDecision::kHideShortPdf;
  } else {
    decision = ReadAnythingOmniboxChipDecision::kHideLowAlphabeticPdf;
  }
  LogDecision(decision);
}

void RunPdfDistillableHeuristic(
    pdf::PDFDocumentHelper* pdf_helper,
    base::OnceCallback<void(bool)> result_callback) {
  CHECK(pdf_helper);

  // Use the text on the first page of the document to estimate if this could
  // be a distillable PDF.
  pdf_helper->GetPageText(
      /*page_index=*/0,
      base::BindOnce(&OnPdfTextReceived, std::move(result_callback)));
}
#endif

pdf::PDFDocumentHelper* GetPdf(content::WebContents* contents) {
#if BUILDFLAG(ENABLE_PDF)
  return pdf::PDFDocumentHelper::MaybeGetForWebContents(contents);
#else
  return nullptr;
#endif
}

void OnReadabilityDecision(base::OnceCallback<void(bool)> result_callback,
                           bool should_show) {
  std::move(result_callback).Run(should_show);
  ReadAnythingOmniboxChipDecision decision =
      should_show ? ReadAnythingOmniboxChipDecision::kShowArticle
                  : ReadAnythingOmniboxChipDecision::kHideReadability;
  LogDecision(decision);
}

void RunReadabilityHeuristic(content::WebContents* contents,
                             base::OnceCallback<void(bool)> result_callback) {
  CHECK(contents);
  RunReadabilityHeuristicsOnWebContents(
      contents,
      base::BindOnce(&OnReadabilityDecision, std::move(result_callback)));
}

void OnOptimizationGuideDecision(
    base::WeakPtr<content::WebContents> contents_weak,
    base::OnceCallback<void(bool)> result_callback,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  content::WebContents* contents = contents_weak.get();
  if (!contents) {
    std::move(result_callback).Run(false);
    return;
  }

  // This check is already done in CheckIfShouldSuggestReadingMode but it's
  // possible that the page was not detected as a PDF yet so check again.
  if (auto* pdf_helper = GetPdf(contents)) {
    RunPdfDistillableHeuristic(pdf_helper, std::move(result_callback));
    return;
  }

  switch (decision) {
    case optimization_guide::OptimizationGuideDecision::kFalse:
      // Optimization guide decided no, so immediately callback with a negative
      // result, bypassing the Readability check.
      std::move(result_callback).Run(false);
      LogDecision(ReadAnythingOmniboxChipDecision::kHideOptimizationGuide);
      break;
    case optimization_guide::OptimizationGuideDecision::kTrue:
    case optimization_guide::OptimizationGuideDecision::kUnknown:
      // If the optimization guide decided yes, or if it doesn't have enough
      // info yet, defer the decision to Readability.
      RunReadabilityHeuristic(contents, std::move(result_callback));
      break;
  }
}

void RunOptimizationGuide(
    OptimizationGuideKeyedService* optimization_guide_decider,
    BrowserWindowInterface* bwi,
    base::OnceCallback<void(bool)> result_callback) {
  CHECK(optimization_guide_decider);
  CHECK(bwi);

  optimization_guide_decider->CanApplyOptimization(
      bwi->GetActiveTabInterface()->GetContents()->GetLastCommittedURL(),
      optimization_guide::proto::READER_MODE_ELIGIBLE,
      base::BindOnce(&OnOptimizationGuideDecision,
                     bwi->GetActiveTabInterface()->GetContents()->GetWeakPtr(),
                     std::move(result_callback)));
}

static int check_count_;

}  // namespace

namespace read_anything {

base::AutoReset<size_t>
ReadAnythingEntryPointController::SetMinPdfTextLengthForTesting(size_t length) {
  return {&g_min_pdf_text_length_for_omnibox, length};
}

// static
void ReadAnythingEntryPointController::InvokePageAction(
    BrowserWindowInterface* bwi,
    const actions::ActionInvocationContext& context) {
  if (!bwi) {
    return;
  }
  std::underlying_type_t<SidePanelOpenTrigger> side_panel_trigger =
      context.GetProperty(kSidePanelOpenTriggerKey);

  ReadAnythingOpenTrigger open_trigger;
  if (side_panel_trigger ==
      static_cast<int>(SidePanelOpenTrigger::kPinnedEntryToolbarButton)) {
    open_trigger = ReadAnythingOpenTrigger::kPinnedSidePanelEntryToolbarButton;
  } else if (IsTriggeredByOmnibox(context)) {
    open_trigger = ReadAnythingOpenTrigger::kOmniboxChip;
    // Reset the ignored count for the omnibox entrypoint because it was used.
    bwi->GetProfile()->GetPrefs()->SetInteger(
        prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount, 0);
    auto* const user_ed = BrowserUserEducationInterface::From(bwi);
    user_ed->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHReadingModePageActionLabelFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  } else {
    return;
  }

  ToggleUI(bwi, open_trigger);
}

// static
void ReadAnythingEntryPointController::ShowUI(
    BrowserWindowInterface* bwi,
    ReadAnythingOpenTrigger open_trigger) {
  if (!bwi) {
    return;
  }
  if (!IsUIShowing(bwi)) {
    base::UmaHistogramEnumeration("Accessibility.ReadAnything.ShowTriggered",
                                  open_trigger);
  }

  if (features::IsImmersiveReadAnythingEnabled()) {
    // TODO(crbug.com/471001915): Once IRM flag is enabled by default, change
    // IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE, one of the triggers of this
    // method, to reflect that it's opening Immersive mode instead of Side
    // Panel.
    if (tabs::TabInterface* tab = bwi->GetActiveTabInterface()) {
      auto* controller = ReadAnythingController::From(tab);
      CHECK(controller);
      controller->ShowInPreferredUI(open_trigger);
    }
  } else {
    SidePanelOpenTrigger side_panel_open_trigger =
        ReadAnythingToSidePanelOpenTrigger(open_trigger);

    bwi->GetFeatures().side_panel_ui()->Show(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything),
        side_panel_open_trigger);
  }
}

// static
void ReadAnythingEntryPointController::ToggleUI(
    BrowserWindowInterface* bwi,
    ReadAnythingOpenTrigger open_trigger) {
  if (!bwi) {
    return;
  }

  if (!IsUIShowing(bwi)) {
    base::UmaHistogramEnumeration("Accessibility.ReadAnything.ShowTriggered",
                                  open_trigger);
  }

  if (features::IsImmersiveReadAnythingEnabled()) {
    if (tabs::TabInterface* tab = bwi->GetActiveTabInterface()) {
      auto* controller = ReadAnythingController::From(tab);
      CHECK(controller);
      controller->ToggleUI(open_trigger);
    }
  } else {
    SidePanelOpenTrigger side_panel_open_trigger =
        ReadAnythingToSidePanelOpenTrigger(open_trigger);

    bwi->GetFeatures().side_panel_ui()->Toggle(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything),
        side_panel_open_trigger);
  }
}

// static
bool ReadAnythingEntryPointController::IsUIShowing(
    BrowserWindowInterface* bwi) {
  if (!features::IsImmersiveReadAnythingEnabled() || !bwi) {
    return IsReadAnythingEntryShowing(bwi);
  }

  auto* controller = ReadAnythingController::From(bwi->GetActiveTabInterface());
  CHECK(controller);
  auto state = controller->GetPresentationState();
  return state ==
             ReadAnythingController::PresentationState::kInImmersiveOverlay ||
         state == ReadAnythingController::PresentationState::kInSidePanel;
}

// static
void ReadAnythingEntryPointController::UpdatePageActionVisibility(
    bool should_show_page_action,
    BrowserWindowInterface* bwi,
    base::OnceCallback<void(user_education::FeaturePromoResult promo_result)>
        show_promo_callback) {
  if (!base::FeatureList::IsEnabled(features::kPageActionsMigration) ||
      !features::IsReadAnythingOmniboxChipEnabled() || !bwi) {
    return;
  }

  page_actions::PageActionController* page_action_controller =
      bwi->GetActiveTabInterface()->GetTabFeatures()->page_action_controller();
  auto* const user_ed = BrowserUserEducationInterface::From(bwi);
  // No need to show the button if reading mode is already open.
  if (should_show_page_action && !IsUIShowing(bwi)) {
    page_action_controller->Show(kActionSidePanelShowReadAnything);
    if (ShouldShowOmniboxChip(bwi)) {
      page_action_controller->ShowSuggestionChip(
          kActionSidePanelShowReadAnything);
    }
    user_education::FeaturePromoParams params(
        feature_engagement::kIPHReadingModePageActionLabelFeature);
    if (show_promo_callback) {
      params.show_promo_result_callback = std::move(show_promo_callback);
    }
    user_ed->MaybeShowFeaturePromo(std::move(params));
  } else {
    user_ed->AbortFeaturePromo(
        feature_engagement::kIPHReadingModePageActionLabelFeature);
    page_action_controller->Hide(kActionSidePanelShowReadAnything);
  }
}

// static
bool ReadAnythingEntryPointController::CheckIfShouldSuggestReadingModeNaive(
    BrowserWindowInterface* bwi) {
  CHECK(features::IsReadAnythingOmniboxChipEnabled());
  CHECK(bwi);

  // Don't show the omnibox entrypoint if automation is enabled, such as
  // during automated testing.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAutomation)) {
    return false;
  }

  // Disable the omnibox on app windows, as these windows don't usually have
  // omnibox support.
  Browser* browser = bwi->GetBrowserForMigrationOnly();
  if (browser && (browser->is_type_app() || browser->is_type_app_popup())) {
    LogDecision(ReadAnythingOmniboxChipDecision::kHideAppWindow);
    return false;
  }

  // Don't show the omnibox entrypoint for non-HTTP(S) URLs. These URLs are
  // not supported by Readability, which is used to check whether the current
  // page is a good candidate for distillation.
  content::WebContents* contents = bwi->GetActiveTabInterface()->GetContents();
  const GURL& url = contents->GetLastCommittedURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    LogDecision(ReadAnythingOmniboxChipDecision::kHideNonHttp);
    return false;
  }

  // Don't show the omnibox entrypoint for sites we know don't distill well.
  for (const char* domain : kDenyList) {
    if (url.DomainIs(domain)) {
      LogDecision(ReadAnythingOmniboxChipDecision::kHideDenyList);
      return false;
    }
  }

  return true;
}

// static
void ReadAnythingEntryPointController::RegisterForSuggestReadingMode(
    Profile* profile) {
  if (auto* optimization_guide_decider =
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile)) {
    optimization_guide_decider->RegisterOptimizationTypes(
        {optimization_guide::proto::READER_MODE_ELIGIBLE});
  }
}

// static
void ReadAnythingEntryPointController::CheckIfShouldSuggestReadingMode(
    BrowserWindowInterface* bwi,
    base::OnceCallback<void(bool)> result_callback) {
  if (!features::IsReadAnythingOmniboxChipEnabled() || !bwi ||
      !CheckIfShouldSuggestReadingModeNaive(bwi)) {
    std::move(result_callback).Run(false);
    return;
  }

  check_count_++;

  // If this page is a PDF, then other heuristics will always return false.
  // But since PDFs are distilled via Screen2x, use a custom heuristic to
  // determine if the PDF will distill well with RM.
  content::WebContents* contents = bwi->GetActiveTabInterface()->GetContents();
  if (auto* pdf_helper = GetPdf(contents)) {
    RunPdfDistillableHeuristic(pdf_helper, std::move(result_callback));
    return;
  }

  auto* optimization_guide_decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(bwi->GetProfile());
  // If there is no optimization guide, cut straight to using Readability
  // instead, which would be used anyway after receiving the decision from the
  // optimization guide. This should only happen if the optimization guide flag
  // is explicitly disabled (it's enabled by default) or for a select few types
  // of internal profiles on ChromeOS.
  if (!optimization_guide_decider) {
    RunReadabilityHeuristic(contents, std::move(result_callback));
    return;
  }

  // The optimization guide uses the lattice article score and other info to
  // determine Reading mode eligibility.
  RunOptimizationGuide(optimization_guide_decider, bwi,
                       std::move(result_callback));
}

// static
void ReadAnythingEntryPointController::OnPageActionIgnored(
    BrowserWindowInterface* bwi) {
  if (!base::FeatureList::IsEnabled(features::kPageActionsMigration) ||
      !features::IsReadAnythingOmniboxChipEnabled() || !bwi) {
    return;
  }

  PrefService* prefs = bwi->GetProfile()->GetPrefs();
  prefs->SetInteger(prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount,
                    GetOmniboxChipIgnoredCount(prefs) + 1);
  if (!ShouldShowOmniboxChip(bwi)) {
    page_actions::PageActionController* page_action_controller =
        bwi->GetActiveTabInterface()
            ->GetTabFeatures()
            ->page_action_controller();
    page_action_controller->HideSuggestionChip(
        kActionSidePanelShowReadAnything);
  }
}

// static
int ReadAnythingEntryPointController::CheckCountForTesting() {
  return check_count_;
}

// static
void ReadAnythingEntryPointController::ResetCheckCountForTesting() {
  check_count_ = 0;
}

}  // namespace read_anything
