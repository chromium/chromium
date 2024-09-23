// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/chosen_object_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "chrome/browser/ui/views/page_info/chosen_object_view_observer.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/page_info/page_info_delegate.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

ChosenObjectView::ChosenObjectView(
    std::unique_ptr<PageInfoUI::ChosenObjectInfo> info,
    std::u16string display_name)
    : info_(std::move(info)) {
  // TODO(crbug.com/40064612): Directly subclass `RichControlsContainerView`
  // instead of adding it as the only child.
  SetUseDefaultFillLayout(true);
  row_view_ = AddChildView(std::make_unique<RichControlsContainerView>());
  row_view_->SetTitle(display_name);

  // Create the delete button. It is safe to use base::Unretained here
  // because the button is owned by this object.
  std::unique_ptr<views::ImageButton> delete_button =
      views::CreateVectorImageButton(base::BindRepeating(
          &ChosenObjectView::ExecuteDeleteCommand, base::Unretained(this)));
  delete_button->SetRequestFocusOnPress(true);
  delete_button->SetTooltipText(l10n_util::GetStringFUTF16(
      info_->ui_info->delete_tooltip_string_id, display_name));
  views::InstallCircleHighlightPathGenerator(delete_button.get());

  // Disable the delete button for policy controlled objects and display the
  // allowed by policy string below for |secondary_label|.
  std::unique_ptr<views::Label> secondary_label;
  if (info_->chooser_object->source ==
      content_settings::SettingSource::kPolicy) {
    delete_button->SetEnabled(false);
    row_view_->AddSecondaryLabel(l10n_util::GetStringUTF16(
        info_->ui_info->allowed_by_policy_description_string_id));
  } else {
    row_view_->AddSecondaryLabel(
        l10n_util::GetStringUTF16(info_->ui_info->description_string_id));
  }

  delete_button_ = row_view_->AddControl(std::move(delete_button));

  UpdateIconImage(/*is_deleted=*/false);
  // Set flex rule, defined in `RichControlsContainerView`, to wrap the subtitle
  // text but size the parent view to match the content.
  SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(base::BindRepeating(
          &RichControlsContainerView::FlexRule, base::Unretained(row_view_))));
}

void ChosenObjectView::AddObserver(ChosenObjectViewObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ChosenObjectView::OnThemeChanged() {
  views::View::OnThemeChanged();
  const ui::ColorProvider* cp = GetColorProvider();
  views::SetImageFromVectorIconWithColor(
      delete_button_, vector_icons::kCloseRoundedIcon,
      cp->GetColor(kColorPageInfoChosenObjectDeleteButtonIcon),
      cp->GetColor(kColorPageInfoChosenObjectDeleteButtonIconDisabled));
}

ChosenObjectView::~ChosenObjectView() = default;

void ChosenObjectView::ResetPermission() {
  if (delete_button_->GetVisible()) {
    ExecuteDeleteCommand();
  }
}

void ChosenObjectView::ExecuteDeleteCommand() {
  // Policy-managed permissions cannot be deleted. This isn't normally
  // reachable but views::test::ButtonTestApi::NotifyClick doesn't check
  // before executing the PressedCallback.
  if (info_->chooser_object->source ==
      content_settings::SettingSource::kPolicy) {
    return;
  }

  // Change the icon to reflect the selected setting.
  UpdateIconImage(/*is_deleted=*/true);

  DCHECK(delete_button_->GetEnabled());
  DCHECK(delete_button_->GetVisible());
  delete_button_->SetVisible(false);

  // Hide the row after revoking access.
  SetVisible(false);

  observer_list_.Notify(&ChosenObjectViewObserver::OnChosenObjectDeleted,
                        *info_);
}

void ChosenObjectView::UpdateIconImage(bool is_deleted) const {
  row_view_->SetIcon(
      PageInfoViewFactory::GetChosenObjectIcon(*info_, is_deleted));
}

BEGIN_METADATA(ChosenObjectView)
END_METADATA
