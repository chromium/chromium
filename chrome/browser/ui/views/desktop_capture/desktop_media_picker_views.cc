// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"

#include <algorithm>
#include <string>
#include <utility>

#include "audio_capture_permission_checker_mac.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_controller.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_manager.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_utils.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_source_view.h"
#include "chrome/browser/ui/views/desktop_capture/screen_capture_permission_checker.h"
#include "chrome/browser/ui/views/desktop_capture/share_this_tab_dialog_views.h"
#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
#include "chrome/browser/ui/views/media_picker_utils.h"
#include "chrome/browser/ui/views/title_origin_label.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "media/audio/audio_features.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"
#if defined(USE_AURA)
#include "ui/aura/window_tree_host.h"
#endif

BASE_FEATURE(kDesktopMediaPickerMultiLineTitle,
             base::FEATURE_DISABLED_BY_DEFAULT);

using ::blink::mojom::MediaStreamRequestResult;
using ::content::DesktopMediaID;
using ::content::RenderFrameHost;
using ::content::WebContents;
using ::content::WebContentsMediaCaptureId;
using RequestSource = ::DesktopMediaPicker::Params::RequestSource;

const DesktopMediaSourceViewStyle& GetGenericScreenStyle() {
  static const DesktopMediaSourceViewStyle style(
      /*columns=*/2,
      /*item_size=*/gfx::Size(266, 224),
      /*icon_rect=*/gfx::Rect(),
      /*label_rect=*/gfx::Rect(8, 196, 250, 36),
      /*text_alignment=*/gfx::HorizontalAlignment::ALIGN_CENTER,
      /*image_rect=*/gfx::Rect(8, 8, 250, 180));
  return style;
}

const DesktopMediaSourceViewStyle& GetSingleScreenStyle() {
  static const DesktopMediaSourceViewStyle style(GetGenericScreenStyle());
  return style;
}

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AudioToggleStatus {
  kAudioNotRequested = 0,
  kAudioRequestedButNotSupported = 1,
  kAudioRequestedButUserDidNotApprove = 2,
  kAudioRequestedAndUserApproved = 3,
  kMaxValue = kAudioRequestedAndUserApproved
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SelectedTabDiscardStatus {
  kNonDiscarded = 0,
  kDiscarded = 1,
  kMaxValue = kDiscarded
};

#if !BUILDFLAG(IS_CHROMEOS) && defined(USE_AURA)
DesktopMediaID::Id AcceleratedWidgetToDesktopMediaId(
    gfx::AcceleratedWidget accelerated_widget) {
#if BUILDFLAG(IS_WIN)
  return reinterpret_cast<DesktopMediaID::Id>(accelerated_widget);
#else
  return static_cast<DesktopMediaID::Id>(accelerated_widget);
#endif
}
#endif

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GDMResult {
  kDialogDismissed = 0,       // Tab/window closed, navigation, etc.
  kUserCancelled = 1,         // User explicitly cancelled.
  kUserSelectedScreen = 2,    // Screen selected.
  kUserSelectedWindow = 3,    // Window selected.
  kUserSelectedOtherTab = 4,  // Other tab selected from tab-list.
  kUserSelectedThisTab = 5,   // Current tab selected from tab-list.
  kMaxValue = kUserSelectedThisTab
};

void RecordUma(GDMResult result, base::TimeTicks dialog_open_time) {
  base::UmaHistogramEnumeration(
      "Media.Ui.GetDisplayMedia.BasicFlow.UserInteraction", result);

  const base::TimeDelta elapsed = base::TimeTicks::Now() - dialog_open_time;
  base::HistogramBase* histogram = base::LinearHistogram::FactoryTimeGet(
      "Media.Ui.GetDisplayMedia.BasicFlow.DialogDuration",
      /*minimum=*/base::Milliseconds(500), /*maximum=*/base::Seconds(45),
      /*bucket_count=*/91, base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTime(elapsed);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PermissionInteraction {
  kNotShown = 0,
  kShown = 1,
  kClicked = 2,
  kMaxValue = kClicked
};

void RecordUmaCancellation(base::TimeTicks dialog_open_time) {
  RecordAction(base::UserMetricsAction("GetDisplayMedia.Cancel"));
  RecordUma(GDMResult::kUserCancelled, dialog_open_time);
}

// Convenience function for recording UMA.
void RecordUmaSelection(content::GlobalRenderFrameHostId capturer_global_id,
                        const DesktopMediaID& selected_media,
                        DesktopMediaList::Type source_type,
                        base::TimeTicks dialog_open_time) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  switch (source_type) {
    case DesktopMediaList::Type::kNone:
    case DesktopMediaList::Type::kCurrentTab:
      NOTREACHED();

    case DesktopMediaList::Type::kScreen:
      RecordAction(base::UserMetricsAction("GetDisplayMedia.SelectScreen"));
      RecordUma(GDMResult::kUserSelectedScreen, dialog_open_time);
      return;

    case DesktopMediaList::Type::kWindow:
      RecordAction(base::UserMetricsAction("GetDisplayMedia.SelectWindow"));
      RecordUma(GDMResult::kUserSelectedWindow, dialog_open_time);
      return;

    case DesktopMediaList::Type::kWebContents: {
      RecordAction(
          base::UserMetricsAction("GetDisplayMedia.SelectWebContents"));
      // Whether the current tab was selected. Note that this can happen
      // through a non-explicit selection of the current tab through the
      // list of all available tabs.
      const bool current_tab_selected =
          capturer_global_id.child_id ==
              selected_media.web_contents_id.render_process_id &&
          capturer_global_id.frame_routing_id ==
              selected_media.web_contents_id.main_render_frame_id;

      RecordUma(current_tab_selected ? GDMResult::kUserSelectedThisTab
                                     : GDMResult::kUserSelectedOtherTab,
                dialog_open_time);
      return;
    }
  }
  NOTREACHED();
}

#if BUILDFLAG(IS_MAC)
void RecordUma(PermissionInteraction permission_interaction) {
  base::UmaHistogramEnumeration(
      "Media.Ui.GetDisplayMedia.PermissionInteractionMac",
      permission_interaction);
}

void RecordPermissionButtonOpenedAction(DesktopMediaList::Type type) {
  switch (type) {
    case DesktopMediaList::Type::kScreen:
      RecordAction(base::UserMetricsAction(
          "GetDisplayMedia.PermissionPane.Screen.Opened"));
      return;

    case DesktopMediaList::Type::kWindow:
      RecordAction(base::UserMetricsAction(
          "GetDisplayMedia.PermissionPane.Window.Opened"));
      return;

    case DesktopMediaList::Type::kWebContents:
    case DesktopMediaList::Type::kCurrentTab:
    case DesktopMediaList::Type::kNone:
      break;
  }
  NOTREACHED();
}
#endif  // BUILDFLAG(IS_MAC)

std::u16string GetLabelForReselectButton(DesktopMediaList::Type type) {
  switch (type) {
    case DesktopMediaList::Type::kScreen:
      return l10n_util::GetStringUTF16(
          IDS_DESKTOP_MEDIA_PICKER_RESELECT_SCREEN);
    case DesktopMediaList::Type::kWindow:
      return l10n_util::GetStringUTF16(
          IDS_DESKTOP_MEDIA_PICKER_RESELECT_WINDOW);
    case DesktopMediaList::Type::kWebContents:
    case DesktopMediaList::Type::kCurrentTab:
    case DesktopMediaList::Type::kNone:
      break;
  }

  NOTREACHED();
}

// Helper to generate the view containing the enterprise icon and a message that
// the picker choices may have been restricted.
std::unique_ptr<views::View> CreatePolicyRestrictedView() {
  auto icon = std::make_unique<views::ImageView>();
  icon->SetImage(ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                                ui::kColorIcon, 18));

  auto policy_label = std::make_unique<views::Label>();
  policy_label->SetMultiLine(true);
  policy_label->SetText(
      l10n_util::GetStringUTF16(IDS_DESKTOP_MEDIA_PICKER_MANAGED));
  policy_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto policy_view = std::make_unique<views::View>();
  views::BoxLayout* layout =
      policy_view->SetLayoutManager(std::make_unique<views::BoxLayout>());

  int text_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING);
  layout->set_between_child_spacing(text_padding);

  policy_view->AddChildView(std::move(icon));
  policy_view->AddChildView(std::move(policy_label));

  return policy_view;
}

