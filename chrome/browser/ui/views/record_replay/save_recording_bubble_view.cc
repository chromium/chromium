// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/record_replay/save_recording_bubble_view.h"

#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/record_replay/save_recording_bubble_controller.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace record_replay {

// static
void SaveRecordingBubbleView::Show(
    views::View* anchor_view,
    content::WebContents* web_contents,
    std::unique_ptr<SaveRecordingBubbleController> controller) {
  CHECK(controller);
  auto* bubble = new SaveRecordingBubbleView(anchor_view, web_contents,
                                             std::move(controller));
  bubble->SetMainImage(
      ui::ImageModel::FromImage(gfx::Image(gfx::CreateVectorIcon(
          vector_icons::kPhotoIcon, 100,
          anchor_view->GetColorProvider()->GetColor(ui::kColorIcon)))));
  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->ShowForReason(LocationBarBubbleDelegateView::USER_GESTURE);
}

SaveRecordingBubbleView::SaveRecordingBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    std::unique_ptr<SaveRecordingBubbleController> controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(std::move(controller)) {
  SetShowTitle(true);
  SetShowCloseButton(true);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kOk, u"Save");
  SetButtonLabel(ui::mojom::DialogButton::kCancel, u"Cancel");

  // Clean up when the widget is closed.
  SetCloseCallback(
      base::BindOnce(&SaveRecordingBubbleController::OnBubbleClosed,
                     base::Unretained(controller_.get())));
}

SaveRecordingBubbleView::~SaveRecordingBubbleView() = default;

void SaveRecordingBubbleView::Init() {
  const auto* layout_provider = views::LayoutProvider::Get();

  // Vertical Layout for content
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  // Description label.
  auto* description_label = AddChildView(std::make_unique<views::Label>(
      u"Next site visit you can repeat this task."));
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Input row
  auto* input_row = AddChildView(std::make_unique<views::View>());
  auto* row_layout =
      input_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          layout_provider->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));

  // Name label
  input_row->AddChildView(std::make_unique<views::Label>(u"Name"));

  // Name textfield.
  name_textfield_ =
      input_row->AddChildView(std::make_unique<views::Textfield>());
  name_textfield_->SetController(this);

  // Calculate placeholder text
  std::u16string placeholder_text;
  GURL url(controller_->GetUrl());
  if (url.is_valid()) {
    std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
        url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (domain.empty()) {
      domain = url.host();
    }
    // Strip TLD (approximate).
    size_t dot_pos = domain.find_last_of('.');
    if (dot_pos != std::string::npos && dot_pos > 0) {
      domain = domain.substr(0, dot_pos);
    }

    base::Time now = base::Time::Now();
    std::string date_str =
        base::UnlocalizedTimeFormatWithPattern(now, "dd-MM-yyyy");

    placeholder_text =
        base::UTF8ToUTF16(domain) + u"_" + base::UTF8ToUTF16(date_str);
  }

  name_textfield_->SetPlaceholderText(placeholder_text);
  name_textfield_->SetAccessibleName(placeholder_text);
  row_layout->SetFlexForView(name_textfield_, 1);

  // Focus the textfield initially.
  SetInitiallyFocusedView(name_textfield_);
}

std::u16string SaveRecordingBubbleView::GetWindowTitle() const {
  return u"Task saved";
}

bool SaveRecordingBubbleView::Accept() {
  controller_->OnSave(name_textfield_->GetText());
  return true;
}

bool SaveRecordingBubbleView::Cancel() {
  controller_->OnCancel();
  return true;
}

void SaveRecordingBubbleView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  // TODO(crbug.com/484307350): check if the name is valid from the controller.
  SetButtonEnabled(ui::mojom::DialogButton::kOk, !new_contents.empty());
  DialogModelChanged();
}

BEGIN_METADATA(SaveRecordingBubbleView)
END_METADATA

}  // namespace record_replay
