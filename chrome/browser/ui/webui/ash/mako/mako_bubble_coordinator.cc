// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/mako_bubble_coordinator.h"

#include <algorithm>

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"
#include "chrome/browser/ui/webui/ash/mako/url_constants.h"
#include "chrome/grit/generated_resources.h"
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

constexpr int kMakoUIPadding = 16;

constexpr int kMakoCornerRadius = 20;

// Height threshold of the mako rewrite UI which determines its screen position.
// Tall UI is centered on the display screen containing the caret, while short
// UI is anchored at the caret.
constexpr int kMakoRewriteHeightThreshold = 400;

std::string_view ToOrcaModeParamValue(MakoEditorMode mode) {
  return mode == MakoEditorMode::kWrite ? kOrcaWriteMode : kOrcaRewriteMode;
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
    // Disable the default offscreen adjustment so that we can customise it.
    set_adjust_if_offscreen(false);
  }
  MakoRewriteView(const MakoRewriteView&) = delete;
  MakoRewriteView& operator=(const MakoRewriteView&) = delete;
  ~MakoRewriteView() override = default;

  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override {
    WebUIBubbleDialogView::ResizeDueToAutoResize(source, new_size);
    const gfx::Rect screen_work_area = display::Screen::GetScreen()
                                           ->GetDisplayMatching(caret_bounds_)
                                           .work_area();

    // If the UI is very tall, just place it at the center of the screen.
    if (new_size.height() > kMakoRewriteHeightThreshold) {
      SetArrowWithoutResizing(views::BubbleBorder::FLOAT);
      SetAnchorRect(screen_work_area);
      return;
    }

    // Otherwise, try to place it near the selection. First, try to left align
    // with the selection, but adjust to keep on screen if needed.
    const gfx::Size widget_size = GetWidget()->GetWindowBoundsInScreen().size();
    int x =
        std::min(caret_bounds_.x(), screen_work_area.right() -
                                        widget_size.width() - kMakoUIPadding);

    // Then, try to place the mako UI just under the top of the selection.
    int y = caret_bounds_.y() + kMakoUIPadding;
    // If that puts it offscreen, try placing it above the selection instead.
    if (y + widget_size.height() + kMakoUIPadding > screen_work_area.bottom()) {
      y = caret_bounds_.y() - kMakoUIPadding - widget_size.height();
    }

    // If it's still offscreen, place it at the bottom of the screen and adjust
    // the horizontal position to try to move it out of the way of the
    // selection.
    if (y < screen_work_area.y() + kMakoUIPadding) {
      y = screen_work_area.bottom() - kMakoUIPadding - widget_size.height();
      // Place it at the right of the selection edge if there is space
      // (including padding), otherwise, place it to the left of the selection.
      x = screen_work_area.right() - caret_bounds_.x() >
                  widget_size.width() + 2 * kMakoUIPadding
              ? caret_bounds_.x() + kMakoUIPadding
              : caret_bounds_.x() - kMakoUIPadding - widget_size.width();
    }

    // If necessary, adjust again to ensure the UI is onscreen.
    gfx::Rect widget_bounds({x, y}, widget_size);
    widget_bounds.AdjustToFit(screen_work_area);

    GetWidget()->SetBounds(widget_bounds);
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

void MakoBubbleCoordinator::LoadConsentUI(Profile* profile) {
  contents_wrapper_ = std::make_unique<BubbleContentsWrapperT<MakoUntrustedUI>>(
      GURL(kChromeUIMakoPrivacyURL), profile, IDS_ACCNAME_ORCA);
  contents_wrapper_->ReloadWebContents();
  views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<MakoConsentView>(contents_wrapper_.get(),
                                        context_caret_bounds_));
}

void MakoBubbleCoordinator::LoadEditorUI(
    Profile* profile,
    MakoEditorMode mode,
    absl::optional<std::string_view> preset_query_id,
    absl::optional<std::string_view> freeform_text) {
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

  contents_wrapper_ = std::make_unique<BubbleContentsWrapperT<MakoUntrustedUI>>(
      url, profile, IDS_ACCNAME_ORCA);
  contents_wrapper_->ReloadWebContents();
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
