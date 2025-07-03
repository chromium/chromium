// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"

#include <memory>
#include <optional>
#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PermissionPromptBubbleBaseView,
                                      kMainViewId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PermissionPromptBubbleBaseView,
                                      kBlockButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PermissionPromptBubbleBaseView,
                                      kAllowButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PermissionPromptBubbleBaseView,
                                      kAllowOnceButtonElementId);

namespace {
std::string GetPermissionActionString(
    PermissionPromptBubbleBaseView::PermissionDialogButton button) {
  switch (button) {
    case PermissionPromptBubbleBaseView::PermissionDialogButton::kAccept:
      return "Accepted";
    case PermissionPromptBubbleBaseView::PermissionDialogButton::kAcceptOnce:
      return "AcceptedOnce";
    case PermissionPromptBubbleBaseView::PermissionDialogButton::kDeny:
      return "Denied";
    default:
      NOTREACHED();
  }
}
}  // namespace

PermissionPromptBubbleBaseView::PermissionPromptBubbleBaseView(
    Browser* browser,
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
    PermissionPromptStyle prompt_style)
    : PermissionPromptBaseView(browser, delegate),
      delegate_(delegate),
      is_one_time_permission_(IsOneTimePermission(*delegate.get())) {
  // Note that browser() may be null in unit tests.
  SetPromptStyle(prompt_style);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      DISTANCE_BUTTON_VERTICAL));

  set_close_on_deactivate(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  SetProperty(views::kElementIdentifierKey, kMainViewId);
}

PermissionPromptBubbleBaseView::~PermissionPromptBubbleBaseView() = default;

void PermissionPromptBubbleBaseView::CreatePermissionButtons(
    const std::u16string& allow_always_text,
    const std::u16string& block_text) {
  if (is_one_time_permission_) {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

    auto buttons_container = std::make_unique<views::View>();
    buttons_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        DISTANCE_BUTTON_VERTICAL));

    auto allow_once_button =
        views::Builder<views::MdTextButton>()
            .SetText(l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_THIS_TIME))
            .SetCallback(base::BindRepeating(
                &PermissionPromptBubbleBaseView::
                    FilterUnintenedEventsAndRunCallbacks,
                base::Unretained(this),
                GetViewId(PermissionDialogButton::kAcceptOnce)))
            .SetID(GetViewId(PermissionDialogButton::kAcceptOnce))
            .SetProperty(views::kElementIdentifierKey,
                         kAllowOnceButtonElementId)
            .Build();

    auto allow_always_button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(&PermissionPromptBubbleBaseView::
                                FilterUnintenedEventsAndRunCallbacks,
                            base::Unretained(this),
                            GetViewId(PermissionDialogButton::kAccept)),
        allow_always_text);
    allow_always_button->SetProperty(views::kElementIdentifierKey,
                                     kAllowButtonElementId);
    allow_always_button->SetID(GetViewId(PermissionDialogButton::kAccept));

    auto block_button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(&PermissionPromptBubbleBaseView::
                                FilterUnintenedEventsAndRunCallbacks,
                            base::Unretained(this),
                            GetViewId(PermissionDialogButton::kDeny)),
        block_text);
    block_button->SetProperty(views::kElementIdentifierKey,
                              kBlockButtonElementId);
    block_button->SetID(GetViewId(PermissionDialogButton::kDeny));

    allow_once_button->SetStyle(ui::ButtonStyle::kTonal);
    allow_always_button->SetStyle(ui::ButtonStyle::kTonal);
    block_button->SetStyle(ui::ButtonStyle::kTonal);

    buttons_container->AddChildView(std::move(allow_always_button));
    buttons_container->AddChildView(std::move(allow_once_button));
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
    SetButtonLabel(ui::mojom::DialogButton::kOk,
                   l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));
    SetAcceptCallback(base::BindOnce(
        &PermissionPromptBubbleBaseView::RunButtonCallback,
        base::Unretained(this), GetViewId(PermissionDialogButton::kAccept)));

    SetButtonLabel(ui::mojom::DialogButton::kCancel,
                   l10n_util::GetStringUTF16(IDS_PERMISSION_DENY));
    SetCancelCallback(base::BindOnce(
        &PermissionPromptBubbleBaseView::RunButtonCallback,
        base::Unretained(this), GetViewId(PermissionDialogButton::kDeny)));

    SetButtonStyle(ui::mojom::DialogButton::kOk, ui::ButtonStyle::kTonal);
    SetButtonStyle(ui::mojom::DialogButton::kCancel, ui::ButtonStyle::kTonal);
  }
}

void PermissionPromptBubbleBaseView::CreateExtraTextLabel(
    const std::u16string& extra_text) {
  auto extra_text_label = views::Builder<views::Label>()
                              .SetText(extra_text)
                              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                              .SetMultiLine(true)
                              .SetID(permissions::PermissionPromptViewID::
                                         VIEW_ID_PERMISSION_PROMPT_EXTRA_TEXT)
                              .Build();
  extra_text_label->SetTextStyle(views::style::STYLE_BODY_3);
  extra_text_label->SetEnabledColor(kColorPermissionPromptRequestText);
  AddChildView(std::move(extra_text_label));
}

