// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/title_origin_label.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/color_tracking_icon_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/widget/widget.h"

PermissionPromptBubbleView::PermissionPromptBubbleView(
    Browser* browser,
    permissions::PermissionPrompt::Delegate* delegate,
    base::TimeTicks permission_requested_time,
    PermissionPromptStyle prompt_style)
    : browser_(browser),
      delegate_(delegate),
      permission_requested_time_(permission_requested_time) {
  // Note that browser_ may be null in unit tests.
  DCHECK(delegate_);

  // To prevent permissions being accepted accidentally, and as a security
  // measure against crbug.com/619429, permission prompts should not be accepted
  // as the default action.
  SetDefaultButton(ui::DIALOG_BUTTON_NONE);

  if (GetShowAllowThisTimeButton()) {
    // Host every button in the extra_view to have full control over the width
    // of the dialog.
    SetButtons(ui::DIALOG_BUTTON_NONE);

    views::LayoutProvider* const layout_provider = views::LayoutProvider::Get();
    const int button_spacing = layout_provider->GetDistanceMetric(
        views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
    auto buttons_container = std::make_unique<views::View>();
    buttons_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        button_spacing));

    auto allow_once_button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(
            &PermissionPromptBubbleView::AcceptPermissionThisTime,
            base::Unretained(this)),
        l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_ONCE));

    auto allow_always_button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(&PermissionPromptBubbleView::AcceptPermission,
                            base::Unretained(this)),
        l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_ALWAYS));

    auto block_button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(&PermissionPromptBubbleView::DenyPermission,
                            base::Unretained(this)),
        l10n_util::GetStringUTF16(IDS_PERMISSION_DENY));

    if (permissions::feature_params::kOkButtonBehavesAsAllowAlways.Get()) {
      buttons_container->AddChildView(std::move(allow_once_button));
      if (views::PlatformStyle::kIsOkButtonLeading) {
        buttons_container->AddChildView(std::move(allow_always_button));
        buttons_container->AddChildView(std::move(block_button));
      } else {
        buttons_container->AddChildView(std::move(block_button));
        buttons_container->AddChildView(std::move(allow_always_button));
      }
    } else {
      buttons_container->AddChildView(std::move(allow_always_button));
      if (views::PlatformStyle::kIsOkButtonLeading) {
        buttons_container->AddChildView(std::move(allow_once_button));
        buttons_container->AddChildView(std::move(block_button));
      } else {
        buttons_container->AddChildView(std::move(block_button));
        buttons_container->AddChildView(std::move(allow_once_button));
      }
    }
    SetExtraView(std::move(buttons_container));
  } else {
    SetButtonLabel(ui::DIALOG_BUTTON_OK,
                   l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));
    SetAcceptCallback(base::BindOnce(
        &PermissionPromptBubbleView::AcceptPermission, base::Unretained(this)));

    SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                   l10n_util::GetStringUTF16(IDS_PERMISSION_DENY));
    SetCancelCallback(base::BindOnce(
        &PermissionPromptBubbleView::DenyPermission, base::Unretained(this)));
  }

  SetPromptStyle(prompt_style);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  for (permissions::PermissionRequest* request : GetVisibleRequests())
    AddRequestLine(request);

  base::Optional<std::u16string> extra_text = GetExtraText();
  if (extra_text.has_value()) {
    auto* extra_text_label =
        AddChildView(std::make_unique<views::Label>(extra_text.value()));
    extra_text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    extra_text_label->SetMultiLine(true);
  }
}

PermissionPromptBubbleView::~PermissionPromptBubbleView() = default;

void PermissionPromptBubbleView::Show() {
  DCHECK(browser_->window());

  // Set |parent_window| because some valid anchors can become hidden.
  set_parent_window(
      platform_util::GetViewForWindow(browser_->window()->GetNativeWindow()));

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);
  // If a browser window (or popup) other than the bubble parent has focus,
  // don't take focus.
  if (browser_->window()->IsActive())
    widget->Show();
  else
    widget->ShowInactive();

  SizeToContents();

  DCHECK(browser_->window());
  set_parent_window(
      platform_util::GetViewForWindow(browser_->window()->GetNativeWindow()));

  UpdateAnchorPosition();
  chrome::RecordDialogCreation(chrome::DialogIdentifier::PERMISSIONS);
}

