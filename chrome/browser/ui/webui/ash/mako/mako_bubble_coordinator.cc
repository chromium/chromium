// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/mako_bubble_coordinator.h"

#include <algorithm>
#include <optional>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/mako/mako_consent_view.h"
#include "chrome/browser/ui/webui/ash/mako/mako_rewrite_view.h"
#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"
#include "chrome/browser/ui/webui/ash/mako/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/orca_resources.h"
#include "chrome/grit/orca_resources_map.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

std::string_view ToOrcaModeParamValue(MakoEditorMode mode) {
  return mode == MakoEditorMode::kWrite ? kOrcaWriteMode : kOrcaRewriteMode;
}

std::string GetSystemLocale() {
  return g_browser_process != nullptr
             ? g_browser_process->GetApplicationLocale()
             : "";
}

}  // namespace

MakoBubbleCoordinator::MakoBubbleCoordinator() = default;

MakoBubbleCoordinator::~MakoBubbleCoordinator() {
  CloseUI();
}

void MakoBubbleCoordinator::LoadConsentUI(Profile* profile) {
  GURL url = net::AppendOrReplaceQueryParameter(GURL(kChromeUIMakoPrivacyURL),
                                                kOrcaHostLanguageParamKey,
                                                GetSystemLocale());

  contents_wrapper_ = std::make_unique<WebUIContentsWrapperT<MakoUntrustedUI>>(
      GURL(kChromeUIMakoPrivacyURL), profile, IDS_ACCNAME_ORCA);
  views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<MakoConsentView>(contents_wrapper_.get(),
                                        context_caret_bounds_));
}

void MakoBubbleCoordinator::LoadEditorUI(
    Profile* profile,
    MakoEditorMode mode,
    bool can_fallback_to_center_position,
    bool feedback_enabled,
    std::optional<std::string_view> preset_query_id,
    std::optional<std::string_view> freeform_text) {
  if (IsShowingUI()) {
    contents_wrapper_->CloseUI();
  }

  GURL url = net::AppendOrReplaceQueryParameter(GURL(kChromeUIMakoOrcaURL),
                                                kOrcaModeParamKey,
                                                ToOrcaModeParamValue(mode));
  url = net::AppendOrReplaceQueryParameter(url, kOrcaPresetParamKey,
                                           preset_query_id);
  url = net::AppendOrReplaceQueryParameter(url, kOrcaFreeformParamKey,
                                           freeform_text);
  url = net::AppendOrReplaceQueryParameter(url, kOrcaHostLanguageParamKey,
                                           GetSystemLocale());
  url = net::AppendOrReplaceQueryParameter(url, kOrcaFeedbackEnabledParamKey,
                                           feedback_enabled ? "true" : "false");
  auto* magic_boost_state = chromeos::MagicBoostState::Get();
  url = net::AppendOrReplaceQueryParameter(
      url, kOrcaMagicBoostParamKey,
      magic_boost_state && magic_boost_state->IsMagicBoostAvailable()
          ? "true"
          : "false");

  if (base::FeatureList::IsEnabled(ash::features::kOrcaResizingSupport)) {
    url = net::AppendOrReplaceQueryParameter(url, kOrcaResizingEnabledParamKey,
                                             "true");
  }

  contents_wrapper_ = std::make_unique<WebUIContentsWrapperT<MakoUntrustedUI>>(
      url, profile, IDS_ACCNAME_ORCA,
      /*esc_closes_ui=*/false);
  views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<MakoRewriteView>(contents_wrapper_.get(),
                                        context_caret_bounds_,
                                        can_fallback_to_center_position));
}

void MakoBubbleCoordinator::ShowUI() {
  if (contents_wrapper_) {
    contents_wrapper_->ShowUI();
  }
}

void MakoBubbleCoordinator::CloseUI() {
  if (contents_wrapper_) {
    contents_wrapper_->CloseUI();
    contents_wrapper_ = nullptr;
  }
}

bool MakoBubbleCoordinator::IsShowingUI() const {
  // TODO(b/301518440): To accurately check if the bubble is open, detect when
  // the JS has finished loading instead of checking this pointer.
  return contents_wrapper_ != nullptr &&
         contents_wrapper_->GetHost() != nullptr;
}

void MakoBubbleCoordinator::CacheContextCaretBounds() {
  const ui::InputMethod* input_method =
      IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  if (input_method && input_method->GetTextInputClient()) {
    context_caret_bounds_ =
        input_method->GetTextInputClient()->GetCaretBounds();
  }
}

}  // namespace ash
