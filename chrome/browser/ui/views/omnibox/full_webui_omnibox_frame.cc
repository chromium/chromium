// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/full_webui_omnibox_frame.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#else
#include "ui/views/view_targeter.h"
#include "ui/views/view_targeter_delegate.h"
#endif  // USE_AURA

namespace {

#if !defined(USE_AURA)
class ResultsViewTargeterDelegate : public views::ViewTargeterDelegate {
 public:
  explicit ResultsViewTargeterDelegate(FullWebUIOmniboxFrame* frame)
      : frame_(frame) {}

  ResultsViewTargeterDelegate(const ResultsViewTargeterDelegate&) = delete;
  ResultsViewTargeterDelegate& operator=(const ResultsViewTargeterDelegate&) =
      delete;
  ~ResultsViewTargeterDelegate() override = default;

  views::View* TargetForRect(views::View* root,
                             const gfx::Rect& rect) override {
    // On macOS (non-Aura), there is no `WindowTargeter` to exclude the shadow
    // margin at the window level. Because the widget bounds include the shadow
    // margin, and in WebUI shadow mode the WebView spans the entire widget,
    // clicks in the transparent shadow margin would be swallowed by the
    // WebView. Redirect rects falling entirely in that margin to `root` (this
    // frame), which forwards them to the browser window beneath so underlying
    // controls (e.g. bookmarks bar or web content) receive the events.
    gfx::Rect interior = root->GetLocalBounds();
    interior.Inset(RoundedOmniboxResultsFrame::GetShadowInsets());
    if (!interior.Intersects(rect)) {
      return root;
    }
    if (frame_->forward_mouse_events()) {
      if (rect.y() < frame_->GetEventForwardingInsets().top()) {
        return root;
      }
    }
    return views::ViewTargeterDelegate::TargetForRect(root, rect);
  }

 private:
  raw_ptr<FullWebUIOmniboxFrame> frame_;
};
#endif  // !USE_AURA

}  // namespace

FullWebUIOmniboxFrame::FullWebUIOmniboxFrame(views::View* contents,
                                             LocationBar* location_bar,
                                             bool forward_mouse_events)
    : RoundedOmniboxResultsFrame(contents, location_bar, forward_mouse_events) {
}

FullWebUIOmniboxFrame::~FullWebUIOmniboxFrame() = default;

void FullWebUIOmniboxFrame::SetElevation(int elevation) {
  if (elevation == 0) {
    SetBorder(views::CreateEmptyBorder(GetShadowInsets()));
#if defined(USE_AURA)
    UpdateWindowTargeter();
#endif  // USE_AURA
    return;
  }

  const int corner_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::ShapeContextTokens::kOmniboxExpandedRadius);
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::Arrow::NONE,
      views::BubbleBorder::Shadow::STANDARD_SHADOW);
  border->set_rounded_corners(gfx::RoundedCornersF(corner_radius));
  border->set_md_shadow_elevation(elevation);
  SetBorder(std::move(border));
#if defined(USE_AURA)
  UpdateWindowTargeter();
#endif  // USE_AURA
}

void FullWebUIOmniboxFrame::SetForwardMouseEvents(bool forward) {
  set_forward_mouse_events(forward);
#if defined(USE_AURA)
  UpdateWindowTargeter();
#endif  // USE_AURA
}

void FullWebUIOmniboxFrame::AddedToWidget() {
#if defined(USE_AURA)
  UpdateWindowTargeter();
#else
  SetEventTargeter(std::make_unique<views::ViewTargeter>(
      std::make_unique<ResultsViewTargeterDelegate>(this)));
#endif  // USE_AURA
}

// Note: The OnMouseMoved function is only called for the shadow area, as mouse-
// moved events are not dispatched through the view hierarchy but are direct-
// dispatched by RootView. This OnMouseEvent function is on the dispatch path
// for all mouse events of the window, so be careful to correctly mark events as
// "handled" above in subviews.
#if !defined(USE_AURA)

void FullWebUIOmniboxFrame::OnMouseEvent(ui::MouseEvent* event) {
  RoundedOmniboxResultsFrame::OnMouseEvent(event);
  // Mark the event as handled after forwarding to the parent widget so it
  // does not propagate up the popup's view hierarchy and get double-handled.
  event->SetHandled();
}

#endif  // !USE_AURA

gfx::Insets FullWebUIOmniboxFrame::GetEventForwardingInsets() const {
  // The widget is always expanded by the shadow margin, whether the shadow is
  // painted by this frame's border or by the page.
  const gfx::Insets insets = GetShadowInsets();
  int top_inset = insets.top() + GetLocationBarAlignmentInsets().top() +
                  GetLayoutConstant(LayoutConstant::kLocationBarHeight);
  return gfx::Insets::TLBR(top_inset, insets.left(), insets.bottom(),
                           insets.right());
}

#if defined(USE_AURA)
void FullWebUIOmniboxFrame::UpdateWindowTargeter() {
  if (!GetWidget() || !GetWidget()->GetNativeWindow()) {
    return;
  }
  auto* window = GetWidget()->GetNativeWindow();
  const gfx::Insets insets =
      forward_mouse_events() ? GetEventForwardingInsets() : GetShadowInsets();
  if (window->targeter()) {
    window->targeter()->SetInsets(insets);
  } else {
    auto targeter = std::make_unique<aura::WindowTargeter>();
    targeter->SetInsets(insets);
    window->SetEventTargeter(std::move(targeter));
  }
}
#endif  // USE_AURA

BEGIN_METADATA(FullWebUIOmniboxFrame)
END_METADATA
