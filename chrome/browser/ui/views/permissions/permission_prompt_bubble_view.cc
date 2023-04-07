// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_view.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/browser/ui/views/title_origin_label.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr UrlIdentity::TypeSet allowed_types = {
    UrlIdentity::Type::kDefault, UrlIdentity::Type::kChromeExtension,
    UrlIdentity::Type::kIsolatedWebApp, UrlIdentity::Type::kFile};

constexpr UrlIdentity::FormatOptions options = {
    .default_options = {
        UrlIdentity::DefaultFormatOptions::kOmitCryptographicScheme}};

UrlIdentity GetUrlIdentity(Browser* browser,
                           permissions::PermissionPrompt::Delegate& delegate) {
  DCHECK(!delegate.Requests().empty());
  GURL origin_url = delegate.GetRequestingOrigin();

  UrlIdentity url_identity =
      UrlIdentity::CreateFromUrl(browser ? browser->profile() : nullptr,
                                 origin_url, allowed_types, options);

  if (url_identity.type == UrlIdentity::Type::kFile) {
    // File URLs will show the same constant.
    url_identity.name =
        l10n_util::GetStringUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT_THIS_FILE);
  }

  return url_identity;
}

// Determines whether the current request should also display an
// "Allow only this time" option in addition to the "Allow on every visit"
// option.
bool IsOneTimePermission(permissions::PermissionPrompt::Delegate& delegate) {
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

std::u16string GetWindowTitleInternal(
    const std::u16string display_name,
    const bool should_show_allow_this_time_button) {
  int message_id = should_show_allow_this_time_button
                       ? IDS_PERMISSIONS_BUBBLE_PROMPT_ONE_TIME
                       : IDS_PERMISSIONS_BUBBLE_PROMPT;

  return l10n_util::GetStringFUTF16(message_id, display_name);
}

std::u16string GetAccessibleWindowTitleInternal(
    const std::u16string display_name,
    std::vector<permissions::PermissionRequest*> visible_requests) {
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

  DCHECK(!visible_requests.empty());

  if (visible_requests.size() == 1) {
    return l10n_util::GetStringFUTF16(
        IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_ONE_PERM, display_name,
        visible_requests[0]->GetMessageTextFragment());
  }

  int template_id =
      visible_requests.size() == 2
          ? IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_TWO_PERMS
          : IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_TWO_PERMS_MORE;
  return l10n_util::GetStringFUTF16(
      template_id, display_name, visible_requests[0]->GetMessageTextFragment(),
      visible_requests[1]->GetMessageTextFragment());
}

bool ShouldShowRequest(permissions::PermissionPrompt::Delegate& delegate,
                       permissions::RequestType type) {
  if (type == permissions::RequestType::kCameraStream) {
    // Hide camera request if camera PTZ request is present as well.
    return !base::Contains(delegate.Requests(),
                           permissions::RequestType::kCameraPanTiltZoom,
                           &permissions::PermissionRequest::request_type);
  }
  return true;
}

std::vector<permissions::PermissionRequest*> GetVisibleRequests(
    permissions::PermissionPrompt::Delegate& delegate) {
  std::vector<permissions::PermissionRequest*> visible_requests;
  for (permissions::PermissionRequest* request : delegate.Requests()) {
    if (ShouldShowRequest(delegate, request->request_type())) {
      visible_requests.push_back(request);
    }
  }
  return visible_requests;
}

// Get extra information to display for the permission, if any.
absl::optional<std::u16string> GetExtraText(
    permissions::PermissionPrompt::Delegate& delegate) {
  switch (delegate.Requests()[0]->request_type()) {
    case permissions::RequestType::kStorageAccess:
      return l10n_util::GetStringFUTF16(
          IDS_STORAGE_ACCESS_PERMISSION_EXPLANATION,
          url_formatter::FormatUrlForSecurityDisplay(
              delegate.GetRequestingOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
          url_formatter::FormatUrlForSecurityDisplay(
              delegate.GetEmbeddingOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
    case permissions::RequestType::kU2fApiRequest:
      return l10n_util::GetStringUTF16(IDS_U2F_API_PERMISSION_EXPLANATION);
    default:
      return absl::nullopt;
  }
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PermissionPromptBubbleView,
                                      kPermissionPromptBubbleViewIdentifier);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PermissionPromptBubbleView,
                                      kAllowButtonElementId);

PermissionPromptBubbleView::PermissionPromptBubbleView(
    Browser* browser,
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
    base::TimeTicks permission_requested_time,
    PermissionPromptStyle prompt_style)
    : browser_(browser),
      delegate_(delegate),
      permission_requested_time_(permission_requested_time),
      is_one_time_permission_(IsOneTimePermission(*delegate.get())),
      url_identity_(GetUrlIdentity(browser, *delegate)),
      accessible_window_title_(GetAccessibleWindowTitleInternal(
          url_identity_.name,
          GetVisibleRequests(*delegate.get()))),
      window_title_(
          GetWindowTitleInternal(url_identity_.name, is_one_time_permission_)) {
  // Note that browser_ may be null in unit tests.

  // To prevent permissions being accepted accidentally, and as a security
  // measure against crbug.com/619429, permission prompts should not be accepted
  // as the default action.
  SetDefaultButton(ui::DIALOG_BUTTON_NONE);
  SetPromptStyle(prompt_style);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      DISTANCE_BUTTON_VERTICAL));

  set_close_on_deactivate(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  for (permissions::PermissionRequest* request :
       GetVisibleRequests(*delegate.get())) {
    AddRequestLine(request);
  }

  absl::optional<std::u16string> extra_text = GetExtraText(*delegate.get());
  if (extra_text.has_value()) {
    auto* extra_text_label =
        AddChildView(std::make_unique<views::Label>(extra_text.value()));
    extra_text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    extra_text_label->SetMultiLine(true);
  }

  if (is_one_time_permission_) {
    SetButtons(ui::DIALOG_BUTTON_NONE);

    auto buttons_container = std::make_unique<views::View>();
    auto* buttons_layout_manager =
        buttons_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical, gfx::Insets(),
            DISTANCE_BUTTON_VERTICAL));
    buttons_layout_manager->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStretch);
    auto allow_once_button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(
            &PermissionPromptBubbleView::AcceptPermissionThisTime,
            base::Unretained(this)),
        l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_ONCE));

    auto allow_always_button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(&PermissionPromptBubbleView::AcceptPermission,
                            base::Unretained(this)),
        l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_ALWAYS));
    allow_always_button->SetProperty(views::kElementIdentifierKey,
                                     kAllowButtonElementId);

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
    AddChildView(std::move(buttons_container));
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

  SetProperty(views::kElementIdentifierKey,
              kPermissionPromptBubbleViewIdentifier);
}

