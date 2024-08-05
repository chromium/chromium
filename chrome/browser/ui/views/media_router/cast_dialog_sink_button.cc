// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"

#include <memory>
#include <utility>

#include "base/debug/stack_trace.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/common/issue.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"

namespace media_router {

namespace {

ui::ImageModel CreateSinkIcon(SinkIconType icon_type, bool enabled = true) {
  ui::ColorId icon_color = enabled ? ui::kColorIcon : ui::kColorIconDisabled;
  return ui::ImageModel::FromVectorIcon(
      *CastDialogSinkButton::GetVectorIcon(icon_type), icon_color,
      kPrimaryIconSize);
}

ui::ImageModel CreateDisabledSinkIcon(SinkIconType icon_type) {
  return CreateSinkIcon(icon_type, false);
}

std::unique_ptr<views::ImageView> CreatePrimaryIconView(
    const ui::ImageModel& image) {
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(image);
  icon_view->SetBorder(views::CreateEmptyBorder(kPrimaryIconBorder));
  return icon_view;
}

std::unique_ptr<views::View> CreatePrimaryIconForSink(const UIMediaSink& sink) {
  // The stop button has the highest priority, and the issue icon comes second.
  if (sink.state == UIMediaSinkState::CONNECTED) {
    return CreatePrimaryIconView(ui::ImageModel::FromVectorIcon(
        kGenericStopIcon, ui::kColorAccent, kPrimaryIconSize));
  } else if (sink.issue) {
    auto icon = std::make_unique<views::ImageView>(
        ui::ImageModel::FromVectorIcon(::vector_icons::kInfoOutlineIcon,
                                       ui::kColorIcon, kPrimaryIconSize));
    icon->SetBorder(views::CreateEmptyBorder(kPrimaryIconBorder));
    return icon;
  } else if (sink.state == UIMediaSinkState::CONNECTING ||
             sink.state == UIMediaSinkState::DISCONNECTING) {
    return CreateThrobber();
  }
  return CreatePrimaryIconView(CreateSinkIcon(sink.icon_type));
}

bool IsIncompatibleDialSink(const UIMediaSink& sink) {
  return sink.provider == mojom::MediaRouteProviderId::DIAL &&
         sink.cast_modes.empty();
}

}  // namespace

CastDialogSinkButton::CastDialogSinkButton(PressedCallback callback,
                                           const UIMediaSink& sink)
    : HoverButton(std::move(callback),
                  CreatePrimaryIconForSink(sink),
                  sink.friendly_name,
                  sink.GetStatusTextForDisplay(),
                  /** secondary_icon_view */ nullptr),
      sink_(sink) {
  SetEnabled(sink.state == UIMediaSinkState::AVAILABLE ||
             sink.state == UIMediaSinkState::CONNECTED);
}

CastDialogSinkButton::~CastDialogSinkButton() = default;

void CastDialogSinkButton::OverrideStatusText(
    const std::u16string& status_text) {
  if (!subtitle()) {
    return;
  }

  if (!saved_status_text_) {
    saved_status_text_ = subtitle()->GetText();
  }

  subtitle()->SetText(status_text);
}

void CastDialogSinkButton::RestoreStatusText() {
  if (!saved_status_text_) {
    return;
  }

  if (subtitle()) {
    subtitle()->SetText(*saved_status_text_);
  }
  saved_status_text_.reset();
}

bool CastDialogSinkButton::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsRightMouseButton())
    return true;
  return HoverButton::OnMousePressed(event);
}

void CastDialogSinkButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsRightMouseButton())
    return;
  HoverButton::OnMouseReleased(event);
}

void CastDialogSinkButton::OnEnabledChanged() {
  HoverButton::OnEnabledChanged();
  // Prevent a DCHECK failure seen at https://crbug.com/912687 by not having an
  // InkDrop if the button is disabled.
  views::InkDrop::Get(this)->SetMode(
      GetEnabled() ? views::InkDropHost::InkDropMode::ON
                   : views::InkDropHost::InkDropMode::OFF);
  // If the button has a state other than AVAILABLE (e.g. CONNECTED), there is
  // no need to change the status or the icon.
  if (sink_.state != UIMediaSinkState::AVAILABLE)
    return;

  ui::ImageModel icon;
  if (GetEnabled()) {
    if (saved_status_text_) {
      RestoreStatusText();
    }
    icon = CreateSinkIcon(sink_.icon_type);
  } else {
    int status_text = IsIncompatibleDialSink(sink_)
                          ? IDS_MEDIA_ROUTER_AVAILABLE_SPECIFIC_SITES
                          : IDS_MEDIA_ROUTER_SOURCE_NOT_SUPPORTED;
    OverrideStatusText(l10n_util::GetStringUTF16(status_text));
    icon = CreateDisabledSinkIcon(sink_.icon_type);
  }
  static_cast<views::ImageView*>(icon_view())->SetImage(icon);

  if (GetWidget())
    UpdateTitleTextStyle();
}

void CastDialogSinkButton::UpdateTitleTextStyle() {
  SkColor background_color =
      GetColorProvider()->GetColor(ui::kColorDialogBackground);
  SetTitleTextStyle(
      GetEnabled() ? views::style::STYLE_PRIMARY : views::style::STYLE_DISABLED,
      background_color, /*color_id=*/std::nullopt);
}

void CastDialogSinkButton::RequestFocus() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static bool requesting_focus = false;
  if (requesting_focus) {
    // TODO(crbug.com/1291739): Figure out why this happens.
    DLOG(ERROR) << "Recursive call to RequestFocus\n"
                << base::debug::StackTrace();
    return;
  }
  requesting_focus = true;
  if (GetEnabled()) {
    HoverButton::RequestFocus();
  } else {
    // The sink button is disabled, but the icon within it may be enabled and
    // want focus.
    icon_view()->RequestFocus();
  }
  requesting_focus = false;
}

void CastDialogSinkButton::OnFocus() {
  // Update the status text before calling |OnFocus()| so that the screen reader
  // can use the updated text.
  if (sink_.state == UIMediaSinkState::CONNECTED) {
    OverrideStatusText(
        l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_STOP_CASTING));
  }
  HoverButton::OnFocus();
}

void CastDialogSinkButton::OnBlur() {
  if (sink_.state == UIMediaSinkState::CONNECTED)
    RestoreStatusText();
}

void CastDialogSinkButton::OnThemeChanged() {
  HoverButton::OnThemeChanged();
  UpdateTitleTextStyle();
}

// static
const gfx::VectorIcon* CastDialogSinkButton::GetVectorIcon(
    SinkIconType icon_type) {
  const gfx::VectorIcon* vector_icon;
  switch (icon_type) {
    case SinkIconType::CAST_AUDIO_GROUP:
      vector_icon = &kSpeakerGroupIcon;
      break;
    case SinkIconType::CAST_AUDIO:
      vector_icon = &kSpeakerIcon;
      break;
    case SinkIconType::WIRED_DISPLAY:
      vector_icon = &kInputIcon;
      break;
    case SinkIconType::CAST:
    case SinkIconType::GENERIC:
    default:
      vector_icon = &kTvIcon;
      break;
  }
  return vector_icon;
}

// static
const gfx::VectorIcon* CastDialogSinkButton::GetVectorIcon(UIMediaSink sink) {
  return sink.issue ? &::vector_icons::kInfoOutlineIcon
                    : GetVectorIcon(sink.icon_type);
}

BEGIN_METADATA(CastDialogSinkButton)
END_METADATA

}  // namespace media_router