bool ShouldSelectTab(DesktopMediaList::Type type,
                     blink::mojom::PreferredDisplaySurface display_surface) {
  switch (type) {
    case DesktopMediaList::Type::kNone:
    case DesktopMediaList::Type::kCurrentTab:
      break;
    case DesktopMediaList::Type::kScreen:
      return display_surface == blink::mojom::PreferredDisplaySurface::MONITOR;
    case DesktopMediaList::Type::kWindow:
      return display_surface == blink::mojom::PreferredDisplaySurface::WINDOW;
    case DesktopMediaList::Type::kWebContents:
      return display_surface == blink::mojom::PreferredDisplaySurface::BROWSER;
  }
  NOTREACHED();
}

std::unique_ptr<views::ScrollView> CreateScrollView(bool audio_requested) {
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetBackgroundColor(ui::kColorSysSurface4);
  // The overflow indicator is disabled to reduce clutter next to the
  // separator to the audio control when audio is requested or the bottom of
  // the dialog when audio is not requested.
  scroll_view->SetDrawOverflowIndicator(false);
  return scroll_view;
}

int GetHintId(bool is_system_audio_offered, bool is_window_audio_offered) {
  // We can never call this function if both screen and window audio are
  // offered.
  CHECK(!is_system_audio_offered || !is_window_audio_offered);

  if (is_system_audio_offered && !is_window_audio_offered) {
    return IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB_OR_SCREEN;
  }
  if (!is_system_audio_offered && is_window_audio_offered) {
    return IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB_OR_WINDOW;
  }
  if (!is_system_audio_offered && !is_window_audio_offered) {
    return IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB;
  }

  // The check must fail to get here.
  return IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB;
}

int GetLabelForShareSystemAudioToggle(bool suppress_local_audio_playback,
                                      bool restrict_own_audio) {
  if (suppress_local_audio_playback) {
    return IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_SCREEN_WITH_MUTE_WARNING;
  }
#if BUILDFLAG(IS_WIN)
  // Due to an API limitation on Windows we must share all output audio
  // devices when restrict_own_audio is used. We use another string for that
  // scenario.
  return restrict_own_audio
             ? IDS_DESKTOP_MEDIA_PICKER_ALSO_SHARE_ALL_AUDIO_OUTPUT
             : IDS_DESKTOP_MEDIA_PICKER_ALSO_SHARE_SYSTEM_AUDIO;
#else
  return IDS_DESKTOP_MEDIA_PICKER_ALSO_SHARE_SYSTEM_AUDIO;
#endif
}

// Returns the audio type for the window capture by taking into consideration
// the `window_audio_type_requested_` and the system's capabilities.
// Find more information at:
// https://w3c.github.io/mediacapture-screen-share/#windowaudiopreferenceenum
DesktopMediaID::AudioType GetWindowCaptureAudioType(
    const DesktopMediaPicker::Params& params) {
  if (!params.request_audio) {
    return DesktopMediaID::AudioType::kNone;
  }

  if (params.window_audio_preference ==
      blink::mojom::WindowAudioPreference::kExclude) {
    return DesktopMediaID::AudioType::kNone;
  }

  if (params.window_audio_preference ==
          blink::mojom::WindowAudioPreference::kWindow &&
      media::IsApplicationAudioCaptureSupported()) {
    return DesktopMediaID::AudioType::kApplication;
  }

  if (params.window_audio_preference ==
          blink::mojom::WindowAudioPreference::kSystem &&
      DesktopMediaPickerController::IsSystemAudioCaptureSupported(
          params.request_source)) {
    return DesktopMediaID::AudioType::kSystem;
  }

  return DesktopMediaID::AudioType::kNone;
}

}  // namespace

bool DesktopMediaPickerDialogView::AudioSupported(
    DesktopMediaList::Type type) const {
  switch (type) {
    case DesktopMediaList::Type::kScreen:
      return DesktopMediaPickerController::IsSystemAudioCaptureSupported(
          request_source_);
    case DesktopMediaList::Type::kWindow:
      return DesktopMediaPickerController::IsSystemAudioCaptureSupported(
                 request_source_) ||
             media::IsApplicationAudioCaptureSupported();
    case DesktopMediaList::Type::kWebContents:
      return true;
    case DesktopMediaList::Type::kNone:
    case DesktopMediaList::Type::kCurrentTab:
      break;
  }
  NOTREACHED();
}

bool DesktopMediaPickerDialogView::AudioRequestedForType(
    DesktopMediaList::Type type) const {
  // TODO(crbug.com/397167331): Instead of special-casing kScreen, iterate
  // over the `categories_`, find the one with the relevant `type` and
  // return `category.audio_offered`.
  if (type == DesktopMediaList::Type::kScreen) {
    return audio_requested_ && !screen_exclude_system_audio_requested_;
  } else if (type == DesktopMediaList::Type::kWindow) {
    return audio_requested_ && (window_audio_type_requested_ !=
                                blink::mojom::WindowAudioPreference::kExclude);
  } else {
    return audio_requested_;
  }
}

