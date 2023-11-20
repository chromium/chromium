// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_one_origin_view.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/ui_features.h"
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
#include "ui/base/ui_base_features.h"
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
    default:
      return absl::nullopt;
  }
}

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_FUCHSIA)
std::optional<MediaCoordinator::ViewType> ComputePreviewType(
    bool has_camera_request,
    bool has_mic_request) {
  if (has_camera_request && has_mic_request) {
    return MediaCoordinator::ViewType::kBoth;
  }
  if (has_camera_request) {
    return MediaCoordinator::ViewType::kCameraOnly;
  }
  if (has_mic_request) {
    return MediaCoordinator::ViewType::kMicOnly;
  }
  return std::nullopt;
}
#endif

}  // namespace

PermissionPromptBubbleOneOriginView::PermissionPromptBubbleOneOriginView(
    Browser* browser,
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
    base::TimeTicks permission_requested_time,
    PermissionPromptStyle prompt_style)
    : PermissionPromptBubbleBaseView(
          browser,
          delegate,
          permission_requested_time,
          prompt_style,
          l10n_util::GetStringFUTF16(
              IDS_PERMISSIONS_BUBBLE_PROMPT,
              PermissionPromptBaseView::GetUrlIdentity(browser, *delegate)
                  .name),
          GetAccessibleWindowTitleInternal(
              PermissionPromptBaseView::GetUrlIdentity(browser, *delegate).name,
              GetVisibleRequests(*delegate.get())),
          GetExtraText(*delegate.get())) {
  bool has_camera_request = false;
  bool has_mic_request = false;
  std::vector<permissions::PermissionRequest*> visible_requests =
      GetVisibleRequests(*delegate.get());
  for (std::size_t i = 0; i < visible_requests.size(); i++) {
    AddRequestLine(visible_requests[i], i);
    if (visible_requests[i]->request_type() ==
        permissions::RequestType::kCameraStream) {
      has_camera_request = true;
    } else if (visible_requests[i]->request_type() ==
               permissions::RequestType::kMicStream) {
      has_mic_request = true;
    }
  }
  MaybeAddMediaPreview(has_camera_request, has_mic_request,
                       visible_requests.size());
}

PermissionPromptBubbleOneOriginView::~PermissionPromptBubbleOneOriginView() =
    default;

void PermissionPromptBubbleOneOriginView::ChildPreferredSizeChanged(
    views::View* child) {
  if (GetBubbleFrameView()) {
    SizeToContents();
  }
}

void PermissionPromptBubbleOneOriginView::AddRequestLine(
    permissions::PermissionRequest* request,
    std::size_t index) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  auto* line_container = AddChildViewAt(std::make_unique<views::View>(), index);
  line_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, provider->GetDistanceMetric(
                             DISTANCE_SUBSECTION_HORIZONTAL_INDENT)),
      provider->GetDistanceMetric(
          DISTANCE_PERMISSION_PROMPT_HORIZONTAL_ICON_LABEL_PADDING)));

  const int kPermissionIconSize = features::IsChromeRefresh2023() ? 20 : 18;
  auto* icon = line_container->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          permissions::GetIconId(request->request_type()), ui::kColorIcon,
          kPermissionIconSize)));
  icon->SetVerticalAlignment(views::ImageView::Alignment::kCenter);

  auto* label = line_container->AddChildView(
      std::make_unique<views::Label>(request->GetMessageTextFragment()));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);

  if (features::IsChromeRefresh2023()) {
    label->SetTextStyle(views::style::STYLE_BODY_3);
    label->SetEnabledColorId(kColorPermissionPromptRequestText);

    constexpr int kPermissionBodyTopMargin = 10;
    line_container->SetProperty(
        views::kMarginsKey, gfx::Insets().set_top(kPermissionBodyTopMargin));
  }
}

void PermissionPromptBubbleOneOriginView::MaybeAddMediaPreview(
    bool has_camera_request,
    bool has_mic_request,
    size_t index) {
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_FUCHSIA)
  if (!base::FeatureList::IsEnabled(features::kCameraMicPreview)) {
    return;
  }

  auto view_type = ComputePreviewType(has_camera_request, has_mic_request);
  if (!view_type) {
    return;
  }

  media_preview_coordinator_.emplace(view_type.value(), *this, index,
                                     /*is_subsection=*/false);
#endif
}