PermissionPromptBubbleView::~PermissionPromptBubbleView() = default;

void PermissionPromptBubbleView::Show() {
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

  // If a browser window (or popup) other than the bubble parent has focus,
  // don't take focus.
  if (browser_->window()->IsActive()) {
    widget->Show();
  } else {
    widget->ShowInactive();
  }

  SizeToContents();
}

void PermissionPromptBubbleView::AddRequestLine(
    permissions::PermissionRequest* request) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  auto* line_container = AddChildView(std::make_unique<views::View>());
  line_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, provider->GetDistanceMetric(
                             DISTANCE_SUBSECTION_HORIZONTAL_INDENT)),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL)));

  constexpr int kPermissionIconSize = 18;
  auto* icon = line_container->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          permissions::GetIconId(request->request_type()), ui::kColorIcon,
          kPermissionIconSize)));
  icon->SetVerticalAlignment(views::ImageView::Alignment::kLeading);

  auto* label = line_container->AddChildView(
      std::make_unique<views::Label>(request->GetMessageTextFragment()));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
}

void PermissionPromptBubbleView::UpdateAnchorPosition() {
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

void PermissionPromptBubbleView::SetPromptStyle(
    PermissionPromptStyle prompt_style) {
  prompt_style_ = prompt_style;
  // If bubble hanging off the padlock icon, with no chip showing, closing the
  // dialog should dismiss the pending request because there's no way to bring
  // the bubble back.
  if (prompt_style_ == PermissionPromptStyle::kBubbleOnly) {
    DialogDelegate::SetCloseCallback(
        base::BindOnce(&PermissionPromptBubbleView::ClosingPermission,
                       base::Unretained(this)));
  } else if (prompt_style_ == PermissionPromptStyle::kChip ||
             prompt_style_ == PermissionPromptStyle::kQuietChip) {
    // Override the `CloseCallback` if it was set previously.
    DialogDelegate::SetCloseCallback(base::DoNothing());
  }
}

void PermissionPromptBubbleView::AddedToWidget() {
  if (url_identity_.type == UrlIdentity::Type::kDefault) {
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
  return window_title_;
}

std::u16string PermissionPromptBubbleView::GetAccessibleWindowTitle() const {
  return accessible_window_title_;
}

void PermissionPromptBubbleView::AcceptPermission() {
  RecordDecision(permissions::PermissionAction::GRANTED);
  if (delegate_) {
    delegate_->Accept();
  }
}

void PermissionPromptBubbleView::AcceptPermissionThisTime() {
  RecordDecision(permissions::PermissionAction::GRANTED_ONCE);
  if (delegate_) {
    delegate_->AcceptThisTime();
  }
}

void PermissionPromptBubbleView::DenyPermission() {
  RecordDecision(permissions::PermissionAction::DENIED);
  if (delegate_) {
    delegate_->Deny();
  }
}

void PermissionPromptBubbleView::ClosingPermission() {
  DCHECK_EQ(prompt_style_, PermissionPromptStyle::kBubbleOnly);
  RecordDecision(permissions::PermissionAction::DISMISSED);
  if (delegate_) {
    delegate_->Dismiss();
  }
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

BEGIN_METADATA(PermissionPromptBubbleView, views::BubbleDialogDelegateView)
END_METADATA
