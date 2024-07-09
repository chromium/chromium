// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_one_origin_view.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
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
#include "components/media_effects/media_device_info.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/common/features.h"
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

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/views/media_preview/media_preview_feature.h"
#endif

namespace {

std::u16string GetAccessibleWindowTitleInternal(
    const std::u16string display_name,
    std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
        visible_requests) {
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

std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
GetVisibleRequests(permissions::PermissionPrompt::Delegate& delegate) {
  std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
      visible_requests;
  for (permissions::PermissionRequest* request : delegate.Requests()) {
    if (ShouldShowRequest(delegate, request->request_type())) {
      visible_requests.push_back(request);
    }
  }
  return visible_requests;
}

// Get extra information to display for the permission, if any.
std::optional<std::u16string> GetExtraText(
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
      return std::nullopt;
  }
}

}  // namespace

PermissionPromptBubbleOneOriginView::PermissionPromptBubbleOneOriginView(
    Browser* browser,
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
    base::TimeTicks permission_requested_time,
    PermissionPromptStyle prompt_style)
    : PermissionPromptBubbleBaseView(browser,
                                     delegate,
                                     permission_requested_time,
                                     prompt_style) {
  std::vector<std::string> requested_audio_capture_device_ids;
  std::vector<std::string> requested_video_capture_device_ids;
  std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
      visible_requests = GetVisibleRequests(*delegate.get());

  SetAccessibleTitle(GetAccessibleWindowTitleInternal(
      GetUrlIdentityObject().name, visible_requests));

  size_t title_offset;
  SetTitle(l10n_util::GetStringFUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT,
                                      GetUrlIdentityObject().name,
                                      &title_offset));
  // Calculate the range of $ORIGIN which should be bold. It will be used while
  // creating title label via `CreateTitleOriginLabel()`.
  SetTitleBoldedRanges(
      {{title_offset, title_offset + GetUrlIdentityObject().name.length()}});

  auto extra_text = GetExtraText(*delegate.get());
  if (extra_text.has_value()) {
    CreateExtraTextLabel(extra_text.value());
  }

  CreatePermissionButtons(GetAllowAlwaysText(visible_requests));

  for (std::size_t i = 0; i < visible_requests.size(); i++) {
    AddRequestLine(visible_requests[i], i);
    if (visible_requests[i]->request_type() ==
            permissions::RequestType::kCameraStream ||
        visible_requests[i]->request_type() ==
            permissions::RequestType::kCameraPanTiltZoom) {
      requested_video_capture_device_ids =
          visible_requests[i]->GetRequestedVideoCaptureDeviceIds();
    } else if (visible_requests[i]->request_type() ==
               permissions::RequestType::kMicStream) {
      requested_audio_capture_device_ids =
          visible_requests[i]->GetRequestedAudioCaptureDeviceIds();
    }
  }
  MaybeAddMediaPreview(requested_audio_capture_device_ids,
                       requested_video_capture_device_ids,
                       visible_requests.size());
}

PermissionPromptBubbleOneOriginView::~PermissionPromptBubbleOneOriginView() =
    default;

void PermissionPromptBubbleOneOriginView::RunButtonCallback(int button_id) {
#if !BUILDFLAG(IS_CHROMEOS)
  auto button = GetPermissionDialogButton(button_id);
  if (button == PermissionDialogButton::kAccept ||
      button == PermissionDialogButton::kAcceptOnce) {
    if (media_previews_.has_value()) {
      media_previews_->UpdateDevicePreferenceRanking();
    }
  }
#endif
  PermissionPromptBubbleBaseView::RunButtonCallback(button_id);
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

  constexpr int kPermissionIconSize = 20;
  auto* icon = line_container->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          permissions::GetIconId(request->request_type()), ui::kColorIcon,
          kPermissionIconSize)));
  icon->SetVerticalAlignment(views::ImageView::Alignment::kCenter);

  auto* label = line_container->AddChildView(
      std::make_unique<views::Label>(request->GetMessageTextFragment()));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);