bool PermissionPromptBubbleView::ShouldShowRequest(
    permissions::RequestType type) const {
  if (type == permissions::RequestType::kCameraStream) {
    // Hide camera request if camera PTZ request is present as well.
    auto requests = delegate_->Requests();
    return std::find_if(requests.begin(), requests.end(), [](auto* request) {
             return request->GetRequestType() ==
                    permissions::RequestType::kCameraPanTiltZoom;
           }) == requests.end();
  }
  return true;
}

std::vector<permissions::PermissionRequest*>
PermissionPromptBubbleView::GetVisibleRequests() const {
  std::vector<permissions::PermissionRequest*> visible_requests;
  for (permissions::PermissionRequest* request : delegate_->Requests()) {
    if (ShouldShowRequest(request->GetRequestType()))
      visible_requests.push_back(request);
  }
  return visible_requests;
}

void PermissionPromptBubbleView::AddRequestLine(
    permissions::PermissionRequest* request) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  auto* line_container = AddChildView(std::make_unique<views::View>());
  line_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, provider->GetDistanceMetric(
                         DISTANCE_SUBSECTION_HORIZONTAL_INDENT)),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL)));

  constexpr int kPermissionIconSize = 18;
  auto* icon = line_container->AddChildView(
      std::make_unique<views::ColorTrackingIconView>(
          permissions::GetIconId(request->GetRequestType()),
          kPermissionIconSize));
  icon->SetVerticalAlignment(views::ImageView::Alignment::kLeading);

  auto* label = line_container->AddChildView(
      std::make_unique<views::Label>(request->GetMessageTextFragment()));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
}

void PermissionPromptBubbleView::UpdateAnchorPosition() {
  DCHECK_EQ(browser_->window()->GetNativeWindow(), parent_window());

  bubble_anchor_util::AnchorConfiguration configuration =
      bubble_anchor_util::GetPermissionPromptBubbleAnchorConfiguration(
          browser_);
  SetAnchorView(configuration.anchor_view);
  SetHighlightedButton(configuration.highlighted_button);
  if (!configuration.anchor_view)
    SetAnchorRect(bubble_anchor_util::GetPageInfoAnchorRect(browser_));
  SetArrow(configuration.bubble_arrow);
}

void PermissionPromptBubbleView::SetPromptStyle(
    PermissionPromptStyle prompt_style) {
  prompt_style_ = prompt_style;
  // If bubble hanging off the padlock icon, with no chip showing, it shouldn't
  // close on deactivate and it should stick until user makes a decision.
  // Otherwise, the chip is indicating the pending permission request and so the
  // bubble can be opened and closed repeatedly.
  if (prompt_style == PermissionPromptStyle::kBubbleOnly) {
    set_close_on_deactivate(false);
    DialogDelegate::SetCloseCallback(
        base::BindOnce(&PermissionPromptBubbleView::ClosingPermission,
                       base::Unretained(this)));
  } else {
    set_close_on_deactivate(true);
    DialogDelegate::SetCloseCallback(base::OnceClosure());
  }
}

void PermissionPromptBubbleView::AddedToWidget() {
  if (GetDisplayNameIsOrigin()) {
    // There is a risk of URL spoofing from origins that are too wide to fit in
    // the bubble; elide origins from the front to prevent this.
    GetBubbleFrameView()->SetTitleView(
        CreateTitleOriginLabel(GetWindowTitle()));
  }
}

bool PermissionPromptBubbleView::ShouldShowCloseButton() const {
  return true;
}

std::u16string PermissionPromptBubbleView::GetWindowTitle() const {
  int message_id;
  if (GetShowAllowThisTimeButton()) {
    message_id = IDS_PERMISSIONS_BUBBLE_PROMPT_ONE_TIME;
  } else {
    message_id = IDS_PERMISSIONS_BUBBLE_PROMPT;
  }
  return l10n_util::GetStringFUTF16(message_id, GetDisplayName());
}

std::u16string PermissionPromptBubbleView::GetAccessibleWindowTitle() const {
  // Generate one of:
  //   $origin wants to: $permission
  //   $origin wants to: $permission and $permission
  //   $origin wants to: $permission, $permission, and more
  // where $permission is the permission's text fragment, a verb phrase
  // describing what the permission is, like:
  //   "Download multiple files"
  //   "Use your camera"
  //
  // There are three separate internationalized messages used, one for each
  // format of title, to provide for accurate i18n. See https://crbug.com/434574
  // for more details.
  auto visible_requests = GetVisibleRequests();
  DCHECK(!visible_requests.empty());

  if (visible_requests.size() == 1) {
    return l10n_util::GetStringFUTF16(
        IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_ONE_PERM,
        GetDisplayName(), visible_requests[0]->GetMessageTextFragment());
  }

  int template_id =
      visible_requests.size() == 2
          ? IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_TWO_PERMS
          : IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_TWO_PERMS_MORE;
  return l10n_util::GetStringFUTF16(
      template_id, GetDisplayName(),
      visible_requests[0]->GetMessageTextFragment(),
      visible_requests[1]->GetMessageTextFragment());
}

