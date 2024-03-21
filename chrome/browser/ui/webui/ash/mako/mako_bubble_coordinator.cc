// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/mako_bubble_coordinator.h"

#include <algorithm>
#include <optional>

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/mako/mako_consent_view.h"
#include "chrome/browser/ui/webui/ash/mako/mako_rewrite_view.h"
#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"
#include "chrome/browser/ui/webui/ash/mako/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/orca_resources.h"
#include "chrome/grit/orca_resources_map.h"
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

}  // namespace

MakoBubbleCoordinator::MakoBubbleCoordinator() = default;

MakoBubbleCoordinator::~MakoBubbleCoordinator() {
  CloseUI();
}

void MakoBubbleCoordinator::LoadConsentUI(Profile* profile) {
  contents_wrapper_ = std::make_unique<WebUIContentsWrapperT<MakoUntrustedUI>>(
      GURL(kChromeUIMakoPrivacyURL), profile, IDS_ACCNAME_ORCA);
  views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<MakoConsentView>(contents_wrapper_.get(),
                                        context_caret_bounds_));
}

void MakoBubbleCoordinator::LoadEditorUI(
    Profile* profile,
    MakoEditorMode mode,
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

  contents_wrapper_ = std::make_unique<WebUIContentsWrapperT<MakoUntrustedUI>>(
      url, profile, IDS_ACCNAME_ORCA, /*webui_resizes_host=*/true,
      /*esc_closes_ui=*/false);
  views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<MakoRewriteView>(contents_wrapper_.get(),
                                        context_caret_bounds_));
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
