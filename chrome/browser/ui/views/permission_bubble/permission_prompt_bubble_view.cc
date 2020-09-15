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
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/title_origin_label.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/permission_request.h"
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
#include "ui/views/controls/color_tracking_icon_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

PermissionPromptBubbleView::PermissionPromptBubbleView(
    Browser* browser,
    permissions::PermissionPrompt::Delegate* delegate,
    base::TimeTicks permission_requested_time)
    : browser_(browser),
      delegate_(delegate),
      visible_requests_(GetVisibleRequests()),
      name_or_origin_(GetDisplayNameOrOrigin()),
      permission_requested_time_(permission_requested_time) {
  // Note that browser_ may be null in unit tests.
  DCHECK(delegate_);

  // To prevent permissions being accepted accidentally, and as a security
  // measure against crbug.com/619429, permission prompts should not be accepted
  // as the default action.
  SetDefaultButton(ui::DIALOG_BUTTON_NONE);

  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(IDS_PERMISSION_DENY));

  SetAcceptCallback(base::BindOnce(
      &PermissionPromptBubbleView::AcceptPermission, base::Unretained(this)));
  SetCancelCallback(base::BindOnce(&PermissionPromptBubbleView::DenyPermission,
                                   base::Unretained(this)));

  // If the permission chip feature is enabled, the chip is indicating the
  // pending permission request and so the bubble can be opened and closed
  // repeatedly.
  if (!base::FeatureList::IsEnabled(features::kPermissionChip)) {
    set_close_on_deactivate(false);
    DialogDelegate::SetCloseCallback(
        base::BindOnce(&PermissionPromptBubbleView::ClosingPermission,
                       base::Unretained(this)));
  }

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  for (permissions::PermissionRequest* request : visible_requests_)
    AddPermissionRequestLine(request);

  base::Optional<base::string16> extra_text = GetExtraText();
  if (extra_text.has_value()) {
    auto* extra_text_label =
        AddChildView(std::make_unique<views::Label>(extra_text.value()));
    extra_text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    extra_text_label->SetMultiLine(true);
  }

  if (visible_requests_[0]->GetContentSettingsType() ==
      ContentSettingsType::PLUGINS) {
    auto* learn_more_button =
        SetExtraView(views::CreateVectorImageButtonWithNativeTheme(
            this, vector_icons::kHelpOutlineIcon));
    learn_more_button->SetFocusForPlatform();
    learn_more_button->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_LEARN_MORE));
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
  UpdateAnchorPosition();
  chrome::RecordDialogCreation(chrome::DialogIdentifier::PERMISSIONS);
}

std::vector<permissions::PermissionRequest*>
PermissionPromptBubbleView::GetVisibleRequests() {
  std::vector<permissions::PermissionRequest*> visible_requests;

  for (permissions::PermissionRequest* request : delegate_->Requests()) {
    if (ShouldShowPermissionRequest(request))
      visible_requests.push_back(request);
  }
  return visible_requests;
}

bool PermissionPromptBubbleView::ShouldShowPermissionRequest(
    permissions::PermissionRequest* request) {
  if (request->GetContentSettingsType() !=
      ContentSettingsType::MEDIASTREAM_CAMERA) {
    return true;
  }

  // Hide camera request only if camera PTZ request is present as well.
  for (permissions::PermissionRequest* request : delegate_->Requests()) {
    if (request->GetContentSettingsType() ==
        ContentSettingsType::CAMERA_PAN_TILT_ZOOM) {
      return false;
    }
  }

  return true;
}

void PermissionPromptBubbleView::AddPermissionRequestLine(
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
      std::make_unique<views::ColorTrackingIconView>(request->GetIconId(),
                                                     kPermissionIconSize));
  icon->SetVerticalAlignment(views::ImageView::Alignment::kLeading);

  auto* label = line_container->AddChildView(
      std::make_unique<views::Label>(request->GetMessageTextFragment()));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
}

void PermissionPromptBubbleView::UpdateAnchorPosition() {
  DCHECK(browser_->window());

  set_parent_window(
      platform_util::GetViewForWindow(browser_->window()->GetNativeWindow()));

  bubble_anchor_util::AnchorConfiguration configuration =
      bubble_anchor_util::GetPermissionPromptBubbleAnchorConfiguration(
          browser_);
  SetAnchorView(configuration.anchor_view);
  SetHighlightedButton(configuration.highlighted_button);
  if (!configuration.anchor_view)
    SetAnchorRect(bubble_anchor_util::GetPageInfoAnchorRect(browser_));
  SetArrow(configuration.bubble_arrow);
}

