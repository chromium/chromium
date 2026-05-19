// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/record_replay/replay_recording_bubble_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/record_replay/core/browser/record_replay_manager.h"
#include "components/record_replay/core/browser/recording.pb.h"
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
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace record_replay {

// static
std::unique_ptr<views::Widget> ReplayRecordingBubbleView::Show(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents,
    std::vector<record_replay::Recording> recordings,
    base::WeakPtr<RecordReplayManager> manager) {
  auto bubble = std::make_unique<ReplayRecordingBubbleView>(
      anchor, web_contents, std::move(recordings), std::move(manager));
  auto* bubble_ptr = bubble.get();
  auto widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble).release());
  bubble_ptr->ShowForReason(LocationBarBubbleDelegateView::USER_GESTURE);
  return widget;
}

ReplayRecordingBubbleView::ReplayRecordingBubbleView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents,
    std::vector<record_replay::Recording> recordings,
    base::WeakPtr<RecordReplayManager> manager)
    : LocationBarBubbleDelegateView(anchor, web_contents),
      recordings_(std::move(recordings)),
      manager_(std::move(manager)) {
  SetShowTitle(true);
  SetShowCloseButton(true);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  if (web_contents) {
    SetSubtitle(base::UTF8ToUTF16(web_contents->GetLastCommittedURL().host()));
  }
}

ReplayRecordingBubbleView::~ReplayRecordingBubbleView() = default;

void ReplayRecordingBubbleView::Init() {
  const auto* layout_provider = views::LayoutProvider::Get();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  // "Record a new task" section
  auto* record_container = AddChildView(std::make_unique<views::View>());
  auto* record_layout =
      record_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          layout_provider->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  record_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* record_text_container =
      record_container->AddChildView(std::make_unique<views::View>());
  record_text_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  auto* record_title = record_text_container->AddChildView(
      std::make_unique<views::Label>(u"Record a new task (UT)"));
  record_title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  record_title->SetFontList(views::Label::GetDefaultFontList().Derive(
      /*size_delta=*/1, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::BOLD));

  auto* record_subtitle = record_text_container->AddChildView(
      std::make_unique<views::Label>(u"Automate recurring tasks (UT)"));
  record_subtitle->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  record_subtitle->SetEnabledColor(ui::kColorSecondaryForeground);

  auto* record_button =
      record_container->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&ReplayRecordingBubbleView::OnRecordButtonClicked,
                              base::Unretained(this)),
          u"Record (UT)"));
  record_button->SetStyle(ui::ButtonStyle::kProminent);

  record_layout->SetFlexForView(record_text_container, /*flex=*/1);

  if (!recordings_.empty()) {
    AddRecentTasks();
  }
}

void ReplayRecordingBubbleView::AddRecentTasks() {
  const auto* layout_provider = views::LayoutProvider::Get();
  AddChildView(std::make_unique<views::Separator>());

  auto* recent_tasks_label =
      AddChildView(std::make_unique<views::Label>(u"Recent tasks (UT)"));
  recent_tasks_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  recent_tasks_label->SetFontList(views::Label::GetDefaultFontList().Derive(
      /*size_delta=*/0, gfx::Font::FontStyle::NORMAL,
      gfx::Font::Weight::NORMAL));

  for (size_t i = 0; i < recordings_.size(); ++i) {
    const auto& recording = recordings_[i];
    auto* task_container = AddChildView(std::make_unique<views::View>());
    auto* task_layout =
        task_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
            layout_provider->GetDistanceMetric(
                views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
    task_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto* icon =
        task_container->AddChildView(std::make_unique<views::ImageView>());
    icon->SetImage(ui::ImageModel::FromVectorIcon(
        vector_icons::kScreenRecordOldIcon, ui::kColorIcon, /*icon_size=*/32));

    auto* task_text_container =
        task_container->AddChildView(std::make_unique<views::View>());
    task_text_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    auto* task_title = task_text_container->AddChildView(
        std::make_unique<views::Label>(base::UTF8ToUTF16(recording.name())));
    task_title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    task_title->SetFontList(views::Label::GetDefaultFontList().Derive(
        /*size_delta=*/0, gfx::Font::FontStyle::NORMAL,
        gfx::Font::Weight::BOLD));

    task_layout->SetFlexForView(task_text_container, /*flex=*/1);

    auto* replay_button =
        task_container->AddChildView(std::make_unique<views::MdTextButton>(
            base::BindRepeating(
                &ReplayRecordingBubbleView::OnReplayButtonClicked,
                base::Unretained(this), i),
            u"Replay (UT)"));
    replay_button->SetStyle(ui::ButtonStyle::kText);
  }
}

std::u16string ReplayRecordingBubbleView::GetWindowTitle() const {
  return u"Saved tasks (UT)";
}

void ReplayRecordingBubbleView::SetReplayCallbackForTesting(
    base::RepeatingClosure callback) {  // IN-TEST
  replay_callback_for_testing_ = std::move(callback);
}

void ReplayRecordingBubbleView::OnReplayButtonClicked(size_t index) {
  if (replay_callback_for_testing_) {
    replay_callback_for_testing_.Run();
    return;
  }
  if (manager_) {
    CHECK_LT(index, recordings_.size());
    manager_->StartReplaySpecific(recordings_[index]);
  }
  GetWidget()->Close();
}

void ReplayRecordingBubbleView::OnRecordButtonClicked() {
  if (manager_) {
    manager_->StartRecording();
  }
  GetWidget()->Close();
}

BEGIN_METADATA(ReplayRecordingBubbleView)
END_METADATA

}  // namespace record_replay