DesktopMediaPickerDialogView::DisplaySurfaceCategory::DisplaySurfaceCategory(
    DesktopMediaList::Type type,
    std::unique_ptr<DesktopMediaListController> controller,
    bool audio_offered,
    bool audio_checked,
    bool supports_reselect_button)
    : type(type),
      controller(std::move(controller)),
      audio_offered(audio_offered),
      audio_checked(audio_checked),
      supports_reselect_button(supports_reselect_button) {}

DesktopMediaPickerDialogView::DisplaySurfaceCategory::DisplaySurfaceCategory(
    DesktopMediaPickerDialogView::DisplaySurfaceCategory&& other)
    : type(other.type),
      controller(std::move(other.controller)),
      audio_offered(other.audio_offered),
      audio_checked(other.audio_checked),
      supports_reselect_button(other.supports_reselect_button),
      pane(other.pane) {}

DesktopMediaPickerDialogView::DisplaySurfaceCategory::
    ~DisplaySurfaceCategory() = default;

DesktopMediaPickerDialogView::DesktopMediaPickerDialogView(
    const DesktopMediaPicker::Params& params,
    DesktopMediaPickerImpl* parent,
    std::vector<std::unique_ptr<DesktopMediaList>> source_lists)
    : web_contents_(params.web_contents),
      request_source_(params.request_source),
      app_name_(params.app_name),
      audio_requested_(params.request_audio),
      screen_exclude_system_audio_requested_(params.exclude_system_audio),
      is_screen_audio_offered_(audio_requested_ &&
                               !params.exclude_system_audio &&
                               AudioSupported(DesktopMediaList::Type::kScreen)),
      window_audio_type_requested_(params.window_audio_preference),
      window_audio_type_offered_(GetWindowCaptureAudioType(params)),
      // Only restrict_own_audio is used if both suppress_local_audio_playback
      // and restrict_own_audio are true. We need to make this choice since
      // there is no implementation for using both at the same time.
      suppress_local_audio_playback_(params.suppress_local_audio_playback &&
                                     !params.restrict_own_audio),
      restrict_own_audio_(params.restrict_own_audio),
      capturer_global_id_(
          params.web_contents
              ? params.web_contents->GetPrimaryMainFrame()->GetGlobalId()
              : content::GlobalRenderFrameHostId()),
      parent_(parent),
      dialog_open_time_(base::TimeTicks::Now()) {
  CHECK(!params.force_audio_checkboxes_to_default_checked ||
        !params.exclude_system_audio);
  RecordAction(base::UserMetricsAction("GetDisplayMedia.ShowDialog"));

#if BUILDFLAG(IS_MAC)
  screen_capture_permission_checker_ =
      ScreenCapturePermissionChecker::MaybeCreate(
          base::BindRepeating(&DesktopMediaPickerDialogView::OnPermissionUpdate,
                              weak_factory_.GetWeakPtr()));
  audio_capture_permission_checker_ =
      AudioCapturePermissionChecker::MaybeCreate(base::BindRepeating(
          &DesktopMediaPickerDialogView::OnAudioPermissionUpdate,
          weak_factory_.GetWeakPtr()));
  RecordUmaAudioCapturePermissionCheckerInteractions(
      audio_capture_permission_checker_
          ? AudioCapturePermissionCheckerInteractions::kEnabled
          : AudioCapturePermissionCheckerInteractions::kDisabled);
#endif

  SetModalType(params.modality);
  int message_id = IDS_DESKTOP_MEDIA_PICKER_SHARE;
#if BUILDFLAG(ENABLE_GLIC)
  if (request_source_ == RequestSource::kGlic) {
    message_id = IDS_GLIC_SCREEN_PICKER_CTA;
  }
#endif
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(message_id));
  SetButtonStyle(ui::mojom::DialogButton::kCancel, ui::ButtonStyle::kTonal);
  RegisterDeleteDelegateCallback(
      RegisterDeleteCallbackPassKey(),
      base::BindOnce(
          [](DesktopMediaPickerDialogView* dialog) {
            // If the dialog is being closed then notify the parent about it.
            // That the parent has not yet been detached indicates that there
            // has been no result yet. We can infer that the user rejected.
            if (dialog->parent_) {
              dialog->parent_->NotifyDialogResult(base::unexpected(
                  MediaStreamRequestResult::PERMISSION_DENIED_BY_USER));
            }
          },
          this));

  const ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kControl),
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL)));

  auto description_label = std::make_unique<views::Label>();
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_label->SetTextStyle(views::style::STYLE_BODY_3);
  description_label->SetEnabledColor(kColorDesktopMediaPickerDescriptionLabel);
  description_label_ = AddChildView(std::move(description_label));

  std::vector<std::pair<std::u16string, std::unique_ptr<View>>> panes;

  // This command-line switch takes precedence over
  // params.force_audio_checkboxes_to_default_checked.
  const bool tab_capture_audio_default_unchecked =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTabCaptureAudioDefaultUnchecked);
  const bool system_audio_capture_default_checked =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSystemAudioCaptureDefaultChecked);

  for (auto& source_list : source_lists) {
    switch (source_list->GetMediaListType()) {
      case DesktopMediaList::Type::kNone:
      case DesktopMediaList::Type::kCurrentTab:
        NOTREACHED();
      case DesktopMediaList::Type::kScreen: {
        std::unique_ptr<views::ScrollView> screen_scroll_view =
            CreateScrollView(audio_requested_);
        screen_scroll_view->SetID(VIEW_ID_MEDIA_PICKER_SCREEN_SCROLL_VIEW);
        std::u16string screen_title_text = l10n_util::GetStringUTF16(
            IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_SCREEN);
        auto list_controller = std::make_unique<DesktopMediaListController>(
            this, std::move(source_list));
        const bool supports_reselect_button =
            list_controller->SupportsReselectButton();
        screen_scroll_view->SetContents(list_controller->CreateView(
            GetGenericScreenStyle(), GetSingleScreenStyle(), screen_title_text,
            DesktopMediaList::Type::kScreen));
        // Allow space for the audio-toggle controller.
        screen_scroll_view->ClipHeightTo(
            GetGenericScreenStyle().item_size.height() +
                GetGenericScreenStyle().label_rect.height(),
            GetGenericScreenStyle().item_size.height() * 3 / 2);
        screen_scroll_view->SetHorizontalScrollBarMode(
            views::ScrollView::ScrollBarMode::kDisabled);

        std::unique_ptr<views::View> pane = SetupPane(
            DesktopMediaList::Type::kScreen, std::move(list_controller),
            /*audio_offered=*/is_screen_audio_offered_,
            /*audio_checked=*/
            params.force_audio_checkboxes_to_default_checked ||
                system_audio_capture_default_checked,
            supports_reselect_button, std::move(screen_scroll_view));
        panes.emplace_back(screen_title_text, std::move(pane));
        break;
      }
      case DesktopMediaList::Type::kWindow: {
        const DesktopMediaSourceViewStyle kWindowStyle =
            DesktopMediaSourceViewStyle(
                /*columns=*/3,
                /*item_size=*/gfx::Size(176, 164),
                /*icon_rect=*/gfx::Rect(8, 136, 16, 16),
                /*label_rect=*/gfx::Rect(32, 136, 136, 20),
                /*text_alignment=*/gfx::HorizontalAlignment::ALIGN_LEFT,
                /*image_rect=*/gfx::Rect(8, 8, 160, 120));

        std::unique_ptr<views::ScrollView> window_scroll_view =
            CreateScrollView(audio_requested_);
        std::u16string window_title_text = l10n_util::GetStringUTF16(
            IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_WINDOW);
        auto list_controller = std::make_unique<DesktopMediaListController>(
            this, std::move(source_list));
        const bool supports_reselect_button =
            list_controller->SupportsReselectButton();
        window_scroll_view->SetContents(list_controller->CreateView(
            kWindowStyle, kWindowStyle, window_title_text,
            DesktopMediaList::Type::kWindow));
        window_scroll_view->ClipHeightTo(kWindowStyle.item_size.height(),
                                         kWindowStyle.item_size.height() * 2);
        window_scroll_view->SetHorizontalScrollBarMode(
            views::ScrollView::ScrollBarMode::kDisabled);
        std::unique_ptr<views::View> pane = SetupPane(
            DesktopMediaList::Type::kWindow, std::move(list_controller),
            /*audio_offered=*/IsWindowAudioOffered(),
            /*audio_checked=*/
            window_audio_type_offered_ ==
                    DesktopMediaID::AudioType::kApplication
                ? true
                : params.force_audio_checkboxes_to_default_checked ||
                      system_audio_capture_default_checked,
            supports_reselect_button, std::move(window_scroll_view));
        panes.emplace_back(window_title_text, std::move(pane));
        break;
      }
      case DesktopMediaList::Type::kWebContents: {
        // Note that "other tab" is inaccurate - we actually allow any tab
        // to be selected in either case.
        const std::u16string title =
            l10n_util::GetStringUTF16(IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_TAB);
        auto list_controller = std::make_unique<DesktopMediaListController>(
            this, std::move(source_list));
        const bool supports_reselect_button =
            list_controller->SupportsReselectButton();
        std::unique_ptr<views::View> list_view =
            list_controller->CreateTabListView(title);
        std::unique_ptr<views::View> pane = SetupPane(
            DesktopMediaList::Type::kWebContents, std::move(list_controller),
            /*audio_offered=*/
            AudioSupported(DesktopMediaList::Type::kWebContents),
            /*audio_checked=*/!tab_capture_audio_default_unchecked,
            supports_reselect_button, std::move(list_view));
        panes.emplace_back(title, std::move(pane));
        break;
      }
    }
  }

  if (panes.size() > 1) {
    auto tabbed_pane = std::make_unique<views::TabbedPane>();
    for (auto& pane : panes) {
      tabbed_pane->AddTab(pane.first, std::move(pane.second));
    }
    for (size_t i = 0; i < categories_.size(); i++) {
      if (ShouldSelectTab(categories_[i].type,
                          params.preferred_display_surface)) {
        tabbed_pane->SelectTabAt(i, /*animate=*/false);
        break;
      }
    }
    tabbed_pane->SetListener(this);
    tabbed_pane->SetFocusBehavior(views::View::FocusBehavior::NEVER);
    tabbed_pane_ = AddChildView(std::move(tabbed_pane));
  } else {
    AddChildView(std::move(panes.front().second));
  }

  if (request_source_ == RequestSource::kGetDisplayMedia) {
    description_label_->SetText(
        l10n_util::GetStringUTF16(IDS_DISPLAY_MEDIA_PICKER_TEXT));
  } else {
    if (params.app_name == params.target_name) {
      description_label_->SetText(l10n_util::GetStringFUTF16(
          IDS_DESKTOP_MEDIA_PICKER_TEXT, params.app_name));
    } else {
      description_label_->SetText(
          l10n_util::GetStringFUTF16(IDS_DESKTOP_MEDIA_PICKER_TEXT_DELEGATED,
                                     params.app_name, params.target_name));
    }
  }

