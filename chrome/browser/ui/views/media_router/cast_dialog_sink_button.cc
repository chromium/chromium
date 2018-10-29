// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_helper.h"
#include "chrome/common/media_router/issue.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/vector_icons.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/ui/media_router/internal/vector_icons/vector_icons.h"
#endif

namespace media_router {

namespace {

class StopButton : public views::LabelButton {
 public:
  StopButton(CastDialogSinkButton* owner,
             views::ButtonListener* button_listener,
             const UIMediaSink& sink,
             int button_tag,
             bool enabled)
      : views::LabelButton(button_listener, base::string16()), owner_(owner) {
    static const gfx::ImageSkia icon = CreateVectorIcon(
        kGenericStopIcon, kPrimaryIconSize, gfx::kGoogleBlue500);
    SetImage(views::Button::STATE_NORMAL, icon);
    SetInkDropMode(InkDropMode::ON);
    set_tag(button_tag);
    SetBorder(views::CreateEmptyBorder(gfx::Insets(kPrimaryIconBorderWidth)));
    SetEnabled(enabled);
    // Make it possible to navigate to this button by pressing the tab key.
    SetFocusBehavior(FocusBehavior::ALWAYS);
    // Remove the outlines drawn when the button is in focus.
    SetInstallFocusRingOnFocus(false);
    SetFocusPainter(nullptr);

    SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_MEDIA_ROUTER_STOP_CASTING_BUTTON_ACCESSIBLE_NAME,
        sink.friendly_name, sink.status_text));
  }

  ~StopButton() override = default;

  SkColor GetInkDropBaseColor() const override {
    return views::style::GetColor(*this, views::style::CONTEXT_BUTTON,
                                  STYLE_SECONDARY);
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return std::make_unique<views::InkDropHighlight>(
        size(), height() / 2,
        gfx::PointF(GetMirroredRect(GetLocalBounds()).CenterPoint()),
        GetInkDropBaseColor());
  }

  bool CanProcessEventsWithinSubtree() const override { return true; }

  // views::Button:
  void StateChanged(ButtonState old_state) override {
    if (state() == Button::STATE_HOVERED) {
      owner_->OverrideStatusText(
          l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_STOP_CASTING));
    } else if (old_state == Button::STATE_HOVERED) {
      owner_->RestoreStatusText();
    }
  }

 private:
  CastDialogSinkButton* const owner_;

  DISALLOW_COPY_AND_ASSIGN(StopButton);
};

gfx::ImageSkia CreateSinkIcon(SinkIconType icon_type, bool enabled = true) {
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
#if defined(GOOGLE_CHROME_BUILD)
    case SinkIconType::MEETING:
      vector_icon = &vector_icons::kMeetIcon;
      break;
    case SinkIconType::HANGOUT:
      vector_icon = &vector_icons::kHangoutIcon;
      break;
#endif  // defined(GOOGLE_CHROME_BUILD)
    case SinkIconType::CAST:
    case SinkIconType::GENERIC:
    default:
      vector_icon = &kTvIcon;
      break;
  }
  SkColor icon_color = enabled ? gfx::kChromeIconGrey : gfx::kGoogleGrey500;
  return gfx::CreateVectorIcon(*vector_icon, kPrimaryIconSize, icon_color);
}

gfx::ImageSkia CreateDisabledSinkIcon(SinkIconType icon_type) {
  return CreateSinkIcon(icon_type, false);
}

std::unique_ptr<views::View> CreatePrimaryIconForSink(
    CastDialogSinkButton* sink_button,
    views::ButtonListener* button_listener,
    const UIMediaSink& sink,
    int button_tag) {
  // The stop button has the highest priority, and the issue icon comes second.
  if (sink.state == UIMediaSinkState::CONNECTED ||
      sink.state == UIMediaSinkState::DISCONNECTING) {
    return std::make_unique<StopButton>(
        sink_button, button_listener, sink, button_tag,
        sink.state == UIMediaSinkState::CONNECTED);
  } else if (sink.issue) {
    auto icon_view = std::make_unique<views::ImageView>();
    icon_view->SetImage(CreateVectorIcon(::vector_icons::kInfoOutlineIcon,
                                         kPrimaryIconSize,
                                         gfx::kChromeIconGrey));
    icon_view->SetBorder(
        views::CreateEmptyBorder(gfx::Insets(kPrimaryIconBorderWidth)));
    return icon_view;
  } else if (sink.state == UIMediaSinkState::CONNECTING) {
    return CreateThrobber();
  }
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(CreateSinkIcon(sink.icon_type));
  icon_view->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kPrimaryIconBorderWidth)));
  return icon_view;
}

base::string16 GetStatusTextForSink(const UIMediaSink& sink) {
  if (sink.issue)
    return base::UTF8ToUTF16(sink.issue->info().title);
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
    const UIMediaSink& sink,
    int button_tag)
    : HoverButton(
          button_listener,
          CreatePrimaryIconForSink(this, button_listener, sink, button_tag),
          sink.friendly_name,
          GetStatusTextForSink(sink),
          /** secondary_icon_view */ nullptr),
      sink_(sink) {
  set_tag(button_tag);
  SetEnabled(sink.state == UIMediaSinkState::AVAILABLE);
}

CastDialogSinkButton::~CastDialogSinkButton() = default;

void CastDialogSinkButton::OverrideStatusText(
    const base::string16& status_text) {
  if (subtitle()) {
    if (!saved_status_text_)
      saved_status_text_ = subtitle()->text();
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
  HoverButton::OnEnabledChanged();
  SkColor background_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_ProminentButtonColor);
  if (enabled() || sink_.state == UIMediaSinkState::CONNECTED) {
    SetTitleTextStyle(views::style::STYLE_PRIMARY, background_color);
    if (sink_.state == UIMediaSinkState::AVAILABLE) {
      static_cast<views::ImageView*>(icon_view())
          ->SetImage(CreateSinkIcon(sink_.icon_type));
    }
  } else {
    SetTitleTextStyle(views::style::STYLE_DISABLED, background_color);
    if (sink_.state == UIMediaSinkState::AVAILABLE) {
      static_cast<views::ImageView*>(icon_view())
          ->SetImage(CreateDisabledSinkIcon(sink_.icon_type));
    }
  }
  // Apply the style change to the title text.
  title()->Layout();
}

void CastDialogSinkButton::RequestFocus() {
  if (enabled()) {
    HoverButton::RequestFocus();
  } else {
    // The sink button is disabled, but the icon within it may be enabled and
    // want focus.
    icon_view()->RequestFocus();
  }
}

}  // namespace media_router
