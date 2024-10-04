// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/desktop_capture_devices_util.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_controller.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_manager.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_utils.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_source_view.h"
#include "chrome/browser/ui/views/desktop_capture/screen_capture_permission_checker.h"
#include "chrome/browser/ui/views/desktop_capture/share_this_tab_dialog_views.h"
#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
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
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
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

using content::DesktopMediaID;
using content::RenderFrameHost;
using content::WebContents;
using content::WebContentsMediaCaptureId;
using RequestSource = DesktopMediaPicker::Params::RequestSource;

enum class DesktopMediaPickerDialogView::DialogType : int {
  kStandard = 0,
  kPreferCurrentTab = 1
};

namespace {

using DialogType = DesktopMediaPickerDialogView::DialogType;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SelectedTabDiscardStatus {
  kNonDiscarded = 0,
  kDiscarded = 1,
  kMaxValue = kDiscarded
};

#if !BUILDFLAG(IS_CHROMEOS_ASH) && defined(USE_AURA)
DesktopMediaID::Id AcceleratedWidgetToDesktopMediaId(
    gfx::AcceleratedWidget accelerated_widget) {
#if BUILDFLAG(IS_WIN)
  return reinterpret_cast<DesktopMediaID::Id>(accelerated_widget);
#else
  return static_cast<DesktopMediaID::Id>(accelerated_widget);
#endif
}
#endif

BASE_FEATURE(kWarnUserOfSystemWideLocalAudioSuppression,
             "WarnUserOfSystemWideLocalAudioSuppression",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

void RecordUmaCancellation(DialogType dialog_type,
                           base::TimeTicks dialog_open_time) {
  RecordAction(base::UserMetricsAction("GetDisplayMedia.Cancel"));
  if (dialog_type == DialogType::kPreferCurrentTab) {
    RecordUma(GDMPreferCurrentTabResult::kUserCancelled, dialog_open_time);
  } else {
    RecordUma(GDMResult::kUserCancelled, dialog_open_time);
  }
}

// Convenience function for recording UMA.
// |source_type| is there to help us distinguish the current tab being
// selected explicitly, from it being selected from the list of all tabs.
void RecordUmaSelection(DialogType dialog_type,
                        content::GlobalRenderFrameHostId capturer_global_id,
                        const DesktopMediaID& selected_media,
                        DesktopMediaList::Type source_type,
                        base::TimeTicks dialog_open_time) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  switch (source_type) {
    case DesktopMediaList::Type::kNone:
      NOTREACHED();

    case DesktopMediaList::Type::kScreen:
      RecordAction(base::UserMetricsAction("GetDisplayMedia.SelectScreen"));
      if (dialog_type == DialogType::kPreferCurrentTab) {
        RecordUma(GDMPreferCurrentTabResult::kUserSelectedScreen,
                  dialog_open_time);
      } else {
        RecordUma(GDMResult::kUserSelectedScreen, dialog_open_time);
      }
      break;

    case DesktopMediaList::Type::kWindow:
      RecordAction(base::UserMetricsAction("GetDisplayMedia.SelectWindow"));
      if (dialog_type == DialogType::kPreferCurrentTab) {
        RecordUma(GDMPreferCurrentTabResult::kUserSelectedWindow,
                  dialog_open_time);
      } else {
        RecordUma(GDMResult::kUserSelectedWindow, dialog_open_time);
      }
      break;

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

      if (dialog_type == DialogType::kPreferCurrentTab) {
        RecordUma(
            current_tab_selected
                ? GDMPreferCurrentTabResult::kUserSelectedThisTabAsGenericTab
                : GDMPreferCurrentTabResult::kUserSelectedOtherTab,
            dialog_open_time);
      } else {
        RecordUma(current_tab_selected ? GDMResult::kUserSelectedThisTab
                                       : GDMResult::kUserSelectedOtherTab,
                  dialog_open_time);
      }
      break;
    }

    case DesktopMediaList::Type::kCurrentTab:
      RecordAction(base::UserMetricsAction("GetDisplayMedia.SelectCurrentTab"));
      RecordUma(GDMPreferCurrentTabResult::kUserSelectedThisTab,
                dialog_open_time);
      break;
  }
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

bool AreEquivalentTypesForAudioCheckbox(DesktopMediaList::Type lhs,
                                        DesktopMediaList::Type rhs) {
  if (lhs == DesktopMediaList::Type::kWebContents ||
      lhs == DesktopMediaList::Type::kCurrentTab) {
    return rhs == DesktopMediaList::Type::kWebContents ||
           rhs == DesktopMediaList::Type::kCurrentTab;
  } else {
    return lhs == rhs;
  }
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
      break;
    case DesktopMediaList::Type::kScreen:
      return display_surface == blink::mojom::PreferredDisplaySurface::MONITOR;
    case DesktopMediaList::Type::kWindow:
      return display_surface == blink::mojom::PreferredDisplaySurface::WINDOW;
    case DesktopMediaList::Type::kWebContents:
    case DesktopMediaList::Type::kCurrentTab:
      return display_surface == blink::mojom::PreferredDisplaySurface::BROWSER;
  }
  NOTREACHED();
}

