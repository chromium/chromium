// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PermissionPromptBubbleBaseView,
                                      kMainViewId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PermissionPromptBubbleBaseView,
                                      kAllowButtonElementId);

PermissionPromptBubbleBaseView::PermissionPromptBubbleBaseView(
    Browser* browser,
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
    base::TimeTicks permission_requested_time,
    PermissionPromptStyle prompt_style,
    std::u16string window_title,
    std::u16string accessible_window_title,
    absl::optional<std::u16string> extra_text)
    : PermissionPromptBaseView(browser, delegate),
      browser_(browser),
      delegate_(delegate),
      permission_requested_time_(permission_requested_time),
      is_one_time_permission_(IsOneTimePermission(*delegate.get())),
      accessible_window_title_(accessible_window_title),
      window_title_(window_title) {
  // Note that browser_ may be null in unit tests.
  SetPromptStyle(prompt_style);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      DISTANCE_BUTTON_VERTICAL));

  set_close_on_deactivate(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  if (extra_text.has_value()) {
    auto* extra_text_label =
        AddChildView(std::make_unique<views::Label>(extra_text.value()));
    extra_text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    extra_text_label->SetMultiLine(true);
    extra_text_label->SetID(permissions::PermissionPromptViewID::
                                VIEW_ID_PERMISSION_PROMPT_EXTRA_TEXT);
    if (features::IsChromeRefresh2023()) {
      extra_text_label->SetTextStyle(views::style::STYLE_BODY_3);
      extra_text_label->SetEnabledColorId(kColorPermissionPromptRequestText);
    }
  }
  if (is_one_time_permission_) {
    SetButtons(ui::DIALOG_BUTTON_NONE);

    auto buttons_container = std::make_unique<views::View>();
    buttons_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        DISTANCE_BUTTON_VERTICAL));

    auto allow_once_button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(&PermissionPromptBubbleBaseView::
                                FilterUnintenedEventsAndRunCallbacks,
                            base::Unretained(this),
                            GetViewId(PermissionDialogButton::kAcceptOnce)),
        l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_THIS_TIME));
    allow_once_button->SetID(GetViewId(PermissionDialogButton::kAcceptOnce));

    auto allow_always_button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(&PermissionPromptBubbleBaseView::
                                FilterUnintenedEventsAndRunCallbacks,
                            base::Unretained(this),
                            GetViewId(PermissionDialogButton::kAccept)),
        l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_EVERY_VISIT));
    allow_always_button->SetProperty(views::kElementIdentifierKey,
                                     kAllowButtonElementId);
    allow_always_button->SetID(GetViewId(PermissionDialogButton::kAccept));

    int block_message_id =
        permissions::feature_params::kUseStrongerPromptLanguage.Get()
            ? IDS_PERMISSION_NEVER_ALLOW
            : IDS_PERMISSION_DONT_ALLOW;
    auto block_button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(&PermissionPromptBubbleBaseView::
                                FilterUnintenedEventsAndRunCallbacks,
                            base::Unretained(this),
                            GetViewId(PermissionDialogButton::kDeny)),
        l10n_util::GetStringUTF16(block_message_id));
    block_button->SetID(GetViewId(PermissionDialogButton::kDeny));

    if (features::IsChromeRefresh2023()) {
      allow_once_button->SetStyle(ui::ButtonStyle::kTonal);
      allow_always_button->SetStyle(ui::ButtonStyle::kTonal);
      block_button->SetStyle(ui::ButtonStyle::kTonal);
    }

    buttons_container->AddChildView(std::move(allow_once_button));
    buttons_container->AddChildView(std::move(allow_always_button));
    buttons_container->AddChildView(std::move(block_button));

    views::LayoutProvider* const layout_provider = views::LayoutProvider::Get();
    buttons_container->SetPreferredSize(gfx::Size(
        layout_provider->GetDistanceMetric(
            views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
            layout_provider->GetInsetsMetric(views::INSETS_DIALOG_BUTTON_ROW)
                .width(),
        buttons_container->GetPreferredSize().height()));
    SetExtraView(std::move(buttons_container));
  } else {
    SetButtonLabel(ui::DIALOG_BUTTON_OK,
                   l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));
    SetAcceptCallback(base::BindOnce(
        &PermissionPromptBubbleBaseView::RunButtonCallback,
        base::Unretained(this), GetViewId(PermissionDialogButton::kAccept)));

    SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                   l10n_util::GetStringUTF16(IDS_PERMISSION_DENY));
    SetCancelCallback(base::BindOnce(
        &PermissionPromptBubbleBaseView::RunButtonCallback,
        base::Unretained(this), GetViewId(PermissionDialogButton::kDeny)));

    if (features::IsChromeRefresh2023()) {
      SetButtonStyle(ui::DIALOG_BUTTON_OK, ui::ButtonStyle::kTonal);
      SetButtonStyle(ui::DIALOG_BUTTON_CANCEL, ui::ButtonStyle::kTonal);
    }
  }

  SetProperty(views::kElementIdentifierKey, kMainViewId);
}

PermissionPromptBubbleBaseView::~PermissionPromptBubbleBaseView() = default;

void PermissionPromptBubbleBaseView::Show() {
  CreateWidget();
  ShowWidget();
}

void PermissionPromptBubbleBaseView::CreateWidget() {
  DCHECK(browser_->window());

  UpdateAnchorPosition();

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);

  if (!is_one_time_permission_) {
    GetOkButton()->SetProperty(views::kElementIdentifierKey,
                               kAllowButtonElementId);
  }

  if (base::FeatureList::IsEnabled(views::features::kWidgetLayering)) {
    widget->SetZOrderSublevel(ChromeWidgetSublevel::kSublevelSecurity);
  }
}

