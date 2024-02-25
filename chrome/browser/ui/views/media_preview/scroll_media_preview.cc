// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/scroll_media_preview.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace scroll_media_preview {

namespace {

// Currently basing this off what extension menu as well as chrome labs bubble
// uses for sizing as suggested as an initial fix by UI.
// TODO(b/324500642): Consider making this dynamic and handled by views. Ideally
// we wouldn't ever pop up so that they pop outside the screen.
constexpr int kMaxScrollViewHeight = 400;

class ScrollViewAndObserver : public views::ScrollView,
                              public views::ViewObserver {
  METADATA_HEADER(ScrollViewAndObserver, views::ScrollView)

 public:
  ScrollViewAndObserver()
      : views::ScrollView((ScrollView::ScrollWithLayers::kEnabled)) {}

  ScrollViewAndObserver(const ScrollViewAndObserver&) = delete;
  ScrollViewAndObserver& operator=(const ScrollViewAndObserver&) = delete;
  ~ScrollViewAndObserver() override {
    if (contents()) {
      contents()->RemoveObserver(this);
    }
  }

  void ObserveContents() {
    if (contents()) {
      contents()->AddObserver(this);
    }
  }

  // ViewObserver override:
  void OnViewPreferredSizeChanged(View* observed_view) override {
    PreferredSizeChanged();
  }
};

BEGIN_METADATA(ScrollViewAndObserver)
END_METADATA

}  // namespace

views::View* CreateScrollViewAndGetContents(views::View& parent_view,
                                            std::optional<size_t> index) {
  auto* scroll_view =
      parent_view.AddChildViewAt(std::make_unique<ScrollViewAndObserver>(),
                                 index.value_or(parent_view.children().size()));
  auto* contents = scroll_view->SetContents(std::make_unique<MediaView>());
  scroll_view->ObserveContents();

  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view->SetVerticalScrollBar(std::make_unique<views::OverlayScrollBar>(
      views::ScrollBar::Orientation::kVertical));
  scroll_view->SetDrawOverflowIndicator(false);

  const int kRoundedRadius = ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::ShapeContextTokens::kOmniboxExpandedRadius);
  scroll_view->SetViewportRoundedCornerRadius(
      gfx::RoundedCornersF(kRoundedRadius));

  scroll_view->ClipHeightTo(0, kMaxScrollViewHeight);
  return contents;
}

}  // namespace scroll_media_preview