std::unique_ptr<views::ScrollView> CreateScrollView(bool audio_requested) {
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetBackgroundThemeColorId(ui::kColorSysSurface4);
  // The overflow indicator is disabled to reduce clutter next to the
  // separator to the audio control when audio is requested or the bottom of
  // the dialog when audio is not requested.
  scroll_view->SetDrawOverflowIndicator(false);
  return scroll_view;
}

}  // namespace

// Enable an updated dialog UI for the getDisplayMedia picker dialog under the
// preferCurrentTab constraint.
BASE_FEATURE(kShareThisTabDialog,
             "ShareThisTabDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool DesktopMediaPickerDialogView::AudioSupported(DesktopMediaList::Type type) {
  switch (type) {
    case DesktopMediaList::Type::kScreen:
      return DesktopMediaPickerController::IsSystemAudioCaptureSupported(
          request_source_);
    case DesktopMediaList::Type::kWindow:
      return false;
    case DesktopMediaList::Type::kWebContents:
    case DesktopMediaList::Type::kCurrentTab:
      return true;
    case DesktopMediaList::Type::kNone:
      break;
  }
  NOTREACHED();
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
    DesktopMediaPickerViews* parent,
    std::vector<std::unique_ptr<DesktopMediaList>> source_lists)
    : web_contents_(params.web_contents),
      request_source_(params.request_source),
      app_name_(params.app_name),
      audio_requested_(params.request_audio),
      suppress_local_audio_playback_(params.suppress_local_audio_playback),
      is_system_audio_offered_(audio_requested_ &&
                               !params.exclude_system_audio &&
                               AudioSupported(DesktopMediaList::Type::kScreen)),
      capturer_global_id_(
          params.web_contents
              ? params.web_contents->GetPrimaryMainFrame()->GetGlobalId()
              : content::GlobalRenderFrameHostId()),
      parent_(parent),
      dialog_open_time_(base::TimeTicks::Now()) {
  DCHECK(!params.force_audio_checkboxes_to_default_checked ||
         !params.exclude_system_audio);
  RecordAction(base::UserMetricsAction("GetDisplayMedia.ShowDialog"));

#if BUILDFLAG(IS_MAC)
  screen_capture_permission_checker_ =
      ScreenCapturePermissionChecker::MaybeCreate(
          base::BindRepeating(&DesktopMediaPickerDialogView::OnPermissionUpdate,
                              weak_factory_.GetWeakPtr()));
#endif

  SetModalType(params.modality);
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_DESKTOP_MEDIA_PICKER_SHARE));
  SetButtonStyle(ui::mojom::DialogButton::kCancel, ui::ButtonStyle::kTonal);
  RegisterDeleteDelegateCallback(base::BindOnce(
      [](DesktopMediaPickerDialogView* dialog) {
        // If the dialog is being closed then notify the parent about it.
        if (dialog->parent_)
          dialog->parent_->NotifyDialogResult(DesktopMediaID());
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
  description_label->SetEnabledColorId(
      kColorDesktopMediaPickerDescriptionLabel);
  description_label_ = AddChildView(std::move(description_label));

  std::vector<std::pair<std::u16string, std::unique_ptr<View>>> panes;

  const bool current_tab_among_sources =
      base::Contains(source_lists, DesktopMediaList::Type::kCurrentTab,
                     &DesktopMediaList::GetMediaListType);

  dialog_type_ = current_tab_among_sources ? DialogType::kPreferCurrentTab
                                           : DialogType::kStandard;

  // This command-line switch takes precedence over
  // params.force_audio_checkboxes_to_default_checked.
  const bool screen_capture_audio_default_unchecked =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kScreenCaptureAudioDefaultUnchecked);

  for (auto& source_list : source_lists) {
    switch (source_list->GetMediaListType()) {
      case DesktopMediaList::Type::kNone:
        NOTREACHED();
      case DesktopMediaList::Type::kScreen: {
        const DesktopMediaSourceViewStyle kGenericScreenStyle =
            DesktopMediaSourceViewStyle(
                /*columns=*/2,
                /*item_size=*/gfx::Size(266, 224),
                /*icon_rect=*/gfx::Rect(),
                /*label_rect=*/gfx::Rect(8, 196, 250, 36),
                /*text_alignment=*/gfx::HorizontalAlignment::ALIGN_CENTER,
                /*image_rect=*/gfx::Rect(8, 8, 250, 180));

        const DesktopMediaSourceViewStyle kSingleScreenStyle =
            kGenericScreenStyle;

        std::unique_ptr<views::ScrollView> screen_scroll_view =
            CreateScrollView(audio_requested_);
        std::u16string screen_title_text = l10n_util::GetStringUTF16(
            IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_SCREEN);
        auto list_controller = std::make_unique<DesktopMediaListController>(
            this, std::move(source_list));
        const bool supports_reselect_button =
            list_controller->SupportsReselectButton();
        screen_scroll_view->SetContents(list_controller->CreateView(
            kGenericScreenStyle, kSingleScreenStyle, screen_title_text,
            DesktopMediaList::Type::kScreen));
        // If the DisplayMediaPickerRedesign flag is active, clip max height to
        // 1.5 item heights to allow space for the audio-toggle controller.
        screen_scroll_view->ClipHeightTo(
            kGenericScreenStyle.item_size.height(),
            kGenericScreenStyle.item_size.height() * 3 / 2);
        screen_scroll_view->SetHorizontalScrollBarMode(
            views::ScrollView::ScrollBarMode::kDisabled);

        std::unique_ptr<views::View> pane = SetupPane(
            DesktopMediaList::Type::kScreen, std::move(list_controller),
            /*audio_offered=*/is_system_audio_offered_,
            /*audio_checked=*/
            params.force_audio_checkboxes_to_default_checked &&
                !screen_capture_audio_default_unchecked,
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
            /*audio_offered=*/
            AudioSupported(DesktopMediaList::Type::kWindow),
            /*audio_checked=*/
            params.force_audio_checkboxes_to_default_checked &&
                !screen_capture_audio_default_unchecked,
            supports_reselect_button, std::move(window_scroll_view));
        panes.emplace_back(window_title_text, std::move(pane));
        break;
      }
      case DesktopMediaList::Type::kWebContents: {
        // Note that "other tab" is inaccurate - we actually allow any tab
        // to be selected in either case.
        const std::u16string title = l10n_util::GetStringUTF16(
            current_tab_among_sources
                ? IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_OTHER_TAB
                : IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_TAB);
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
            /*audio_checked=*/!screen_capture_audio_default_unchecked,
            supports_reselect_button, std::move(list_view));
        panes.emplace_back(title, std::move(pane));
        break;
      }
      case DesktopMediaList::Type::kCurrentTab: {
        const DesktopMediaSourceViewStyle kCurrentTabStyle(
            /*columns=*/1,
            /*item_size=*/gfx::Size(360, 280),
            /*icon_rect=*/gfx::Rect(),
            /*label_rect=*/gfx::Rect(),
            /*text_alignment=*/gfx::HorizontalAlignment::ALIGN_CENTER,
            /*image_rect=*/gfx::Rect(20, 20, 320, 240));
        std::unique_ptr<views::ScrollView> window_scroll_view =
            CreateScrollView(audio_requested_);
        const std::u16string title = l10n_util::GetStringUTF16(
            IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_THIS_TAB);
        auto list_controller = std::make_unique<DesktopMediaListController>(
            this, std::move(source_list));
        const bool supports_reselect_button =
            list_controller->SupportsReselectButton();
        window_scroll_view->SetContents(list_controller->CreateView(
            kCurrentTabStyle, kCurrentTabStyle, title,
            DesktopMediaList::Type::kCurrentTab));
        window_scroll_view->ClipHeightTo(
            kCurrentTabStyle.item_size.height(),
            kCurrentTabStyle.item_size.height() * 2);
        window_scroll_view->SetHorizontalScrollBarMode(
            views::ScrollView::ScrollBarMode::kDisabled);
        std::unique_ptr<views::View> pane = SetupPane(
            DesktopMediaList::Type::kCurrentTab, std::move(list_controller),

            /*audio_offered=*/
            AudioSupported(DesktopMediaList::Type::kWebContents),
            /*audio_checked=*/!screen_capture_audio_default_unchecked,
            supports_reselect_button, std::move(window_scroll_view));
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
    tabbed_pane->set_listener(this);
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

  DCHECK(!categories_.empty());

  if (params.restricted_by_policy) {
    AddChildView(CreatePolicyRestrictedView());
  }

  previously_selected_category_ = GetSelectedTabIndex();
  ConfigureUIForNewPane(previously_selected_category_);

  // If |params.web_contents| is set and it's not a background page then the
  // picker will be shown modal to the web contents. Otherwise the picker is
  // shown in a separate window.
  views::Widget* widget = nullptr;
  bool modal_dialog = params.web_contents &&
                      !params.web_contents->GetDelegate()->IsNeverComposited(
                          params.web_contents);
  if (modal_dialog) {
    Browser* browser = chrome::FindBrowserWithTab(params.web_contents);
    // Close the extension popup to prevent spoofing.
    if (browser && browser->window() &&
        browser->window()->GetExtensionsContainer()) {
      browser->window()->GetExtensionsContainer()->HideActivePopup();
    }
    widget =
        constrained_window::ShowWebModalDialogViews(this, params.web_contents);
  } else {
#if BUILDFLAG(IS_MAC)
    // On Mac, ModalType::kChild with a null parent isn't allowed - fall back to
    // ModalType::kWindow.
    SetModalType(ui::mojom::ModalType::kWindow);
#endif
    widget = CreateDialogWidget(this, params.context, nullptr);
    widget->Show();
  }

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

#if !BUILDFLAG(IS_CHROMEOS_ASH) && defined(USE_AURA)
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
  if (dialog_type_ == DialogType::kPreferCurrentTab) {
    RecordUma(GDMPreferCurrentTabResult::kDialogDismissed, dialog_open_time_);
  } else {
    RecordUma(GDMResult::kDialogDismissed, dialog_open_time_);
  }
}

void DesktopMediaPickerDialogView::TabSelectedAt(int index) {
  if (previously_selected_category_ == index)
    return;
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
  bool has_audio_control =
      prev_category.pane && prev_category.pane->AudioOffered();
  if (!has_audio_control || !prev_category.audio_offered) {
    return;
  }

  // Store pre-change audio control state.
  // Note: Current-tab and and any-tab are both tab-based captures,
  // and therefore share their audio control's state.
  const bool checked =
      prev_category.pane && prev_category.pane->IsAudioSharingApprovedByUser();
  for (auto& category : categories_) {
    if (AreEquivalentTypesForAudioCheckbox(category.type, prev_category.type)) {
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

std::u16string DesktopMediaPickerDialogView::GetLabelForAudioToggle(
    const DisplaySurfaceCategory& category) const {
  if (!category.audio_offered) {
    return l10n_util::GetStringUTF16(
        is_system_audio_offered_
            ? IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB_OR_SCREEN
            : IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB);
  }

  switch (category.type) {
    case DesktopMediaList::Type::kScreen: {
      bool show_warning = suppress_local_audio_playback_ &&
                          base::FeatureList::IsEnabled(
                              kWarnUserOfSystemWideLocalAudioSuppression);
      if (request_source_ == RequestSource::kGetDisplayMedia &&
          !base::FeatureList::IsEnabled(
              ::kSuppressLocalAudioPlaybackForSystemAudio)) {
        // Suppression blocked by killswitch, so no need to show a warning.
        show_warning = false;
      }
      return l10n_util::GetStringUTF16(
          show_warning
              ? IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_SCREEN_WITH_MUTE_WARNING
              : IDS_DESKTOP_MEDIA_PICKER_ALSO_SHARE_SYSTEM_AUDIO);
    }
    case DesktopMediaList::Type::kWindow:
      NOTREACHED();
    case DesktopMediaList::Type::kWebContents:
    case DesktopMediaList::Type::kCurrentTab:
      return l10n_util::GetStringUTF16(
          IDS_DESKTOP_MEDIA_PICKER_ALSO_SHARE_TAB_AUDIO);
    case DesktopMediaList::Type::kNone:
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
  auto share_audio_view =
      audio_requested_
          ? std::make_unique<ShareAudioView>(GetLabelForAudioToggle(category),
                                             category.audio_offered)
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

std::u16string DesktopMediaPickerDialogView::GetWindowTitle() const {
  if (request_source_ == RequestSource::kGetDisplayMedia) {
    return l10n_util::GetStringFUTF16(IDS_DISPLAY_MEDIA_PICKER_TITLE,
                                      app_name_);
  }

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
  DCHECK(IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  // Ok button should only be enabled when a source is selected.
  std::optional<DesktopMediaID> source_optional =
      accepted_source_.has_value() ? accepted_source_
                                   : GetSelectedController()->GetSelection();
  DesktopMediaID source = source_optional.value();
  source.audio_share = IsAudioSharingApprovedByUser();
  if (request_source_ == RequestSource::kGetDisplayMedia) {
    RecordUmaSelection(dialog_type_, capturer_global_id_, source,
                       GetSelectedSourceListType(), dialog_open_time_);
  }
  RecordSourceCountsUma();
  RecordTabDiscardedStatusUma(source);

  if (parent_)
    parent_->NotifyDialogResult(source);

  // Return true to close the window.
  return true;
}

bool DesktopMediaPickerDialogView::Cancel() {
  if (request_source_ == RequestSource::kGetDisplayMedia) {
    RecordUmaCancellation(dialog_type_, dialog_open_time_);
  }
  RecordSourceCountsUma();

  return views::DialogDelegateView::Cancel();
}

bool DesktopMediaPickerDialogView::ShouldShowCloseButton() const {
  return false;
}

void DesktopMediaPickerDialogView::OnWidgetInitialized() {
  views::DialogDelegateView::OnWidgetInitialized();
}

void DesktopMediaPickerDialogView::OnSelectionChanged() {
  DialogModelChanged();
}

void DesktopMediaPickerDialogView::AcceptSource() {
  // This will call Accept() and close the dialog.
  AcceptDialog();
}

void DesktopMediaPickerDialogView::AcceptSpecificSource(
    const DesktopMediaID& source) {
  accepted_source_ = std::optional<DesktopMediaID>(source);
  AcceptSource();
}

void DesktopMediaPickerDialogView::Reject() {
  RecordSourceCountsUma();
  CancelDialog();
}

void DesktopMediaPickerDialogView::OnSourceListLayoutChanged() {
  PreferredSizeChanged();
  // TODO(pbos): Ideally this would use shared logic similar to
  // BubbleDialogDelegateView::SizeToContents() instead of implementing sizing
  // logic in-place.
  const gfx::Size new_size = GetWidget()->GetRootView()->GetPreferredSize();
  if (GetModalType() == ui::mojom::ModalType::kChild) {
    // For the web-modal dialog resize the dialog in place.
    // TODO(pbos): This should ideally use UpdateWebContentsModalDialogPosition
    // to keep the widget centered horizontally. As this dialog is fixed-width
    // we're effectively only changing the height, so reusing the current
    // widget origin should be equivalent.
    GetWidget()->SetSize(new_size);
    return;
  }

  // When not using the web-modal dialog, center the dialog with its new size.
  GetWidget()->CenterWindow(new_size);
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
      base::ranges::find(categories_, DesktopMediaList::Type::kWebContents,
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
  if (controller != GetSelectedController() || !reselect_button_)
    return;

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
#endif

BEGIN_METADATA(DesktopMediaPickerDialogView)
END_METADATA

DesktopMediaPickerViews::DesktopMediaPickerViews() : dialog_(nullptr) {}

DesktopMediaPickerViews::~DesktopMediaPickerViews() {
  if (dialog_) {
    if (request_source_ == RequestSource::kGetDisplayMedia) {
      dialog_->RecordUmaDismissal();
    }
    dialog_->DetachParent();
    dialog_->GetWidget()->Close();
  }
}

void DesktopMediaPickerViews::Show(
    const DesktopMediaPicker::Params& params,
    std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
    DoneCallback done_callback) {
  DesktopMediaPickerManager::Get()->OnShowDialog(params);

  request_source_ = params.request_source;
  callback_ = std::move(done_callback);
  dialog_ =
      new DesktopMediaPickerDialogView(params, this, std::move(source_lists));
}

void DesktopMediaPickerViews::NotifyDialogResult(const DesktopMediaID& source) {
  // Once this method is called the |dialog_| will close and destroy itself.
  dialog_->DetachParent();
  dialog_ = nullptr;

  DesktopMediaPickerManager::Get()->OnHideDialog();

  if (callback_.is_null())
    return;

  // Notify the |callback_| asynchronously because it may need to destroy
  // DesktopMediaPicker.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), source));
}

// static
std::unique_ptr<DesktopMediaPicker> DesktopMediaPicker::Create(
    const content::MediaStreamRequest* request) {
  if (base::FeatureList::IsEnabled(kShareThisTabDialog) && request &&
      request->video_type ==
          blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB) {
    return std::make_unique<ShareThisTabDialogViews>();
  } else {
    return std::make_unique<DesktopMediaPickerViews>();
  }
}
