// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bulleted_label_list_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace {
constexpr int kColumnSetId = 0;

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
  flags.setColor(views::style::GetColor(*this, views::style::CONTEXT_LABEL,
                                        views::style::STYLE_PRIMARY));
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
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = layout->AddColumnSet(kColumnSetId);

  int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     views::GridLayout::kFixedSize,
                     views::GridLayout::ColumnSize::kFixed, width, width);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                     views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  for (const auto& text : texts)
    AddLabel(text);
}

BulletedLabelListView::~BulletedLabelListView() {}

void BulletedLabelListView::AddLabel(const std::u16string& text) {
  views::GridLayout* layout =
      static_cast<views::GridLayout*>(GetLayoutManager());
  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);

  auto label = std::make_unique<views::Label>(text);

  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  layout->AddView(std::make_unique<BulletView>());
  layout->AddView(std::move(label));
}

BEGIN_METADATA(BulletedLabelListView, views::View)
END_METADATA
