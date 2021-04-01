// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"

#include <memory>
#include <utility>

#include "base/debug/stack_trace.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
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
#include "components/strings/grit/components_strings.h"
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
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/vector_icons.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/ui/media_router/internal/vector_icons/vector_icons.h"
#endif

namespace media_router {

namespace {

bool CastToMeetingEnabled() {
  return base::FeatureList::IsEnabled(kCastToMeetingFromCastDialog);
}

bool IsMeetingIconType(SinkIconType icon_type) {
  switch (icon_type) {
    case SinkIconType::MEETING:
    case SinkIconType::HANGOUT:
      return true;
    default:
      return false;
  }
}

bool IsEnabledIconType(SinkIconType icon_type) {
  return CastToMeetingEnabled() || !IsMeetingIconType(icon_type);
}

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

bool IsIncompatibleDialSink(const UIMediaSink& sink) {
  return sink.provider == MediaRouteProviderId::DIAL && sink.cast_modes.empty();
}

std::u16string GetStatusTextForSink(const UIMediaSink& sink) {
  if (!IsEnabledIconType(sink.icon_type))
    return std::u16string();

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
      return std::u16string();
  }
}

// Find the index of a tab whose URL matches the given origin and path.  In the
// case of multiple matches, the active tab is given priority, otherwise returns
// the index of an arbitrary tab.
base::Optional<int> FindTabWithUrlPrefix(TabStripModel* tabs,
                                         const GURL& origin,
                                         base::StringPiece path) {
  base::Optional<int> to_activate;
  for (int i = 0; i < tabs->count(); i++) {
    auto* content = tabs->GetWebContentsAt(i);
    const GURL& url = content->GetVisibleURL();
    if (url.GetOrigin() == origin && url.path() == path) {
      to_activate = i;
      if (tabs->active_index() <= i)
        break;
    }
  }
  return to_activate;
}

// Selects or creates a tab for a meeting ID.  Tries to select an existing tab
// in the current window or some other window, and if no tab is found, opens a
// new tab for the meeting.
//
// If there is no meeting ID, this function just selects or creates a tab
// showing the start page of Google Meet.
void ShowMeetTab(Profile* profile,
                 const base::Optional<std::string>& meeting_id) {
  const GURL origin("https://meet.google.com");
  const std::string path = meeting_id ? "/" + *meeting_id : std::string();

  const auto& browsers = *BrowserList::GetInstance();
  for (auto iter = browsers.begin_last_active();
       iter != browsers.end_last_active(); ++iter) {
    Browser* browser = *iter;
    if (browser->profile() == profile && browser->window() &&
        browser->is_type_normal()) {
      auto* tabs = browser->tab_strip_model();
      auto tab_index = FindTabWithUrlPrefix(tabs, origin, path);
      if (tab_index.has_value()) {
        browser->window()->Show();
        tabs->ActivateTabAt(tab_index.value());
        return;
      }
    }
  }

  NavigateParams params(profile, origin.Resolve(path),
                        ui::PAGE_TRANSITION_FIRST);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

class CastToMeetingDeprecationWarningView : public views::View {
 public:
  METADATA_HEADER(CastToMeetingDeprecationWarningView);
  CastToMeetingDeprecationWarningView(const std::string& sink_id,
                                      Profile* profile) {
    DCHECK(profile);

    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
    layout->set_inside_border_insets(gfx::Insets(5));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);

    // Add space to line up with text in receiver list.
    auto* spacer = AddChildView(std::make_unique<views::View>());
    spacer->SetPreferredSize(gfx::Size(55, 1));

    std::u16string text = l10n_util::GetStringUTF16(
        CastToMeetingEnabled() ? IDS_MEDIA_ROUTER_CAST_TO_MEETING_DEPRECATED
                               : IDS_MEDIA_ROUTER_CAST_TO_MEETING_REMOVED);
    std::vector<std::u16string> substrings{u"Google Meet"};
    std::vector<size_t> offsets;
    text = base::ReplaceStringPlaceholders(text, substrings, &offsets);

    auto* label = AddChildView(std::make_unique<views::StyledLabel>());
    label->SetText(text);

    // Try to extract a meeting ID from a sink ID.  This should always succeed
    // unless the "meeting" is actually a Hangout, or if the wrong version of
    // the Cast extension is installed.
    base::Optional<std::string> meeting_id;
    if (sink_id.size() == 17 && base::StartsWith(sink_id, "meet:")) {
      meeting_id = sink_id.substr(5);
    }

    gfx::Range link_range(offsets[0], offsets[0] + substrings[0].length());
    auto link_style =
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &ShowMeetTab, base::Unretained(profile), meeting_id));
    link_style.disable_line_wrapping = false;
    label->AddStyleRange(link_range, link_style);
  }
};

BEGIN_METADATA(CastToMeetingDeprecationWarningView, views::View)
END_METADATA

}  // namespace

CastDialogSinkButton::CastDialogSinkButton(PressedCallback callback,
                                           const UIMediaSink& sink)
    : HoverButton(
          IsEnabledIconType(sink.icon_type)
              ? std::move(callback)
              // Using the default constructor here causes the button to be
              // disabled, including the visual "ink drop" effect.  Calling
              // SetEnabled() or SetState() does not have the same effect.
              : PressedCallback(),
          CreatePrimaryIconForSink(sink),
          sink.friendly_name,
          GetStatusTextForSink(sink),
          /** secondary_icon_view */ nullptr),
      sink_(sink) {
  SetEnabled(sink.state == UIMediaSinkState::AVAILABLE ||
             sink.state == UIMediaSinkState::CONNECTED);
}

CastDialogSinkButton::~CastDialogSinkButton() = default;

void CastDialogSinkButton::OverrideStatusText(
    const std::u16string& status_text) {
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
    if (IsIncompatibleDialSink(sink_)) {
      OverrideStatusText(
          l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_AVAILABLE_SPECIFIC_SITES));
    } else {
      OverrideStatusText(
          l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SOURCE_NOT_SUPPORTED));
    }
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

std::unique_ptr<views::View>
CastDialogSinkButton::MakeCastToMeetingDeprecationWarningView(
    Profile* profile) {
  return IsMeetingIconType(sink_.icon_type)
             ? std::make_unique<CastToMeetingDeprecationWarningView>(sink_.id,
                                                                     profile)
             : nullptr;
}

// static
const gfx::VectorIcon* CastDialogSinkButton::GetVectorIcon(
    SinkIconType icon_type) {
  if (!IsEnabledIconType(icon_type))
    return &::vector_icons::kHelpOutlineIcon;

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

// static
const gfx::VectorIcon* CastDialogSinkButton::GetVectorIcon(UIMediaSink sink) {
  return sink.issue ? &::vector_icons::kInfoOutlineIcon
                    : GetVectorIcon(sink.icon_type);
}

BEGIN_METADATA(CastDialogSinkButton, HoverButton)
END_METADATA

}  // namespace media_router
