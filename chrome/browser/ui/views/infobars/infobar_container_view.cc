// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_container_view.h"

#include <numeric>

#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/focus_ring.h"

namespace {

class ContentShadow : public views::View {
  METADATA_HEADER(ContentShadow, views::View)

 public:
  ContentShadow();

 protected:
  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnPaint(gfx::Canvas* canvas) override;
};

ContentShadow::ContentShadow() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

gfx::Size ContentShadow::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(0, views::BubbleBorder::GetBorderAndShadowInsets().height());
}

void ContentShadow::OnPaint(gfx::Canvas* canvas) {
  // Outdent the sides to make the shadow appear uniform in the corners.
  gfx::RectF container_bounds(parent()->GetLocalBounds());
  View::ConvertRectToTarget(parent(), this, &container_bounds);
  container_bounds.Inset(
      gfx::InsetsF::VH(0, -views::BubbleBorder::kShadowBlur));

  views::BubbleBorder::DrawBorderAndShadow(gfx::RectFToSkRect(container_bounds),
                                           canvas, GetColorProvider());
}

BEGIN_METADATA(ContentShadow)
END_METADATA

}  // namespace

constexpr int kSeparatorHeightDip = 1;

InfoBarContainerView::InfoBarContainerView(Delegate* delegate)
    : infobars::InfoBarContainer(delegate),
      content_shadow_(new ContentShadow()) {
  SetID(VIEW_ID_INFO_BAR_CONTAINER);
  AddChildView(content_shadow_.get());
  views::SetCascadingColorProviderColor(this, views::kCascadingBackgroundColor,
                                        kColorToolbar);
  SetBackground(
      views::CreateThemedSolidBackground(kColorInfoBarContentAreaSeparator));

  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF8(IDS_ACCNAME_INFOBAR_CONTAINER));
}

InfoBarContainerView::~InfoBarContainerView() {
  RemoveAllInfoBarsForDestruction();
}

bool InfoBarContainerView::IsEmpty() const {
  // NOTE: Can't check if the size IsEmpty() since it's always 0-width.
  return GetPreferredSize().height() == 0;
}

void InfoBarContainerView::Layout(PassKey) {
  const auto set_bounds = [this](int top, View* child) {
    const int height = static_cast<InfoBarView*>(child)->computed_height();
    // Do not add separator dip if it's the first infobar. The first infobar
    // should be flush with the top of InfoBarContainerView.
    int add_separator_height =
        (child == children().front()) ? 0 : kSeparatorHeightDip;
    child->SetBounds(0, top + add_separator_height, width(), height);
    return child->bounds().bottom();
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

gfx::Size InfoBarContainerView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const auto enlarge_size = [this](const gfx::Size& size, const View* child) {
    const gfx::Size child_size = child->GetPreferredSize();
    int add_separator_height =
        (child == children().front()) ? 0 : kSeparatorHeightDip;
    return gfx::Size(
        std::max(size.width(), child_size.width()),
        size.height() + child_size.height() + add_separator_height);
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

BEGIN_METADATA(InfoBarContainerView)
END_METADATA
