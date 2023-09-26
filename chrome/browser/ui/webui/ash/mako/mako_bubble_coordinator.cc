// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/mako_bubble_coordinator.h"

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"
#include "chrome/browser/ui/webui/ash/mako/url_constants.h"
#include "chrome/grit/orca_resources.h"
#include "chrome/grit/orca_resources_map.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

constexpr int kCursorVerticalPadding = 8;

constexpr int kMakoCornerRadius = 20;

// Height threshold of the mako rewrite UI which determines its screen position.
// Tall UI is centered on the display screen containing the caret, while short
// UI is anchored at the caret.
constexpr int kMakoRewriteHeightThreshold = 400;

// TODO(b/289969807): As a placeholder, use 3961 which is the emoji picker
// identifier for task manager. We should create a proper one for mako.
constexpr int kMakoTaskManagerStringID = 3961;

std::string_view ToOrcaModeParamValue(MakoEditorMode mode) {
  return mode == MakoEditorMode::kWrite ? kOrcaWriteMode : kOrcaRewriteMode;
}

const ui::TextInputClient* GetTextInputClient() {
  const ui::InputMethod* input_method =
      IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  return input_method ? input_method->GetTextInputClient() : nullptr;
}

class MakoRewriteView : public WebUIBubbleDialogView {
 public:
  METADATA_HEADER(MakoRewriteView);
  MakoRewriteView(BubbleContentsWrapper* contents_wrapper,
                  const gfx::Rect& caret_bounds)
      : WebUIBubbleDialogView(nullptr, contents_wrapper),
        caret_bounds_(caret_bounds) {
    set_has_parent(false);
    set_corner_radius(kMakoCornerRadius);
    set_adjust_if_offscreen(true);
  }
  MakoRewriteView(const MakoRewriteView&) = delete;
  MakoRewriteView& operator=(const MakoRewriteView&) = delete;
  ~MakoRewriteView() override = default;

  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override {
    if (new_size.height() > kMakoRewriteHeightThreshold) {
      // Place tall UI at the center of the screen.
      SetArrowWithoutResizing(views::BubbleBorder::FLOAT);
      SetAnchorRect(display::Screen::GetScreen()
                        ->GetDisplayMatching(caret_bounds_)
                        .work_area());
    } else {
      // Anchor short UI at the caret.
      SetArrowWithoutResizing(views::BubbleBorder::TOP_LEFT);
      gfx::Rect anchor_rect = caret_bounds_;
      anchor_rect.Outset(gfx::Outsets::VH(kCursorVerticalPadding, 0));
      SetAnchorRect(anchor_rect);
    }
    WebUIBubbleDialogView::ResizeDueToAutoResize(source, new_size);
  }

 private:
  gfx::Rect caret_bounds_;
};

BEGIN_METADATA(MakoRewriteView, WebUIBubbleDialogView)
END_METADATA

class MakoConsentView : public WebUIBubbleDialogView {
 public:
  METADATA_HEADER(MakoConsentView);
  MakoConsentView(BubbleContentsWrapper* contents_wrapper,
                  const gfx::Rect& caret_bounds)
      : WebUIBubbleDialogView(nullptr, contents_wrapper) {
    set_has_parent(false);
    set_corner_radius(kMakoCornerRadius);
    SetModalType(ui::MODAL_TYPE_SYSTEM);
    SetArrowWithoutResizing(views::BubbleBorder::FLOAT);
    SetAnchorRect(display::Screen::GetScreen()
                      ->GetDisplayMatching(caret_bounds)
                      .work_area());
  }
  MakoConsentView(const MakoConsentView&) = delete;
  MakoConsentView& operator=(const MakoConsentView&) = delete;
  ~MakoConsentView() override = default;
};

BEGIN_METADATA(MakoConsentView, WebUIBubbleDialogView)
END_METADATA

}  // namespace

MakoBubbleCoordinator::MakoBubbleCoordinator() = default;

MakoBubbleCoordinator::~MakoBubbleCoordinator() {
  CloseUI();
}

void MakoBubbleCoordinator::ShowConsentUI(Profile* profile) {
  if (!GetTextInputClient()) {
    return;
  }

  caret_bounds_ = GetTextInputClient()->GetCaretBounds();
  contents_wrapper_ = std::make_unique<BubbleContentsWrapperT<MakoUntrustedUI>>(
      GURL(kChromeUIMakoPrivacyURL), profile, kMakoTaskManagerStringID);
  contents_wrapper_->ReloadWebContents();
  views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<MakoConsentView>(contents_wrapper_.get(),
                                        caret_bounds_.value()))
      ->Show();
}

void MakoBubbleCoordinator::ShowEditorUI(
    Profile* profile,
    MakoEditorMode mode,
    absl::optional<std::string_view> preset_query_id,
    absl::optional<std::string_view> freeform_text) {
  if (IsShowingUI()) {
    // If switching contents (e.g. from consent UI to rewrite UI), close the
    // current contents and use the cached caret bounds.
    contents_wrapper_->CloseUI();
    CHECK(caret_bounds_.has_value());
  } else if (const auto* text_input_client = GetTextInputClient()) {
    // Otherwise, try to get the caret bounds from the text input client.
    caret_bounds_ = text_input_client->GetCaretBounds();
  } else {
    // Otherwise, don't show mako UI.
    return;
  }

  GURL url = net::AppendOrReplaceQueryParameter(GURL(kChromeUIMakoOrcaURL),
                                                kOrcaModeParamKey,
                                                ToOrcaModeParamValue(mode));
  url = net::AppendOrReplaceQueryParameter(url, kOrcaPresetParamKey,
                                           preset_query_id);
  url = net::AppendOrReplaceQueryParameter(url, kOrcaFreeformParamKey,
                                           freeform_text);

  contents_wrapper_ = std::make_unique<BubbleContentsWrapperT<MakoUntrustedUI>>(
      url, profile, kMakoTaskManagerStringID);
  contents_wrapper_->ReloadWebContents();
  views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<MakoRewriteView>(contents_wrapper_.get(),
                                        caret_bounds_.value()))
      ->Show();
}

void MakoBubbleCoordinator::CloseUI() {
  if (contents_wrapper_) {
    contents_wrapper_->CloseUI();
    contents_wrapper_ = nullptr;
    caret_bounds_ = absl::nullopt;
  }
}

bool MakoBubbleCoordinator::IsShowingUI() const {
  // TODO(b/301518440): To accurately check if the bubble is open, detect when
  // the JS has finished loading instead of checking this pointer.
  return contents_wrapper_ != nullptr &&
         contents_wrapper_->GetHost() != nullptr;
}

}  // namespace ash
