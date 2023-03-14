// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bulleted_label_list_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/table_layout.h"

namespace {

class BulletView : public views::View {
 public:
  METADATA_HEADER(BulletView);
  BulletView() = default;
  BulletView(const BulletView&) = delete;
  BulletView& operator=(const BulletView&) = delete;

  void OnPaint(gfx::Canvas* canvas) override;
};

void BulletView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  SkScalar radius = std::min(height(), width()) / 8.0;
  gfx::Point center = GetLocalBounds().CenterPoint();

  SkPath path;
  path.addCircle(center.x(), center.y(), radius);

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(GetColorProvider()->GetColor(views::style::GetColorId(
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY)));
  flags.setAntiAlias(true);

  canvas->DrawPath(path, flags);
}

BEGIN_METADATA(BulletView, views::View)
END_METADATA

}  // namespace

BulletedLabelListView::BulletedLabelListView()
    : BulletedLabelListView(std::vector<std::u16string>()) {}

BulletedLabelListView::BulletedLabelListView(
    const std::vector<std::u16string>& texts) {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
  SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(views::LayoutAlignment::kStretch,
                  views::LayoutAlignment::kStretch,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kFixed, width, width)
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kStretch, 1.0,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0);

  for (const auto& text : texts)
    AddLabel(text);
}

BulletedLabelListView::~BulletedLabelListView() = default;

void BulletedLabelListView::AddLabel(const std::u16string& text) {
  views::TableLayout* layout =
      static_cast<views::TableLayout*>(GetLayoutManager());
  layout->AddRows(1, views::TableLayout::kFixedSize);

  AddChildView(std::make_unique<BulletView>());
  auto* label = AddChildView(std::make_unique<views::Label>(text));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

BEGIN_METADATA(BulletedLabelListView, views::View)
END_METADATA