#if BUILDFLAG(ENABLE_GLIC)
  if (request_source_ == RequestSource::kGlic) {
    description_label_->SetText(
        l10n_util::GetStringUTF16(IDS_GLIC_SCREEN_PICKER_DESCRIPTION));
  }
#endif

  DCHECK(!categories_.empty());

  if (params.restricted_by_policy) {
    AddChildView(CreatePolicyRestrictedView());
  }

  previously_selected_category_ = GetSelectedTabIndex();
  ConfigureUIForNewPane(previously_selected_category_);

  bool modal_dialog = MediaPickerCanShowAsWebModal(params.web_contents);
  views::Widget* widget = CreateMediaPickerDialogWidget(
      modal_dialog ? chrome::FindBrowserWithTab(params.web_contents) : nullptr,
      params.web_contents,
      /*delegate=*/this, params.context, /*parent=*/gfx::NativeView());

  extensions::SecurityDialogTracker::GetInstance()->AddSecurityDialog(widget);

#if BUILDFLAG(IS_MAC)
  // On Mac, even modals are shown using separate native windows.
  bool is_separate_native_window = true;
#else
  bool is_separate_native_window = !modal_dialog;
#endif

  // If the picker is a separate native window, it should not be shown in the
  // source list, so its id is passed into NativeDesktopMediaList to be ignored.
  DesktopMediaID dialog_window_id;
  if (is_separate_native_window) {
    dialog_window_id = DesktopMediaID::RegisterNativeWindow(
        DesktopMediaID::TYPE_WINDOW, widget->GetNativeWindow());

#if !BUILDFLAG(IS_CHROMEOS) && defined(USE_AURA)
    // Set native window ID if the windows is outside Ash.
    dialog_window_id.id = AcceleratedWidgetToDesktopMediaId(
        widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
#elif BUILDFLAG(IS_MAC)
    // On Mac, the window_id in DesktopMediaID is the same as the actual native
    // window ID. Note that assuming this is a bit of a layering violation; the
    // fact that this code makes that assumption is documented at the code that
    // causes it to hold, so hopefully nobody changes that :)
    dialog_window_id.id = dialog_window_id.window_id;
#endif
  }

  for (auto& category : categories_) {
    category.controller->StartUpdating(dialog_window_id);
  }

  GetSelectedController()->FocusView();
}

DesktopMediaPickerDialogView::~DesktopMediaPickerDialogView() {
#if BUILDFLAG(IS_MAC)
  RecordPermissionInteractionUma();
#endif
}