void PermissionPromptBubbleView::AddedToWidget() {
  if (name_or_origin_.is_origin) {
    // There is a risk of URL spoofing from origins that are too wide to fit in
    // the bubble; elide origins from the front to prevent this.
    GetBubbleFrameView()->SetTitleView(
        CreateTitleOriginLabel(GetWindowTitle()));
  }
}

bool PermissionPromptBubbleView::ShouldShowCloseButton() const {
  return true;
}

base::string16 PermissionPromptBubbleView::GetWindowTitle() const {
  return l10n_util::GetStringFUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT,
                                    name_or_origin_.name_or_origin);
}

base::string16 PermissionPromptBubbleView::GetAccessibleWindowTitle() const {
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
  DCHECK(!visible_requests_.empty());

  if (visible_requests_.size() == 1) {
    return l10n_util::GetStringFUTF16(
        IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_ONE_PERM,
        name_or_origin_.name_or_origin,
        visible_requests_[0]->GetMessageTextFragment());
  }

  int template_id =
      visible_requests_.size() == 2
          ? IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_TWO_PERMS
          : IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_TWO_PERMS_MORE;
  return l10n_util::GetStringFUTF16(
      template_id, name_or_origin_.name_or_origin,
      visible_requests_[0]->GetMessageTextFragment(),
      visible_requests_[1]->GetMessageTextFragment());
}

gfx::Size PermissionPromptBubbleView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void PermissionPromptBubbleView::ButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  DCHECK_EQ(sender, GetExtraView());
  chrome::AddSelectedTabWithURL(browser_,
                                GURL(chrome::kFlashDeprecationLearnMoreURL),
                                ui::PAGE_TRANSITION_LINK);
}

PermissionPromptBubbleView::DisplayNameOrOrigin
PermissionPromptBubbleView::GetDisplayNameOrOrigin() const {
  DCHECK(!visible_requests_.empty());
  GURL origin_url = visible_requests_[0]->GetOrigin();

  if (origin_url.SchemeIs(extensions::kExtensionScheme)) {
    base::string16 extension_name =
        extensions::ui_util::GetEnabledExtensionNameForUrl(origin_url,
                                                           browser_->profile());
    if (!extension_name.empty())
      return {extension_name, false /* is_origin */};
  }

  // File URLs should be displayed as "This file".
  if (origin_url.SchemeIsFile()) {
    return {l10n_util::GetStringUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT_THIS_FILE),
            false /* is_origin */};
  }

  // Web URLs should be displayed as the origin in the URL.
  return {url_formatter::FormatUrlForSecurityDisplay(
              origin_url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
          true /* is_origin */};
}

base::Optional<base::string16> PermissionPromptBubbleView::GetExtraText()
    const {
  switch (visible_requests_[0]->GetContentSettingsType()) {
    case ContentSettingsType::PLUGINS:
      // TODO(crbug.com/1058401): Remove this warning text once flash is
      // deprecated.
      return l10n_util::GetStringUTF16(IDS_FLASH_PERMISSION_WARNING_FRAGMENT);
    case ContentSettingsType::STORAGE_ACCESS:
      return l10n_util::GetStringFUTF16(
          IDS_STORAGE_ACCESS_PERMISSION_EXPLANATION,
          url_formatter::FormatUrlForSecurityDisplay(
              visible_requests_[0]->GetOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
          url_formatter::FormatUrlForSecurityDisplay(
              delegate_->GetEmbeddingOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
    default:
      return base::nullopt;
  }
}

void PermissionPromptBubbleView::AcceptPermission() {
  RecordDecision();
  delegate_->Accept();
}

void PermissionPromptBubbleView::DenyPermission() {
  RecordDecision();
  delegate_->Deny();
}

void PermissionPromptBubbleView::ClosingPermission() {
  RecordDecision();
  delegate_->Closing();
}

void PermissionPromptBubbleView::RecordDecision() {
  base::UmaHistogramLongTimes(
      "Permissions.Prompt.TimeToDecision",
      base::TimeTicks::Now() - permission_requested_time_);
}