void PermissionPromptBubbleBaseView::ShowWidget() {
  // If a browser window (or popup) other than the bubble parent has focus,
  // don't take focus.
  if (browser_->window()->IsActive()) {
    GetWidget()->Show();
  } else {
    GetWidget()->ShowInactive();
  }

  SizeToContents();
}

void PermissionPromptBubbleBaseView::UpdateAnchorPosition() {
  bubble_anchor_util::AnchorConfiguration configuration =
      bubble_anchor_util::GetPermissionPromptBubbleAnchorConfiguration(
          browser_);
  SetAnchorView(configuration.anchor_view);
  // In fullscreen, `anchor_view` may be nullptr because the toolbar is hidden,
  // therefore anchor to the browser window instead.
  if (configuration.anchor_view) {
    set_parent_window(configuration.anchor_view->GetWidget()->GetNativeView());
  } else {
    set_parent_window(
        platform_util::GetViewForWindow(browser_->window()->GetNativeWindow()));
  }
  SetHighlightedButton(configuration.highlighted_button);
  if (!configuration.anchor_view) {
    SetAnchorRect(bubble_anchor_util::GetPageInfoAnchorRect(browser_));
  }
  SetArrow(configuration.bubble_arrow);
}

void PermissionPromptBubbleBaseView::SetPromptStyle(
    PermissionPromptStyle prompt_style) {
  prompt_style_ = prompt_style;
  // If bubble hanging off the padlock icon, with no chip showing, closing the
  // dialog should dismiss the pending request because there's no way to bring
  // the bubble back.
  if (prompt_style_ == PermissionPromptStyle::kBubbleOnly) {
    DialogDelegate::SetCloseCallback(
        base::BindOnce(&PermissionPromptBubbleBaseView::ClosingPermission,
                       base::Unretained(this)));
  } else if (prompt_style_ == PermissionPromptStyle::kChip ||
             prompt_style_ == PermissionPromptStyle::kQuietChip) {
    // Override the `CloseCallback` if it was set previously.
    DialogDelegate::SetCloseCallback(base::DoNothing());
  }
}

bool PermissionPromptBubbleBaseView::ShouldShowCloseButton() const {
  return true;
}

std::u16string PermissionPromptBubbleBaseView::GetWindowTitle() const {
  return window_title_;
}

std::u16string PermissionPromptBubbleBaseView::GetAccessibleWindowTitle()
    const {
  return accessible_window_title_;
}

void PermissionPromptBubbleBaseView::ClosingPermission() {
  DCHECK_EQ(prompt_style_, PermissionPromptStyle::kBubbleOnly);
  RecordDecision(permissions::PermissionAction::DISMISSED);
  if (delegate_) {
    delegate_->Dismiss();
  }
}

void PermissionPromptBubbleBaseView::RunButtonCallback(int button_id) {
  PermissionDialogButton button = GetPermissionDialogButton(button_id);
  switch (button) {
    case PermissionDialogButton::kAccept:
      RecordDecision(permissions::PermissionAction::GRANTED);
      delegate_->Accept();
      return;
    case PermissionDialogButton::kAcceptOnce:
      RecordDecision(permissions::PermissionAction::GRANTED_ONCE);
      delegate_->AcceptThisTime();
      return;
    case PermissionDialogButton::kDeny:
      RecordDecision(permissions::PermissionAction::DENIED);
      delegate_->Deny();
      return;
  }
  NOTREACHED();
}

// static
bool PermissionPromptBubbleBaseView::IsOneTimePermission(
    permissions::PermissionPrompt::Delegate& delegate) {
  if (!base::FeatureList::IsEnabled(
          permissions::features::kOneTimePermission)) {
    return false;
  }
  CHECK_GT(delegate.Requests().size(), 0u);
  for (auto* request : delegate.Requests()) {
    auto content_setting_type =
        permissions::RequestTypeToContentSettingsType(request->request_type());
    if (!content_setting_type.has_value() ||
        !permissions::PermissionUtil::CanPermissionBeAllowedOnce(
            content_setting_type.value())) {
      return false;
    }
  }
  return true;
}

void PermissionPromptBubbleBaseView::RecordDecision(
    permissions::PermissionAction action) {
  const std::string uma_suffix =
      permissions::PermissionUmaUtil::GetPermissionActionString(action);

  std::string time_to_decision_uma_name = std::string();

  switch (prompt_style_) {
    case PermissionPromptStyle::kBubbleOnly:
      time_to_decision_uma_name = "Permissions.Prompt.TimeToDecision";
      break;
    case PermissionPromptStyle::kEmbeddedElementSecondaryUI:
      time_to_decision_uma_name =
          "Permissions.kEmbeddedElementSecondaryUI.TimeToDecision";
      break;
    case PermissionPromptStyle::kChip:
      time_to_decision_uma_name = "Permissions.Chip.TimeToDecision";
      break;
    default:
      // |PermissionPromptStyle::kQuietChip| and
      // |PermissionPromptStyle::kLocationBarRightIcon| do not use
      // PermissionPromptBubbleBaseView and will reach this case.
      NOTREACHED();
  }

  base::UmaHistogramLongTimes(
      time_to_decision_uma_name + "." + uma_suffix,
      base::TimeTicks::Now() - permission_requested_time_);
}

BEGIN_METADATA(PermissionPromptBubbleBaseView, views::BubbleDialogDelegateView)
END_METADATA