void DesktopMediaPickerDialogView::RecordUmaDismissal() const {
  RecordUma(GDMResult::kDialogDismissed, dialog_open_time_);
}

void DesktopMediaPickerDialogView::TabSelectedAt(int index) {
  if (previously_selected_category_ == index) {
    return;
  }
  ConfigureUIForNewPane(index);
  categories_[previously_selected_category_].controller->HideView();
  categories_[index].controller->FocusView();
  DialogModelChanged();
  previously_selected_category_ = index;
}

void DesktopMediaPickerDialogView::ConfigureUIForNewPane(int index) {
  // Process any potential audio_checked state updates.
  StoreAudioCheckboxState();

  RemoveCurrentPaneUI();

  const DisplaySurfaceCategory& category = categories_[index];
  MaybeCreateReselectButtonForPane(category);

  if (!category.pane) {
    return;
  }

  if (audio_requested_ && category.audio_offered) {
    category.pane->SetAudioSharingApprovedByUser(category.audio_checked);
  }
#if BUILDFLAG(IS_MAC)
  if (category.pane->IsPermissionPaneVisible()) {
    permission_pane_was_shown_ = true;
    RecordPermissionButtonOpenedAction(category.type);
  }
#endif
}

void DesktopMediaPickerDialogView::StoreAudioCheckboxState() {
  const DisplaySurfaceCategory& prev_category =
      categories_[previously_selected_category_];
  const bool has_audio_control =
      prev_category.pane && prev_category.pane->AudioOffered();
  if (!has_audio_control || !prev_category.audio_offered) {
    return;
  }

  // Store pre-change audio control state.
  const bool checked =
      prev_category.pane && prev_category.pane->IsAudioSharingApprovedByUser();
  for (auto& category : categories_) {
    if (category.type == prev_category.type) {
      category.audio_checked = checked;
    }
  }
}

void DesktopMediaPickerDialogView::RemoveCurrentPaneUI() {
  // We cannot remove the Views from the "ExtraView" slot where they are added.
  // They will be re-created for the given category that is visible, but in the
  // mean time (and in case they aren't needed), we set them invisible and drop
  // our pointer to them. Once they are replaced by a new "SetExtraView" call
  // they will be destroyed.

  if (reselect_button_) {
    reselect_button_->SetVisible(false);
    reselect_button_ = nullptr;
  }
}

void DesktopMediaPickerDialogView::MaybeCreateReselectButtonForPane(
    const DisplaySurfaceCategory& category) {
  if (!category.supports_reselect_button) {
    return;
  }

  auto reselect_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&DesktopMediaListController::OnReselectRequested,
                          category.controller->GetWeakPtr()),
      GetLabelForReselectButton(category.type));
  reselect_button->SetVisible(true);
  reselect_button->SetEnabled(category.controller->can_reselect());
  reselect_button_ = SetExtraView(std::move(reselect_button));
}

int DesktopMediaPickerDialogView::GetLabelForWindowPaneAudioToggle() const {
  switch (window_audio_type_offered_) {
    case DesktopMediaID::AudioType::kNone:
      return GetHintId(is_screen_audio_offered_,
                       /*is_window_audio_offered=*/false);
    case DesktopMediaID::AudioType::kApplication:
      return IDS_DESKTOP_MEDIA_PICKER_ALSO_SHARE_APPLICATION_AUDIO;
    case DesktopMediaID::AudioType::kSystem:
      return GetLabelForShareSystemAudioToggle(suppress_local_audio_playback_,
                                               restrict_own_audio_);
  }
  NOTREACHED();
}

bool DesktopMediaPickerDialogView::IsWindowAudioOffered() const {
  return window_audio_type_offered_ !=
         content::DesktopMediaID::AudioType::kNone;
}

std::u16string DesktopMediaPickerDialogView::GetLabelForAudioToggle(
    const DisplaySurfaceCategory& category) const {
  if (!category.audio_offered) {
    return l10n_util::GetStringUTF16(
        GetHintId(is_screen_audio_offered_, IsWindowAudioOffered()));
  }

  switch (category.type) {
    case DesktopMediaList::Type::kScreen: {
      return l10n_util::GetStringUTF16(GetLabelForShareSystemAudioToggle(
          suppress_local_audio_playback_, restrict_own_audio_));
    }
    case DesktopMediaList::Type::kWindow:
      // Check windowAudio preference, as we can select either window or system
      // audio
      return l10n_util::GetStringUTF16(GetLabelForWindowPaneAudioToggle());
    case DesktopMediaList::Type::kWebContents:
      return l10n_util::GetStringUTF16(
          IDS_DESKTOP_MEDIA_PICKER_ALSO_SHARE_TAB_AUDIO);
    case DesktopMediaList::Type::kNone:
    case DesktopMediaList::Type::kCurrentTab:
      break;
  }
  NOTREACHED();
}

std::unique_ptr<views::View> DesktopMediaPickerDialogView::SetupPane(
    DesktopMediaList::Type type,
    std::unique_ptr<DesktopMediaListController> controller,
    bool audio_offered,
    bool audio_checked,
    bool supports_reselect_button,
    std::unique_ptr<views::View> content_view) {
  DisplaySurfaceCategory& category =
      categories_.emplace_back(type, std::move(controller), audio_offered,
                               audio_checked, supports_reselect_button);

  base::RepeatingCallback<void(void)> trigger_audio_permission_check;
#if BUILDFLAG(IS_MAC)
  if (audio_capture_permission_checker_ &&
      (type == DesktopMediaList::Type::kScreen ||
       type == DesktopMediaList::Type::kWindow)) {
    trigger_audio_permission_check = base::BindRepeating(
        &DesktopMediaPickerDialogView::OnAudioSharingApprovedByUserUpdate,
        weak_factory_.GetWeakPtr());
  }
#endif

  auto share_audio_view =
      audio_requested_
          ? std::make_unique<ShareAudioView>(GetLabelForAudioToggle(category),
                                             category.audio_offered,
                                             trigger_audio_permission_check)
          : nullptr;

  auto pane = std::make_unique<DesktopMediaPaneView>(
      category.type, std::move(content_view), std::move(share_audio_view));
  if (audio_requested_ && audio_offered) {
    pane->SetAudioSharingApprovedByUser(audio_checked);
  }
  category.pane = pane.get();
  return pane;
}

int DesktopMediaPickerDialogView::GetSelectedTabIndex() const {
  return tabbed_pane_ ? tabbed_pane_->GetSelectedTabIndex() : 0;
}

const DesktopMediaListController*
DesktopMediaPickerDialogView::GetSelectedController() const {
  return categories_[GetSelectedTabIndex()].controller.get();
}