#if !BUILDFLAG(IS_CHROMEOS)
  if (request->request_type() == permissions::RequestType::kMicStream) {
    mic_permission_label_ = label;
  } else if (request->request_type() ==
             permissions::RequestType::kCameraStream) {
    camera_permission_label_ = label;
  } else if (request->request_type() ==
             permissions::RequestType::kCameraPanTiltZoom) {
    ptz_camera_permission_label_ = label;
  }
#endif

  label->SetTextStyle(views::style::STYLE_BODY_3);
  label->SetEnabledColorId(kColorPermissionPromptRequestText);

  if (index == 0u) {
    constexpr int kPermissionBodyTopMargin = 10;
    line_container->SetProperty(
        views::kMarginsKey, gfx::Insets().set_top(kPermissionBodyTopMargin));
  }
}

void PermissionPromptBubbleOneOriginView::MaybeAddMediaPreview(
    std::vector<std::string> requested_audio_capture_device_ids,
    std::vector<std::string> requested_video_capture_device_ids,
    size_t index) {
#if !BUILDFLAG(IS_CHROMEOS)
  // Unit tests call this without initializing `browser_`, but this should not
  // happen in production code.
  if (!browser()) {
    return;
  }

  if (requested_audio_capture_device_ids.empty() &&
      requested_video_capture_device_ids.empty()) {
    return;
  }

  if (!camera_permission_label_ && !mic_permission_label_ &&
      !ptz_camera_permission_label_) {
    return;
  }

  // Check this last, as it queries the origin trials service.
  if (!media_preview_feature::ShouldShowMediaPreview(
          *browser()->profile(), delegate()->GetRequestingOrigin(),
          delegate()->GetEmbeddingOrigin(),
          media_preview_metrics::UiLocation::kPermissionPrompt)) {
    return;
  }

  auto* cached_device_info = media_effects::MediaDeviceInfo::GetInstance();
  devices_observer_.Observe(cached_device_info);
  if (camera_permission_label_ || ptz_camera_permission_label_) {
    // Initialize camera label with the current number of cached video devices.
    OnVideoDevicesChanged(cached_device_info->GetVideoDeviceInfos());
  }
  if (mic_permission_label_) {
    // Initialize mic label with the current number of cached audio devices.
    OnAudioDevicesChanged(cached_device_info->GetAudioDeviceInfos());
  }

  media_previews_.emplace(browser(), this, index,
                          requested_audio_capture_device_ids,
                          requested_video_capture_device_ids);
#endif
}

#if !BUILDFLAG(IS_CHROMEOS)
void PermissionPromptBubbleOneOriginView::OnAudioDevicesChanged(
    const std::optional<std::vector<media::AudioDeviceDescription>>&
        device_infos) {
  if (!mic_permission_label_ || !device_infos) {
    return;
  }

  const auto real_device_names =
      media_effects::GetRealAudioDeviceNames(device_infos.value());

  mic_permission_label_->SetText(l10n_util::GetStringFUTF16(
      IDS_MEDIA_CAPTURE_AUDIO_ONLY_PERMISSION_FRAGMENT_WITH_COUNT,
      base::NumberToString16(real_device_names.size())));

  mic_permission_label_->SetTooltipText(
      base::UTF8ToUTF16(base::JoinString(real_device_names, "\n")));
}

void PermissionPromptBubbleOneOriginView::OnVideoDevicesChanged(
    const std::optional<std::vector<media::VideoCaptureDeviceInfo>>&
        device_infos) {
  bool have_any_camera_label =
      ptz_camera_permission_label_ || camera_permission_label_;
  if (!have_any_camera_label || !device_infos) {
    return;
  }

  auto camera_label = camera_permission_label_;
  auto message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY_PERMISSION_FRAGMENT_WITH_COUNT;
  if (ptz_camera_permission_label_) {
    camera_label = ptz_camera_permission_label_;
    message_id =
        IDS_MEDIA_CAPTURE_CAMERA_PAN_TILT_ZOOM_PERMISSION_FRAGMENT_WITH_COUNT;
  }

  const auto real_device_names =
      media_effects::GetRealVideoDeviceNames(device_infos.value());
  camera_label->SetText(l10n_util::GetStringFUTF16(
      message_id, base::NumberToString16(real_device_names.size())));
  camera_label->SetTooltipText(
      base::UTF8ToUTF16(base::JoinString(real_device_names, "\n")));
}
#endif
