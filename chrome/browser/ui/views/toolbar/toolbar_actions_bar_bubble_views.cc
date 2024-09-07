// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/locale_settings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"

namespace {
const int kBubbleExtraIconSize = 16;
}

ToolbarActionsBarBubbleViews::ToolbarActionsBarBubbleViews(
    views::View* anchor_view,
    bool anchored_to_action,
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> delegate)
    : views::BubbleDialogDelegateView(anchor_view,
                                      views::BubbleBorder::TOP_RIGHT),
      delegate_(std::move(delegate)),
      anchored_to_action_(anchored_to_action) {
  std::u16string ok_text = delegate_->GetActionButtonText();
  std::u16string cancel_text = delegate_->GetDismissButtonText();

  int buttons = static_cast<int>(ui::mojom::DialogButton::kNone);
  if (!ok_text.empty())
    buttons |= static_cast<int>(ui::mojom::DialogButton::kOk);
  if (!cancel_text.empty())
    buttons |= static_cast<int>(ui::mojom::DialogButton::kCancel);
  SetButtons(buttons);
  SetDefaultButton(static_cast<int>(delegate_->GetDefaultDialogButton()));
  SetButtonLabel(ui::mojom::DialogButton::kOk, ok_text);
  SetButtonLabel(ui::mojom::DialogButton::kCancel, cancel_text);
  SetExtraView(CreateExtraInfoView());

  SetAcceptCallback(base::BindOnce(
      &ToolbarActionsBarBubbleViews::NotifyDelegateOfClose,
      base::Unretained(this), ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE));
  SetCancelCallback(base::BindOnce(
      &ToolbarActionsBarBubbleViews::NotifyDelegateOfClose,
      base::Unretained(this),
      ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_USER_ACTION));
  SetCloseCallback(base::BindOnce(
      &ToolbarActionsBarBubbleViews::NotifyDelegateOfClose,
      base::Unretained(this),
      ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_DEACTIVATION));

  DCHECK(anchor_view);
}

ToolbarActionsBarBubbleViews::~ToolbarActionsBarBubbleViews() {}

std::string ToolbarActionsBarBubbleViews::GetAnchorActionId() const {
  return delegate_->GetAnchorActionId();
}

std::unique_ptr<views::View>
ToolbarActionsBarBubbleViews::CreateExtraInfoView() {
  std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
      extra_view_info = delegate_->GetExtraViewInfo();

  if (!extra_view_info)
    return nullptr;

  std::unique_ptr<views::ImageView> icon;
  if (extra_view_info->resource) {
    icon = std::make_unique<views::ImageView>();
    icon->SetImage(ui::ImageModel::FromVectorIcon(
        *extra_view_info->resource, ui::kColorIcon, kBubbleExtraIconSize));
  }

  std::unique_ptr<views::View> extra_view;
  const std::u16string& text = extra_view_info->text;
  if (!text.empty()) {
    if (extra_view_info->is_learn_more) {
      auto image_button = views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&ToolbarActionsBarBubbleViews::ButtonPressed,
                              base::Unretained(this)),
          vector_icons::kHelpOutlineIcon);
      image_button->SetTooltipText(text);
      learn_more_button_ = image_button.get();
      extra_view = std::move(image_button);
    } else {
      extra_view = std::make_unique<views::Label>(text);
    }
  }

  if (icon && extra_view) {
    std::unique_ptr<views::View> parent = std::make_unique<views::View>();
    parent->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_VERTICAL)));
    parent->AddChildView(std::move(icon));
    parent->AddChildView(std::move(extra_view));
    return parent;
  }
  return icon ? std::move(icon) : std::move(extra_view);
}

void ToolbarActionsBarBubbleViews::ButtonPressed() {
  NotifyDelegateOfClose(ToolbarActionsBarBubbleDelegate::CLOSE_LEARN_MORE);
  // Note that the Widget may or may not already be closed at this point,
  // depending on delegate_->ShouldCloseOnDeactivate(). Widget::Close() protects
  // against multiple calls (so long as they are not nested), and Widget
  // destruction is asynchronous, so it is safe to call Close() again.
  GetWidget()->Close();
}

void ToolbarActionsBarBubbleViews::NotifyDelegateOfClose(
    ToolbarActionsBarBubbleDelegate::CloseAction action) {
  if (delegate_notified_of_close_)
    return;
  delegate_notified_of_close_ = true;
  delegate_->OnBubbleClosed(action);
}

std::u16string ToolbarActionsBarBubbleViews::GetWindowTitle() const {
  return delegate_->GetHeadingText();
}

bool ToolbarActionsBarBubbleViews::ShouldShowCloseButton() const {
  return true;
}

void ToolbarActionsBarBubbleViews::AddedToWidget() {
  // This is currently never added to a widget when the widget is already
  // visible. If this changed, delegate_->OnBubbleShown() would also need to be
  // called here.
  DCHECK(!GetWidget()->IsVisible());
  DCHECK(!observer_notified_of_show_);

  GetWidget()->AddObserver(this);
  BubbleDialogDelegateView::AddedToWidget();
}

void ToolbarActionsBarBubbleViews::RemovedFromWidget() {
  GetWidget()->RemoveObserver(this);
}

void ToolbarActionsBarBubbleViews::Init() {
  std::u16string body_text_string = delegate_->GetBodyText(anchored_to_action_);
  if (body_text_string.empty()) {
    return;
  }

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  int width =
      provider->GetDistanceMetric(views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
      margins().width();

  if (!body_text_string.empty()) {
    body_text_ = new views::Label(body_text_string);
    body_text_->SetMultiLine(true);
    body_text_->SizeToFit(width);
    body_text_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(body_text_.get());
  }
}

void ToolbarActionsBarBubbleViews::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  DCHECK_EQ(GetWidget(), widget);
  if (!visible)
    return;

  GetWidget()->RemoveObserver(this);
  if (observer_notified_of_show_)
    return;

  observer_notified_of_show_ = true;
  // Using Unretained is safe here because the delegate (which might invoke the
  // callback) is owned by this object.
  delegate_->OnBubbleShown(
      base::BindOnce(&views::Widget::Close, base::Unretained(GetWidget())));
}

BEGIN_METADATA(ToolbarActionsBarBubbleViews)
ADD_READONLY_PROPERTY_METADATA(std::string, AnchorActionId)
END_METADATA