DesktopMediaListController*
DesktopMediaPickerDialogView::GetSelectedController() {
  return categories_[GetSelectedTabIndex()].controller.get();
}

DesktopMediaList::Type DesktopMediaPickerDialogView::GetSelectedSourceListType()
    const {
  const int index = GetSelectedTabIndex();
  DCHECK_GE(index, 0);
  DCHECK_LT(static_cast<size_t>(index), categories_.size());
  return categories_[index].type;
}

bool DesktopMediaPickerDialogView::IsAudioSharingApprovedByUser() const {
  const int index = GetSelectedTabIndex();
  CHECK_GE(index, 0);
  CHECK_LT(static_cast<size_t>(index), categories_.size());
  return categories_[index].pane &&
         categories_[index].pane->IsAudioSharingApprovedByUser();
}

bool DesktopMediaPickerDialogView::IsAudioSharingControlEnabled() const {
  const int index = GetSelectedTabIndex();
  CHECK_GE(index, 0);
  CHECK_LT(static_cast<size_t>(index), categories_.size());
  return categories_[index].pane &&
         categories_[index].pane->IsAudioSharingControlEnabled();
}

void DesktopMediaPickerDialogView::RecordSourceCountsUma() {
  // Note that tabs are counted up to 1000, and windows/screens up to 100.

  const std::optional<int> tab_count =
      CountSourcesOfType(DesktopMediaList::Type::kWebContents);
  if (tab_count.has_value()) {
    base::UmaHistogramCounts1000(
        "Media.Ui.GetDisplayMedia.BasicFlow.SourceCount.Tabs",
        tab_count.value());
  }

  const std::optional<int> window_count =
      CountSourcesOfType(DesktopMediaList::Type::kWindow);
  if (window_count.has_value()) {
    base::UmaHistogramCounts100(
        "Media.Ui.GetDisplayMedia.BasicFlow.SourceCount.Windows",
        window_count.value());
  }

  const std::optional<int> screen_count =
      CountSourcesOfType(DesktopMediaList::Type::kScreen);
  if (screen_count.has_value()) {
    base::UmaHistogramCounts100(
        "Media.Ui.GetDisplayMedia.BasicFlow.SourceCount.Screens",
        screen_count.value());
  }
}

void DesktopMediaPickerDialogView::RecordAudioToggleUma(
    const content::DesktopMediaID& source) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (request_source_ != RequestSource::kGetDisplayMedia) {
    return;
  }

  const char* display_surface = nullptr;
  switch (source.type) {
    case DesktopMediaID::Type::TYPE_WEB_CONTENTS:
      display_surface = "Tabs";
      break;
    case DesktopMediaID::Type::TYPE_WINDOW:
      display_surface = "Windows";
      break;
    case DesktopMediaID::Type::TYPE_SCREEN:
      display_surface = "Screens";
      break;
    case DesktopMediaID::Type::TYPE_NONE:
      break;  // Should not happen - subsequent CHECK failure.
  }
  CHECK_NE(display_surface, nullptr);
  const std::string name =
      base::StrCat({"Media.Ui.GetDisplayMedia.BasicFlow.AudioToggleState.",
                    display_surface});

  const DesktopMediaList::Type type = AsDesktopMediaListType(source.type);
  AudioToggleStatus status;
  if (!AudioRequestedForType(type)) {
    status = AudioToggleStatus::kAudioNotRequested;
  } else if (!AudioSupported(type)) {
    status = AudioToggleStatus::kAudioRequestedButNotSupported;
  } else {
    status = source.audio_share
                 ? AudioToggleStatus::kAudioRequestedAndUserApproved
                 : AudioToggleStatus::kAudioRequestedButUserDidNotApprove;
  }

  base::UmaHistogramEnumeration(name, status);

  if (source.type == DesktopMediaID::Type::TYPE_WINDOW &&
      window_audio_type_offered_ == DesktopMediaID::AudioType::kApplication) {
    base::UmaHistogramEnumeration(
        "Media.Ui.GetDisplayMedia.BasicFlow.AudioToggleState.WindowsAppAudio",
        status);
  }
}

void DesktopMediaPickerDialogView::RecordTabDiscardedStatusUma(
    const DesktopMediaID& source) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (source.type != DesktopMediaID::Type::TYPE_WEB_CONTENTS) {
    return;
  }

  const WebContentsMediaCaptureId& web_contents_id = source.web_contents_id;
  RenderFrameHost* const rfh = RenderFrameHost::FromID(
      web_contents_id.render_process_id, web_contents_id.main_render_frame_id);
  WebContents* const wc = WebContents::FromRenderFrameHost(rfh);
  if (!wc) {
    return;
  }

  const SelectedTabDiscardStatus status =
      wc->WasDiscarded() ? SelectedTabDiscardStatus::kDiscarded
                         : SelectedTabDiscardStatus::kNonDiscarded;

  // Note: For simplicty's sake, we count all invocations of the picker,
  // regardless of whether getDisplayMedia() or extension-based.
  base::UmaHistogramEnumeration(
      "Media.Ui.GetDisplayMedia.BasicFlow.SelectedTabDiscardStatus", status);
}

#if BUILDFLAG(IS_MAC)
void DesktopMediaPickerDialogView::RecordUserActionOnDeniedAudioPermissionUma(
    std::optional<content::DesktopMediaID> source) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (request_source_ != RequestSource::kGetDisplayMedia) {
    return;
  }

  if (!audio_capture_permission_checker_ ||
      audio_capture_permission_checker_->GetState() !=
          AudioCapturePermissionChecker::State::kDenied) {
    return;
  }

  AudioCapturePermissionCheckerInteractions action;
  if (!source) {
    action =
        AudioCapturePermissionCheckerInteractions::kCancelSharingAfterDenial;
  } else if (source->type == DesktopMediaID::Type::TYPE_WEB_CONTENTS) {
    action = AudioCapturePermissionCheckerInteractions::kShareTabAfterDenial;
  } else {
    action = source->audio_share
                 ? AudioCapturePermissionCheckerInteractions::
                       kShareWindowOrScreenWithAudioAfterDenial
                 : AudioCapturePermissionCheckerInteractions::
                       kShareWindowOrScreenWithoutAudioAfterDenial;
  }

  RecordUmaAudioCapturePermissionCheckerInteractions(action);
}
#endif  // BUILDFLAG(IS_MAC)

std::optional<int> DesktopMediaPickerDialogView::CountSourcesOfType(
    DesktopMediaList::Type type) {
  std::optional<int> count;

  for (const DisplaySurfaceCategory& category : categories_) {
    if (category.type != type) {
      continue;
    }

    if (!count.has_value()) {
      count = 0;
    }
    *count += static_cast<int>(category.controller->GetSourceCount());
  }

  return count;
}