void PermissionPromptBubbleBaseView::Show() {
  CreateWidget();
  ShowWidget();
}

void PermissionPromptBubbleBaseView::CreateWidget() {
  CHECK(browser()->window());

  UpdateAnchorPosition();

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);

  if (!is_one_time_permission_) {
    GetCancelButton()->SetProperty(views::kElementIdentifierKey,
                                   kBlockButtonElementId);
    GetOkButton()->SetProperty(views::kElementIdentifierKey,
                               kAllowButtonElementId);
  }

  widget->SetZOrderSublevel(ChromeWidgetSublevel::kSublevelSecurity);
}

void PermissionPromptBubbleBaseView::ShowWidget() {
  // If a browser window (or popup) other than the bubble parent has focus,
  // don't take focus.
  if (browser()->window()->IsActive()) {
    GetWidget()->Show();
  } else {
    GetWidget()->ShowInactive();
  }
}

void PermissionPromptBubbleBaseView::UpdateAnchorPosition() {
  AnchorToPageInfoOrChip();
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

void PermissionPromptBubbleBaseView::ClosingPermission() {
  DCHECK_EQ(prompt_style_, PermissionPromptStyle::kBubbleOnly);

  if (delegate_) {
    permissions::PermissionUmaUtil::RecordActionBrowserAlwaysActive(
        request_type(), "Dismissed", record_browser_always_active_value());
    delegate_->Dismiss();
  }
}

void PermissionPromptBubbleBaseView::RunButtonCallback(int button_id) {
  PermissionDialogButton button = GetPermissionDialogButton(button_id);
  permissions::PermissionUmaUtil::RecordActionBrowserAlwaysActive(
      request_type(), GetPermissionActionString(button),
      record_browser_always_active_value());
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
#if BUILDFLAG(IS_CHROMEOS)
  // `PERMISSION_SMART_CARD` is essentially a chooser permission without an
  // actual chooser - thus, there is no blocklist of devices and no real
  // difference between deny and dismiss. Ergo, deny clicks should be handled as
  // dismiss, including imposing embargo and recording appropriate histograms.
  const bool is_deny_supported =
      request_type() != permissions::RequestTypeForUma::PERMISSION_SMART_CARD;
#endif  // BUILDFLAG(IS_CHROMEOS)
  if (browser_view && browser_view->GetLocationBarView()->GetChipController() &&
      browser_view->GetLocationBarView()
          ->GetChipController()
          ->IsPermissionPromptChipVisible() &&
      browser_view->GetLocationBarView()
          ->GetChipController()
          ->IsBubbleShowing()) {
    ChipController* chip_controller =
        browser_view->GetLocationBarView()->GetChipController();
    switch (button) {
      case PermissionDialogButton::kAccept:
        chip_controller->PromptDecided(permissions::PermissionAction::GRANTED);
        return;

      case PermissionDialogButton::kAcceptOnce:
        chip_controller->PromptDecided(
            permissions::PermissionAction::GRANTED_ONCE);
        return;

      case PermissionDialogButton::kDeny:
        chip_controller->PromptDecided(
#if BUILDFLAG(IS_CHROMEOS)
            is_deny_supported ? permissions::PermissionAction::DENIED
                              : permissions::PermissionAction::DISMISSED
#else
            permissions::PermissionAction::DENIED
#endif  // BUILDFLAG(IS_CHROMEOS)
        );
        return;
    }
  }

  switch (button) {
    case PermissionDialogButton::kAccept:
      delegate_->Accept();
      return;
    case PermissionDialogButton::kAcceptOnce:
      delegate_->AcceptThisTime();
      return;
    case PermissionDialogButton::kDeny:
#if BUILDFLAG(IS_CHROMEOS)
      is_deny_supported ? delegate_->Deny() : delegate_->Dismiss();
#else
      delegate_->Deny();
#endif  // BUILDFLAG(IS_CHROMEOS)
      return;
  }
  NOTREACHED();
}

std::u16string PermissionPromptBubbleBaseView::GetPermissionFragmentForTesting()
    const {
  std::u16string origin = GetUrlIdentityObject().name;
  return GetAccessibleWindowTitle().substr(
      GetAccessibleWindowTitle().find(origin) + origin.length());
}

// static
bool PermissionPromptBubbleBaseView::IsOneTimePermission(
    permissions::PermissionPrompt::Delegate& delegate) {
  CHECK_GT(delegate.Requests().size(), 0u);
  for (const auto& request : delegate.Requests()) {
    auto content_setting_type =
        permissions::RequestTypeToContentSettingsType(request->request_type());
    if (!content_setting_type.has_value() ||
        !permissions::PermissionUtil::DoesSupportTemporaryGrants(
            content_setting_type.value())) {
      return false;
    }
  }
  return true;
}

BEGIN_METADATA(PermissionPromptBubbleBaseView)
END_METADATA