std::u16string PermissionPromptBubbleView::GetDisplayName() const {
  DCHECK(!delegate_->Requests().empty());
  GURL origin_url = delegate_->GetRequestingOrigin();

  if (origin_url.SchemeIs(extensions::kExtensionScheme)) {
    std::u16string extension_name =
        extensions::ui_util::GetEnabledExtensionNameForUrl(origin_url,
                                                           browser_->profile());
    if (!extension_name.empty())
      return extension_name;
  }

  // File URLs should be displayed as "This file".
  if (origin_url.SchemeIsFile())
    return l10n_util::GetStringUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT_THIS_FILE);

  // Web URLs should be displayed as the origin in the URL.
  return url_formatter::FormatUrlForSecurityDisplay(
      origin_url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
}

bool PermissionPromptBubbleView::GetDisplayNameIsOrigin() const {
  DCHECK(!delegate_->Requests().empty());
  GURL origin_url = delegate_->GetRequestingOrigin();
  return (!origin_url.SchemeIs(extensions::kExtensionScheme) ||
          extensions::ui_util::GetEnabledExtensionNameForUrl(
              origin_url, browser_->profile())
              .empty()) &&
         !origin_url.SchemeIsFile();
}

base::Optional<std::u16string> PermissionPromptBubbleView::GetExtraText()
    const {
  switch (delegate_->Requests()[0]->GetRequestType()) {
    case permissions::RequestType::kStorageAccess:
      return l10n_util::GetStringFUTF16(
          IDS_STORAGE_ACCESS_PERMISSION_EXPLANATION,
          url_formatter::FormatUrlForSecurityDisplay(
              delegate_->GetRequestingOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
          url_formatter::FormatUrlForSecurityDisplay(
              delegate_->GetEmbeddingOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
    default:
      return base::nullopt;
  }
}

void PermissionPromptBubbleView::AcceptPermission() {
  RecordDecision(permissions::PermissionAction::GRANTED);
  delegate_->Accept();
}

void PermissionPromptBubbleView::AcceptPermissionThisTime() {
  RecordDecision(permissions::PermissionAction::GRANTED_ONCE);
  delegate_->AcceptThisTime();
}

void PermissionPromptBubbleView::DenyPermission() {
  RecordDecision(permissions::PermissionAction::DENIED);
  delegate_->Deny();
}

void PermissionPromptBubbleView::ClosingPermission() {
  DCHECK_EQ(prompt_style_, PermissionPromptStyle::kBubbleOnly);
  RecordDecision(permissions::PermissionAction::DISMISSED);
  delegate_->Closing();
}

void PermissionPromptBubbleView::RecordDecision(
    permissions::PermissionAction action) {
  const std::string uma_suffix =
      permissions::PermissionUmaUtil::GetPermissionActionString(action);
  std::string time_to_decision_uma_name =
      prompt_style_ == PermissionPromptStyle::kBubbleOnly
          ? "Permissions.Prompt.TimeToDecision"
          : "Permissions.Chip.TimeToDecision";
  base::UmaHistogramLongTimes(
      time_to_decision_uma_name + "." + uma_suffix,
      base::TimeTicks::Now() - permission_requested_time_);
}

bool PermissionPromptBubbleView::GetShowAllowThisTimeButton() const {
  if (!base::FeatureList::IsEnabled(
          permissions::features::kOneTimeGeolocationPermission)) {
    return false;
  }
  if (delegate_->Requests().size() > 1)
    return false;
  CHECK_GT(delegate_->Requests().size(), 0u);
  return delegate_->Requests()[0]->GetRequestType() ==
         permissions::RequestType::kGeolocation;
}

BEGIN_METADATA(PermissionPromptBubbleView, views::BubbleDialogDelegateView)
ADD_READONLY_PROPERTY_METADATA(std::u16string, DisplayName)
ADD_READONLY_PROPERTY_METADATA(bool, DisplayNameIsOrigin)
END_METADATA