void DesktopMediaPickerDialogView::DetachParent() {
  parent_ = nullptr;
}

gfx::Size DesktopMediaPickerDialogView::CalculatePreferredSize(
    const views::SizeBounds& /*available_size*/) const {
  static constexpr size_t kDialogViewWidth = 600;
  return gfx::Size(
      kDialogViewWidth,
      GetLayoutManager()->GetPreferredHeightForWidth(this, kDialogViewWidth));
}

void DesktopMediaPickerDialogView::AddedToWidget() {
  // Allow breaking the title over multiple lines if
  // DesktopMediaPickerDialogView has a BubbleFrameView in order to handle long
  // domain names. DesktopMediaPickerDialogView is not guaranteed to have a
  // BubbleFrameView so this check is needed, but in practice it uses one on all
  // desktop platforms.
  //
  // TODO(420734141): Make DesktopMediaPickerDialogView always have a
  // BubbleFrameView.
  views::BubbleFrameView* bubble_frame_view = GetBubbleFrameView();
  if (base::FeatureList::IsEnabled(kDesktopMediaPickerMultiLineTitle) &&
      bubble_frame_view) {
    bubble_frame_view->SetTitleView(CreateTitleOriginLabel(GetWindowTitle()));
  }
}

std::u16string DesktopMediaPickerDialogView::GetWindowTitle() const {
  if (request_source_ == RequestSource::kGetDisplayMedia) {
    return l10n_util::GetStringFUTF16(IDS_DISPLAY_MEDIA_PICKER_TITLE,
                                      app_name_);
  }
#if BUILDFLAG(ENABLE_GLIC)
  if (request_source_ == RequestSource::kGlic) {
    return l10n_util::GetStringUTF16(IDS_GLIC_SCREEN_PICKER_HEADLINE);
  }
#endif

  int title_id = IDS_DESKTOP_MEDIA_PICKER_TITLE;

  if (!tabbed_pane_) {
    switch (categories_.front().type) {
      case DesktopMediaList::Type::kScreen:
        title_id = IDS_DESKTOP_MEDIA_PICKER_TITLE_SCREEN_ONLY;
        break;
      case DesktopMediaList::Type::kWindow:
        title_id = IDS_DESKTOP_MEDIA_PICKER_TITLE_WINDOW_ONLY;
        break;
      case DesktopMediaList::Type::kWebContents:
        title_id = IDS_DESKTOP_MEDIA_PICKER_TITLE_WEB_CONTENTS_ONLY;
        break;
      default:
        break;
    }
  }

  return l10n_util::GetStringUTF16(title_id);
}

bool DesktopMediaPickerDialogView::IsDialogButtonEnabled(
    ui::mojom::DialogButton button) const {
  return button != ui::mojom::DialogButton::kOk ||
         GetSelectedController()->GetSelection().has_value() ||
         accepted_source_.has_value();
}

views::View* DesktopMediaPickerDialogView::GetInitiallyFocusedView() {
  return GetCancelButton();
}

