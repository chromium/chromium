// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/record_replay/replay_recording_bubble_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace record_replay {

// static
std::unique_ptr<views::Widget> ReplayRecordingBubbleView::Show(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents,
    const std::u16string& recording_name,
    base::OnceClosure on_replay_started) {
  auto bubble = std::make_unique<ReplayRecordingBubbleView>(
      anchor, web_contents, recording_name, std::move(on_replay_started));
  auto* bubble_ptr = bubble.get();
  bubble_ptr->SetMainImage(
      ui::ImageModel::FromImage(gfx::Image(gfx::CreateVectorIcon(
          vector_icons::kPlayArrowIcon, /*dip_size=*/100,
          web_contents->GetColorProvider().GetColor(ui::kColorIcon)))));
  auto widget = base::WrapUnique(views::BubbleDialogDelegateView::CreateBubble(
      std::move(bubble), views::Widget::InitParams::CLIENT_OWNS_WIDGET));
  bubble_ptr->ShowForReason(LocationBarBubbleDelegateView::USER_GESTURE);
  return widget;
}

ReplayRecordingBubbleView::ReplayRecordingBubbleView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents,
    const std::u16string& recording_name,
    base::OnceClosure on_replay_started)
    : LocationBarBubbleDelegateView(anchor, web_contents),
      recording_name_(recording_name),
      on_replay_started_(std::move(on_replay_started)) {
  SetShowTitle(true);
  SetShowCloseButton(true);
  // TODO(crbug.com/507035858): Remove (UT) when strings are
  // internationalized.
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kOk, u"Replay (UT)");
  SetButtonLabel(ui::mojom::DialogButton::kCancel, u"Cancel (UT)");
}

ReplayRecordingBubbleView::~ReplayRecordingBubbleView() = default;

void ReplayRecordingBubbleView::Init() {
  const auto* layout_provider = views::LayoutProvider::Get();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  auto* description_label = AddChildView(std::make_unique<views::Label>(
      u"Do you want to replay the following recording? (UT)"));
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto* name_label =
      AddChildView(std::make_unique<views::Label>(recording_name_));
  name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  name_label->SetFontList(views::Label::GetDefaultFontList().Derive(
      0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::BOLD));
}

std::u16string ReplayRecordingBubbleView::GetWindowTitle() const {
  return u"Replay Recording (UT)";
}

bool ReplayRecordingBubbleView::Accept() {
  if (on_replay_started_) {
    std::move(on_replay_started_).Run();
  }
  return true;
}

bool ReplayRecordingBubbleView::Cancel() {
  return true;
}

BEGIN_METADATA(ReplayRecordingBubbleView)
END_METADATA

}  // namespace record_replay
