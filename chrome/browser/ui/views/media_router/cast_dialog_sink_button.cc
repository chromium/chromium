// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"

#include <memory>

#include "base/debug/stack_trace.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/common/issue.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/color_tracking_icon_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/vector_icons.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/ui/media_router/internal/vector_icons/vector_icons.h"
#endif

namespace media_router {

namespace {

gfx::ImageSkia CreateSinkIcon(SinkIconType icon_type, bool enabled = true) {
  SkColor icon_color = enabled ? gfx::kChromeIconGrey : gfx::kGoogleGrey500;
  return gfx::CreateVectorIcon(*CastDialogSinkButton::GetVectorIcon(icon_type),
                               kPrimaryIconSize, icon_color);
}

gfx::ImageSkia CreateDisabledSinkIcon(SinkIconType icon_type) {
  return CreateSinkIcon(icon_type, false);
}

std::unique_ptr<views::ImageView> CreatePrimaryIconView(
    const gfx::ImageSkia& image) {
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(image);
  icon_view->SetBorder(views::CreateEmptyBorder(kPrimaryIconBorder));
  return icon_view;
}

std::unique_ptr<views::View> CreatePrimaryIconForSink(const UIMediaSink& sink) {
  // The stop button has the highest priority, and the issue icon comes second.
  if (sink.state == UIMediaSinkState::CONNECTED) {
    return CreatePrimaryIconView(gfx::CreateVectorIcon(
        kGenericStopIcon, kPrimaryIconSize, gfx::kGoogleBlue500));
  } else if (sink.issue) {
    auto icon = std::make_unique<views::ColorTrackingIconView>(
        ::vector_icons::kInfoOutlineIcon, kPrimaryIconSize);
    icon->SetBorder(views::CreateEmptyBorder(kPrimaryIconBorder));
    return icon;
  } else if (sink.state == UIMediaSinkState::CONNECTING ||
             sink.state == UIMediaSinkState::DISCONNECTING) {
    return CreateThrobber();
  }
  return CreatePrimaryIconView(CreateSinkIcon(sink.icon_type));
}

base::string16 GetStatusTextForSink(const UIMediaSink& sink) {
  if (sink.issue)
    return base::UTF8ToUTF16(sink.issue->info().title);
  // If the sink is disconnecting, say so instead of using the source info
  // stored in |sink.status_text|.
  if (sink.state == UIMediaSinkState::DISCONNECTING)
    return l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_DISCONNECTING);
  if (!sink.status_text.empty())
    return sink.status_text;
  switch (sink.state) {
    case UIMediaSinkState::AVAILABLE:
      return l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_AVAILABLE);
    case UIMediaSinkState::CONNECTING:
      return l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_CONNECTING);
    default:
      return base::string16();
  }
}

}  // namespace

CastDialogSinkButton::CastDialogSinkButton(
    views::ButtonListener* button_listener,
    const UIMediaSink& sink)
    : HoverButton(button_listener,
                  CreatePrimaryIconForSink(sink),
                  sink.friendly_name,
                  GetStatusTextForSink(sink),
                  /** secondary_icon_view */ nullptr),
      sink_(sink) {
  SetEnabled(sink.state == UIMediaSinkState::AVAILABLE ||
             sink.state == UIMediaSinkState::CONNECTED);
}

CastDialogSinkButton::CastDialogSinkButton(
    views::ButtonListener* button_listener,
    const UIMediaSink& sink,
    int button_tag)
    : CastDialogSinkButton(button_listener, sink) {
  set_tag(button_tag);
}

CastDialogSinkButton::~CastDialogSinkButton() = default;

void CastDialogSinkButton::OverrideStatusText(
    const base::string16& status_text) {
  if (subtitle()) {
    if (!saved_status_text_)
      saved_status_text_ = subtitle()->GetText();
    subtitle()->SetText(status_text);
  }
}

void CastDialogSinkButton::RestoreStatusText() {
  if (saved_status_text_) {
    if (subtitle())
      subtitle()->SetText(*saved_status_text_);
    saved_status_text_.reset();
  }
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
  // Prevent a DCHECK failure seen at https://crbug.com/912687 by not having an
  // InkDrop if the button is disabled.
  SetInkDropMode(GetEnabled() ? InkDropMode::ON : InkDropMode::OFF);
  // If the button has a state other than AVAILABLE (e.g. CONNECTED), there is
  // no need to change the status or the icon.
  if (sink_.state != UIMediaSinkState::AVAILABLE)
    return;

  SkColor background_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground);
  if (GetEnabled()) {
    SetTitleTextStyle(views::style::STYLE_PRIMARY, background_color);
    if (saved_status_text_)
      RestoreStatusText();
    static_cast<views::ImageView*>(icon_view())
        ->SetImage(CreateSinkIcon(sink_.icon_type));
  } else {
    SetTitleTextStyle(views::style::STYLE_DISABLED, background_color);
    OverrideStatusText(
        l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SOURCE_NOT_SUPPORTED));
    static_cast<views::ImageView*>(icon_view())
        ->SetImage(CreateDisabledSinkIcon(sink_.icon_type));
  }
  // Apply the style change to the title text.
  title()->Layout();
}

void CastDialogSinkButton::RequestFocus() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static bool requesting_focus = false;
  if (requesting_focus) {
    // TODO(jrw): Figure out why this happens.
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
  HoverButton::OnFocus();
  if (sink_.state == UIMediaSinkState::CONNECTED) {
    OverrideStatusText(
        l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_STOP_CASTING));
  }
}

void CastDialogSinkButton::OnBlur() {
  if (sink_.state == UIMediaSinkState::CONNECTED)
    RestoreStatusText();
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
    case SinkIconType::EDUCATION:
      vector_icon = &kCastForEducationIcon;
      break;
    case SinkIconType::WIRED_DISPLAY:
      vector_icon = &kInputIcon;
      break;
// Use proprietary icons only in Chrome builds. The default TV icon is used
// instead for these sink types in Chromium builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case SinkIconType::MEETING:
      vector_icon = &vector_icons::kMeetIcon;
      break;
    case SinkIconType::HANGOUT:
      vector_icon = &vector_icons::kHangoutIcon;
      break;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case SinkIconType::CAST:
    case SinkIconType::GENERIC:
    default:
      vector_icon = &kTvIcon;
      break;
  }
  return vector_icon;
}

}  // namespace media_router