bool DesktopMediaPickerDialogView::Accept() {
  CHECK(IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  // Accept() can only be called if IsDialogButtonEnabled() for the OK button,
  // which implies that at least one of these two options has_value().
  DesktopMediaID source = accepted_source_.has_value()
                              ? accepted_source_.value()
                              : GetSelectedController()->GetSelection().value();
  source.audio_share = IsAudioSharingApprovedByUser();

  if (source.type == DesktopMediaID::Type::TYPE_WINDOW) {
    source.window_audio_type = window_audio_type_offered_;
  }

  if (request_source_ == RequestSource::kGetDisplayMedia) {
    RecordUmaSelection(capturer_global_id_, source, GetSelectedSourceListType(),
                       dialog_open_time_);
  }
  RecordSourceCountsUma();
  RecordAudioToggleUma(source);
  RecordTabDiscardedStatusUma(source);
#if BUILDFLAG(IS_MAC)
  RecordUserActionOnDeniedAudioPermissionUma(source);
#endif

  if (parent_) {
    parent_->NotifyDialogResult(source);
  }

  // Return true to close the window.
  return true;
}

bool DesktopMediaPickerDialogView::Cancel() {
  if (request_source_ == RequestSource::kGetDisplayMedia) {
    RecordUmaCancellation(dialog_open_time_);
  }
  RecordSourceCountsUma();
#if BUILDFLAG(IS_MAC)
  RecordUserActionOnDeniedAudioPermissionUma(std::nullopt);
#endif

  return views::DialogDelegateView::Cancel();
}

bool DesktopMediaPickerDialogView::ShouldShowCloseButton() const {
  return false;
}

void DesktopMediaPickerDialogView::OnWidgetInitialized() {
  views::DialogDelegateView::OnWidgetInitialized();
}

void DesktopMediaPickerDialogView::
    MaybeUpdateAudioSharingControlStateForApplicationAudioCapture() {
  CHECK_EQ(GetSelectedSourceListType(), DesktopMediaList::Type::kWindow);
  CHECK_EQ(window_audio_type_offered_, DesktopMediaID::AudioType::kApplication);

  DisplaySurfaceCategory& window_category = categories_[GetSelectedTabIndex()];
  const bool has_audio_control =
      window_category.pane && window_category.pane->AudioOffered();
  if (!has_audio_control || !window_category.audio_offered) {
    return;
  }

  if (GetSelectedController()->HasSelectedChromiumWindow() &&
      !is_chromium_window_selected_) {
    // Disable the audio-checkbox if the selected window is a Chromium
    // window, since we cannot capture audio from Chromium windows for privacy
    // reasons.
    if (window_category.pane) {
      window_category.audio_checked =
          window_category.pane->IsAudioSharingApprovedByUser();
      window_category.pane->SetAudioSharingApprovedByUser(false);
      window_category.pane->SetAudioSharingControlEnabled(false);
    }
    is_chromium_window_selected_ = true;
  } else if (!GetSelectedController()->HasSelectedChromiumWindow() &&
             is_chromium_window_selected_) {
    // Restore the audio-checkbox state.
    if (window_category.pane) {
      window_category.pane->SetAudioSharingApprovedByUser(
          window_category.audio_checked);
      window_category.pane->SetAudioSharingControlEnabled(true);
    }
    is_chromium_window_selected_ = false;
  }
}

void DesktopMediaPickerDialogView::OnSelectionChanged() {
  if (GetSelectedSourceListType() == DesktopMediaList::Type::kWindow &&
      window_audio_type_offered_ == DesktopMediaID::AudioType::kApplication) {
    MaybeUpdateAudioSharingControlStateForApplicationAudioCapture();
  }
  DialogModelChanged();
}

void DesktopMediaPickerDialogView::AcceptSource() {
  // This will call Accept() and close the dialog.
  AcceptDialog();
}

void DesktopMediaPickerDialogView::AcceptSpecificSource(
    const DesktopMediaID& source) {
  VLOG(1) << "DMPDV::AcceptSpecificSource: source_id = " << source.id;

  if (tabbed_pane_) {
    for (size_t i = 0; i < categories_.size(); i++) {
      if (AsDesktopMediaIdType(categories_[i].type) == source.type) {
        tabbed_pane_->SelectTabAt(i, /*animate=*/false);
        break;
      }
    }
  }

  accepted_source_ = std::optional<DesktopMediaID>(source);
  AcceptSource();
}

void DesktopMediaPickerDialogView::Reject() {
  RecordSourceCountsUma();
  CancelDialog();
}

void DesktopMediaPickerDialogView::OnSourceListLayoutChanged() {
  PreferredSizeChanged();
}

void DesktopMediaPickerDialogView::OnDelegatedSourceListDismissed() {
#if BUILDFLAG(IS_MAC)
  // This function is called when the native picker has been cancelled or has
  // experienced an error. In both these cases for MacOS, we should reject the
  // dialog and close it.
  Reject();
  return;
#else
  if (!tabbed_pane_) {
    Reject();
    return;
  }

  size_t fallback_pane_index = std::distance(
      categories_.begin(),
      std::ranges::find(categories_, DesktopMediaList::Type::kWebContents,
                        &DisplaySurfaceCategory::type));

  if (fallback_pane_index >= categories_.size()) {
    Reject();
    return;
  }

  categories_[fallback_pane_index].controller->ClearSelection();

  tabbed_pane_->SelectTabAt(fallback_pane_index);

  GetCancelButton()->RequestFocus();
#endif
}

void DesktopMediaPickerDialogView::OnCanReselectChanged(
    const DesktopMediaListController* controller) {
  // DelegatedSourceLists (currently just PipeWire and currently the only
  // controllers that support a reselect button), aren't necessarily running on
  // the UI thread; so there is a very slight chance that we could have an event
  // working it's way back to us after we've switched controllers. If that's the
  // case, then the state will be updated the next time that controller is
  // active, but we shouldn't update it just now.
  if (controller != GetSelectedController() || !reselect_button_) {
    return;
  }

  reselect_button_->SetEnabled(controller->can_reselect());
}

#if BUILDFLAG(IS_MAC)
void DesktopMediaPickerDialogView::OnPermissionUpdate(bool has_permission) {
  CHECK(screen_capture_permission_checker_);

  if (!initial_permission_state_.has_value()) {
    initial_permission_state_ = has_permission;
  }

  if (has_permission) {
    // Avoid needless polling.
    // (A user who revokes permission while the media-picker is visible,
    // likely knows what they are doing, and can recover by themselves.)
    screen_capture_permission_checker_->Stop();
  }

  for (auto& category : categories_) {
    category.pane->OnScreenCapturePermissionUpdate(has_permission);
  }
}

void DesktopMediaPickerDialogView::RecordPermissionInteractionUma() const {
  if (initial_permission_state_.value_or(true)) {
    return;
  }

  bool permission_button_was_clicked = false;
  for (auto& category : categories_) {
    if (category.pane->WasPermissionButtonClicked()) {
      permission_button_was_clicked = true;
      break;
    }
  }

  const PermissionInteraction permission_interaction =
      permission_button_was_clicked ? PermissionInteraction::kClicked
      : permission_pane_was_shown_  ? PermissionInteraction::kShown
                                    : PermissionInteraction::kNotShown;

  RecordUma(permission_interaction);
}

void DesktopMediaPickerDialogView::OnAudioSharingApprovedByUserUpdate() {
  const int index = GetSelectedTabIndex();
  CHECK_GE(index, 0);
  CHECK_LT(static_cast<size_t>(index), categories_.size());
  if (!categories_[index].pane) {
    return;
  }

  if (categories_[index].pane->IsAudioSharingApprovedByUser()) {
    switch (audio_capture_permission_checker_->GetState()) {
      case AudioCapturePermissionChecker::State::kUnknown:
        audio_capture_permission_checker_->RunCheck();
        break;
      case AudioCapturePermissionChecker::State::kDenied:
        categories_[index].pane->SetAudioWarningVisible(true);
        break;
      case AudioCapturePermissionChecker::State::kGranted:
      case AudioCapturePermissionChecker::State::kChecking:
        // Do nothing.
        break;
    }
  } else {
    categories_[index].pane->SetAudioWarningVisible(false);
  }
}

void DesktopMediaPickerDialogView::OnAudioPermissionUpdate() {
  if (audio_capture_permission_checker_->GetState() !=
      AudioCapturePermissionChecker::State::kDenied) {
    return;
  }

  for (auto& category : categories_) {
    if (!category.pane || (category.type != DesktopMediaList::Type::kScreen &&
                           category.type != DesktopMediaList::Type::kWindow)) {
      continue;
    }

    if (category.pane->IsAudioSharingApprovedByUser()) {
      category.pane->SetAudioWarningVisible(true);
    }
  }
}

#endif

BEGIN_METADATA(DesktopMediaPickerDialogView)
END_METADATA

DesktopMediaPickerImpl::DesktopMediaPickerImpl() : dialog_(nullptr) {}

DesktopMediaPickerImpl::~DesktopMediaPickerImpl() {
  if (dialog_) {
    if (request_source_ == RequestSource::kGetDisplayMedia) {
      dialog_->RecordUmaDismissal();
    }
    dialog_->DetachParent();
    dialog_->GetWidget()->Close();
  }
}

void DesktopMediaPickerImpl::Show(
    const DesktopMediaPicker::Params& params,
    std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
    DoneCallback done_callback) {
  DesktopMediaPickerManager::Get()->OnShowDialog(params);

  request_source_ = params.request_source;
  callback_ = std::move(done_callback);
  dialog_ =
      new DesktopMediaPickerDialogView(params, this, std::move(source_lists));
}

void DesktopMediaPickerImpl::NotifyDialogResult(
    base::expected<DesktopMediaID, MediaStreamRequestResult> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Once this method is called the |dialog_| will close and destroy itself.
  dialog_->DetachParent();
  dialog_ = nullptr;

  DesktopMediaPickerManager::Get()->OnHideDialog();

  if (callback_.is_null()) {
    return;
  }

  // Notify the |callback_| asynchronously because it may need to destroy
  // DesktopMediaPicker.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), result));
}

// static
std::unique_ptr<DesktopMediaPicker> DesktopMediaPicker::Create(
    const content::MediaStreamRequest* request) {
  if (request &&
      request->video_type ==
          blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB) {
    return std::make_unique<ShareThisTabMediaPicker>();
  } else {
    return std::make_unique<DesktopMediaPickerImpl>();
  }
}
