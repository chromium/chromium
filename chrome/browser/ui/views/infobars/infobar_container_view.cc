// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_container_view.h"

#include <numeric>

#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/bubble/bubble_border.h"

namespace {

class ContentShadow : public views::View {
 public:
  ContentShadow();

 protected:
  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;
};

ContentShadow::ContentShadow() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

gfx::Size ContentShadow::CalculatePreferredSize() const {
  return gfx::Size(0, views::BubbleBorder::GetBorderAndShadowInsets().height());
}

void ContentShadow::OnPaint(gfx::Canvas* canvas) {
  // Outdent the sides to make the shadow appear uniform in the corners.
  gfx::RectF container_bounds(parent()->GetLocalBounds());
  View::ConvertRectToTarget(parent(), this, &container_bounds);
  container_bounds.Inset(-views::BubbleBorder::kShadowBlur, 0);

  views::BubbleBorder::DrawBorderAndShadow(gfx::RectFToSkRect(container_bounds),
                                           &cc::PaintCanvas::drawRect, canvas);
}

}  // namespace

// static
const char InfoBarContainerView::kViewClassName[] = "InfoBarContainerView";

InfoBarContainerView::InfoBarContainerView(Delegate* delegate)
    : infobars::InfoBarContainer(delegate),
      content_shadow_(new ContentShadow()) {
  SetID(VIEW_ID_INFO_BAR_CONTAINER);
  AddChildView(content_shadow_);
}

InfoBarContainerView::~InfoBarContainerView() {
  RemoveAllInfoBarsForDestruction();
}

void InfoBarContainerView::Layout() {
  const auto set_bounds = [this](int top, auto* child) {
    const int height = static_cast<InfoBarView*>(child)->computed_height();
    child->SetBounds(0, top, width(), height);
    return top + height;
  };
  DCHECK_EQ(content_shadow_, children().back());
  const int top = std::accumulate(children().begin(),
                                  std::prev(children().end()), 0, set_bounds);

  // The shadow is positioned flush with the bottom infobar, with the separator
  // there drawn by the shadow code (so we don't have to extend our bounds out
  // to be able to draw it; see comments in CalculatePreferredSize() on why the
  // shadow is drawn outside the container bounds).
  content_shadow_->SetBounds(0, top, width(),
                             content_shadow_->GetPreferredSize().height());
}

const char* InfoBarContainerView::GetClassName() const {
  return kViewClassName;
}

void InfoBarContainerView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kGroup;
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ACCNAME_INFOBAR_CONTAINER));
}

gfx::Size InfoBarContainerView::CalculatePreferredSize() const {
  const auto enlarge_size = [](const gfx::Size& size, const auto* child) {
    const gfx::Size child_size = child->GetPreferredSize();
    return gfx::Size(std::max(size.width(), child_size.width()),
                     size.height() + child_size.height());
  };
  // Don't reserve space for the bottom shadow here.  Because the shadow paints
  // to its own layer and this class doesn't, it can paint outside the size
  // computed here.  Not including the shadow bounds means the browser will
  // automatically lay out web content beginning below the bottom infobar
  // (instead of below the shadow), and clicks in the shadow region will go to
  // the web content instead of the infobars; both of these effects are
  // desirable.  On the other hand, it also means the browser doesn't know the
  // shadow is there and could lay out something atop it or size the window too
  // small for it; but these are unlikely.
  DCHECK_EQ(content_shadow_, children().back());
  return std::accumulate(children().cbegin(), std::prev(children().cend()),
                         gfx::Size(), enlarge_size);
}

void InfoBarContainerView::PlatformSpecificAddInfoBar(
    infobars::InfoBar* infobar,
    size_t position) {
  AddChildViewAt(static_cast<InfoBarView*>(infobar),
                 static_cast<int>(position));
}

void InfoBarContainerView::PlatformSpecificRemoveInfoBar(
    infobars::InfoBar* infobar) {
  RemoveChildView(static_cast<InfoBarView*>(infobar));
}

void InfoBarContainerView::PlatformSpecificInfoBarStateChanged(
    bool is_animating) {
  // If we just finished animating the removal of the previous top infobar, the
  // new top infobar should now stop drawing a top separator.  In this case the
  // previous top infobar is zero-sized but has not yet been removed from the
  // container, so we'll have at least three children (two infobars and a
  // shadow), and the new top infobar is child 1.  The conditional below
  // won't exclude cases where we're adding rather than removing an infobar, but
  // doing unnecessary work on the second infobar in those cases is harmless.
  if (!is_animating && children().size() > 2) {
    // Dropping the separator may change the height.
    auto* infobar = static_cast<InfoBarView*>(children()[1]);
    infobar->RecalculateHeight();

    // We need to force a paint whether or not the height actually changed.
    infobar->SchedulePaint();
  }
}
