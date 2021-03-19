// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_source_view.h"
#include "chrome/browser/ui/views/desktop_capture/get_current_browsing_context_media_dialog.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window_tree_host.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/projector_session.h"
#endif

using content::DesktopMediaID;

enum class DesktopMediaPickerDialogView::DialogSource : int {
  kGetCurrentBrowsingContextMedia = 0,
  kGetDisplayMedia = 1
};

namespace {

using DialogSource = DesktopMediaPickerDialogView::DialogSource;

#if !BUILDFLAG(IS_CHROMEOS_ASH) && defined(USE_AURA)
DesktopMediaID::Id AcceleratedWidgetToDesktopMediaId(
    gfx::AcceleratedWidget accelerated_widget) {
#if defined(OS_WIN)
  return reinterpret_cast<DesktopMediaID::Id>(accelerated_widget);
#else
  return static_cast<DesktopMediaID::Id>(accelerated_widget);
#endif
}
#endif

enum class GCBCMResult {
  kDialogDismissed = 0,                  // Tab/window closed, navigation, etc.
  kUserCancelled = 1,                    // User explicitly cancelled.
  kUserSelectedScreen = 2,               // Screen selected.
  kUserSelectedWindow = 3,               // Window selected.
  kUserSelectedOtherTab = 4,             // Other tab selected from tab-list.
  kUserSelectedThisTabAsGenericTab = 5,  // Current tab selected from tab-list.
  kUserSelectedThisTab = 6,  // Current tab selected from current-tab menu.
  kMaxValue = kUserSelectedThisTab
};

enum class GDMResult {
  kDialogDismissed = 0,       // Tab/window closed, navigation, etc.
  kUserCancelled = 1,         // User explicitly cancelled.
  kUserSelectedScreen = 2,    // Screen selected.
  kUserSelectedWindow = 3,    // Window selected.
  kUserSelectedOtherTab = 4,  // Other tab selected from tab-list.
  kUserSelectedThisTab = 5,   // Current tab selected from tab-list.
  kMaxValue = kUserSelectedThisTab
};

void RecordUma(GCBCMResult result) {
  base::UmaHistogramEnumeration(
      "Media.Ui.GetCurrentBrowsingContextMedia.ExplicitSelection."
      "UserInteraction",
      result);
}

void RecordUma(GDMResult result) {
  base::UmaHistogramEnumeration("Media.Ui.GetDisplayMedia.UserInteraction",
                                result);
}

void RecordUmaDismissal(DialogSource dialog_source) {
  if (dialog_source == DialogSource::kGetCurrentBrowsingContextMedia) {
    RecordUma(GCBCMResult::kDialogDismissed);
  } else {
    RecordUma(GDMResult::kDialogDismissed);
  }
}

void RecordUmaCancellation(DialogSource dialog_source) {
  if (dialog_source == DialogSource::kGetCurrentBrowsingContextMedia) {
    RecordUma(GCBCMResult::kUserCancelled);
  } else {
    RecordUma(GDMResult::kUserCancelled);
  }
}

// Convenience function for recording UMA.
// |source_type| is there to help us distinguish the current tab being
// selected explicitly, from it being selected from the list of all tabs.
void RecordUmaSelection(DialogSource dialog_source,
                        content::WebContents* web_contents,
                        const DesktopMediaID& selected_media,
                        DesktopMediaList::Type source_type) {
  switch (source_type) {
    case DesktopMediaList::Type::kNone: {
      NOTREACHED();
      break;
    }

    case DesktopMediaList::Type::kScreen: {
      if (dialog_source == DialogSource::kGetCurrentBrowsingContextMedia) {
        RecordUma(GCBCMResult::kUserSelectedScreen);
      } else {
        RecordUma(GDMResult::kUserSelectedScreen);
      }
      break;
    }

    case DesktopMediaList::Type::kWindow: {
      if (dialog_source == DialogSource::kGetCurrentBrowsingContextMedia) {
        RecordUma(GCBCMResult::kUserSelectedWindow);
      } else {
        RecordUma(GDMResult::kUserSelectedWindow);
      }
      break;
    }

    case DesktopMediaList::Type::kWebContents: {
      // Whether the current tab was selected. Note that this can happen
      // through a non-explicit selection of the current tab through the
      // list of all available tabs.
      const bool current_tab_selected =
          web_contents &&
          web_contents->GetMainFrame()->GetProcess()->GetID() ==
              selected_media.web_contents_id.render_process_id &&
          web_contents->GetMainFrame()->GetRoutingID() ==
              selected_media.web_contents_id.main_render_frame_id;

      if (dialog_source == DialogSource::kGetCurrentBrowsingContextMedia) {
        RecordUma(current_tab_selected
                      ? GCBCMResult::kUserSelectedThisTabAsGenericTab
                      : GCBCMResult::kUserSelectedOtherTab);
      } else {
        RecordUma(current_tab_selected ? GDMResult::kUserSelectedThisTab
                                       : GDMResult::kUserSelectedOtherTab);
      }
      break;
    }

    case DesktopMediaList::Type::kCurrentTab: {
      RecordUma(GCBCMResult::kUserSelectedThisTab);
      break;
    }
  }
}

bool IsProjectorEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!ash::ProjectorController::Get())
    return false;

  // TODO(https://crbug.com/1184881): If there is a session ongoing we should
  // disable the checkbox.
  return ash::ProjectorController::Get()->IsEligible();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
}

void SetProjectorToolsVisible(bool is_visible) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ProjectorController::Get()->SetProjectorToolsVisible(is_visible);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ProjectorCheckboxPressed(views::Checkbox* presenter_checkbox) {
  DCHECK(presenter_checkbox);
  SetProjectorToolsVisible(presenter_checkbox->GetChecked());
}

void StartProjectorSession(const content::DesktopMediaID& accepted_id) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (accepted_id.type == content::DesktopMediaID::TYPE_NONE)
    return;

  ash::SourceType scope;

  switch (accepted_id.type) {
    case content::DesktopMediaID::TYPE_SCREEN:
      scope = ash::SourceType::kFullscreen;
      break;
    case content::DesktopMediaID::TYPE_WINDOW:
      scope = ash::SourceType::kWindow;
      break;
    case content::DesktopMediaID::TYPE_WEB_CONTENTS:
      scope = ash::SourceType::kTab;
      break;
    case content::DesktopMediaID::TYPE_NONE:
      scope = ash::SourceType::kUnset;
      break;
  }

  ash::ProjectorController::Get()->StartProjectorSession(
      scope, content::DesktopMediaID::GetNativeWindowById(accepted_id));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

DesktopMediaPickerDialogView::DesktopMediaPickerDialogView(
    const DesktopMediaPicker::Params& params,
    DesktopMediaPickerViews* parent,
    std::vector<std::unique_ptr<DesktopMediaList>> source_lists)
    : web_contents_(params.web_contents), parent_(parent) {
  SetModalType(params.modality);
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_DESKTOP_MEDIA_PICKER_SHARE));
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  std::unique_ptr<views::View> extra_views_container;
  if (params.request_audio || IsProjectorEnabled()) {
    extra_views_container = std::make_unique<views::View>();
    // A simple horizontal layout manager.
    extra_views_container->SetLayoutManager(
        std::make_unique<views::BoxLayout>());

    if (params.request_audio) {
      std::unique_ptr<views::Checkbox> audio_share_checkbox =
          std::make_unique<views::Checkbox>(
              l10n_util::GetStringUTF16(IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE));
      audio_share_checkbox->SetChecked(params.approve_audio_by_default);
      audio_share_checkbox_ =
          extra_views_container->AddChildView(std::move(audio_share_checkbox));
    }

    if (IsProjectorEnabled()) {
      std::unique_ptr<views::Checkbox> presenter_tools_checkbox =
          std::make_unique<views::Checkbox>(
              l10n_util::GetStringUTF16(IDS_DESKTOP_MEDIA_PRESENTER_TOOLS));

      presenter_tools_checkbox_ = extra_views_container->AddChildView(
          std::move(presenter_tools_checkbox));
      presenter_tools_checked_subscription_ =
          presenter_tools_checkbox_->AddCheckedChangedCallback(
              base::BindRepeating(&ProjectorCheckboxPressed,
                                  presenter_tools_checkbox_));
      presenter_tools_checkbox_->SetChecked(true);
    }

    extra_views_container_ = SetExtraView(std::move(extra_views_container));
    extra_views_container_->SetVisible(true);
  }

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetDialogInsetsForContentType(views::TEXT, views::CONTROL),
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL)));

  auto description_label = std::make_unique<views::Label>();
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_label_ = AddChildView(std::move(description_label));

  std::vector<std::pair<std::u16string, std::unique_ptr<View>>> panes;

  const bool current_tab_among_sources = std::any_of(
      source_lists.begin(), source_lists.end(),
      [](const std::unique_ptr<DesktopMediaList>& list) {
        return list->GetMediaListType() == DesktopMediaList::Type::kCurrentTab;
      });

  dialog_source_ = current_tab_among_sources
                       ? DialogSource::kGetCurrentBrowsingContextMedia
                       : DialogSource::kGetDisplayMedia;

  for (auto& source_list : source_lists) {
    switch (source_list->GetMediaListType()) {
      case DesktopMediaList::Type::kNone: {
        NOTREACHED();
        break;
      }
      case DesktopMediaList::Type::kScreen: {
        source_types_.push_back(DesktopMediaList::Type::kScreen);

        const DesktopMediaSourceViewStyle kSingleScreenStyle(
            1,                                       // columns
            gfx::Size(360, 280),                     // item_size
            gfx::Rect(),                             // icon_rect
            gfx::Rect(),                             // label_rect
            gfx::HorizontalAlignment::ALIGN_CENTER,  // text_alignment
            gfx::Rect(20, 20, 320, 240),             // image_rect
            5);                                      // focus_rectangle_inset

        const DesktopMediaSourceViewStyle kGenericScreenStyle(
            2,                                       // columns
            gfx::Size(270, 220),                     // item_size
            gfx::Rect(),                             // icon_rect
            gfx::Rect(15, 165, 240, 40),             // label_rect
            gfx::HorizontalAlignment::ALIGN_CENTER,  // text_alignment
            gfx::Rect(15, 15, 240, 150),             // image_rect
            5);                                      // focus_rectangle_inset

        std::unique_ptr<views::ScrollView> screen_scroll_view =
            views::ScrollView::CreateScrollViewWithBorder();
        std::u16string screen_title_text = l10n_util::GetStringUTF16(
            IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_SCREEN);
        auto list_controller = std::make_unique<DesktopMediaListController>(
            this, std::move(source_list));
        screen_scroll_view->SetContents(list_controller->CreateView(
            kGenericScreenStyle, kSingleScreenStyle, screen_title_text));
        list_controllers_.push_back(std::move(list_controller));

        screen_scroll_view->ClipHeightTo(
            kGenericScreenStyle.item_size.height(),
            kGenericScreenStyle.item_size.height() * 2);
        screen_scroll_view->SetHorizontalScrollBarMode(
            views::ScrollView::ScrollBarMode::kDisabled);

        panes.push_back(
            std::make_pair(screen_title_text, std::move(screen_scroll_view)));
        break;
      }
      case DesktopMediaList::Type::kWindow: {
        source_types_.push_back(DesktopMediaList::Type::kWindow);

        const DesktopMediaSourceViewStyle kWindowStyle(
            3,                                     // columns
            gfx::Size(180, 160),                   // item_size
            gfx::Rect(10, 120, 20, 20),            // icon_rect
            gfx::Rect(32, 110, 138, 40),           // label_rect
            gfx::HorizontalAlignment::ALIGN_LEFT,  // text_alignment
            gfx::Rect(8, 8, 164, 104),             // image_rect
            5);                                    // focus_rectangle_inset

        std::unique_ptr<views::ScrollView> window_scroll_view =
            views::ScrollView::CreateScrollViewWithBorder();
        std::u16string window_title_text = l10n_util::GetStringUTF16(
            IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_WINDOW);
        auto list_controller = std::make_unique<DesktopMediaListController>(
            this, std::move(source_list));
        window_scroll_view->SetContents(list_controller->CreateView(
            kWindowStyle, kWindowStyle, window_title_text));
        list_controllers_.push_back(std::move(list_controller));

        window_scroll_view->ClipHeightTo(kWindowStyle.item_size.height(),
                                         kWindowStyle.item_size.height() * 2);
        window_scroll_view->SetHorizontalScrollBarMode(
            views::ScrollView::ScrollBarMode::kDisabled);

        panes.push_back(
            std::make_pair(window_title_text, std::move(window_scroll_view)));
        break;
      }
      case DesktopMediaList::Type::kWebContents: {
        source_types_.push_back(DesktopMediaList::Type::kWebContents);
        // Note that "other tab" is inaccurate - we actually allow any tab
        // to be selected in either case.
        const std::u16string title = l10n_util::GetStringUTF16(
            current_tab_among_sources
                ? IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_OTHER_TAB
                : IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_TAB);
        auto list_controller = std::make_unique<DesktopMediaListController>(
            this, std::move(source_list));
        panes.push_back(
            std::make_pair(title, list_controller->CreateTabListView(title)));
        list_controllers_.push_back(std::move(list_controller));
        break;
      }
      case DesktopMediaList::Type::kCurrentTab: {
        source_types_.push_back(DesktopMediaList::Type::kCurrentTab);
        const DesktopMediaSourceViewStyle kCurrentTabStyle(
            1,                                       // columns
            gfx::Size(360, 280),                     // item_size
            gfx::Rect(),                             // icon_rect
            gfx::Rect(),                             // label_rect
            gfx::HorizontalAlignment::ALIGN_CENTER,  // text_alignment
            gfx::Rect(20, 20, 320, 240),             // image_rect
            5);                                      // focus_rectangle_inset
        std::unique_ptr<views::ScrollView> window_scroll_view =
            views::ScrollView::CreateScrollViewWithBorder();
        const std::u16string title = l10n_util::GetStringUTF16(
            IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_THIS_TAB);
        auto list_controller = std::make_unique<DesktopMediaListController>(
            this, std::move(source_list));
        window_scroll_view->SetContents(list_controller->CreateView(
            kCurrentTabStyle, kCurrentTabStyle, title));
        list_controllers_.push_back(std::move(list_controller));
        window_scroll_view->ClipHeightTo(
            kCurrentTabStyle.item_size.height(),
            kCurrentTabStyle.item_size.height() * 2);
        window_scroll_view->SetHorizontalScrollBarMode(
            views::ScrollView::ScrollBarMode::kDisabled);
        panes.emplace_back(title, std::move(window_scroll_view));
        break;
      }
    }
  }

  if (panes.size() > 1) {
    auto tabbed_pane = std::make_unique<views::TabbedPane>();
    for (auto& pane : panes)
      tabbed_pane->AddTab(pane.first, std::move(pane.second));
    tabbed_pane->set_listener(this);
    tabbed_pane->SetFocusBehavior(views::View::FocusBehavior::NEVER);
    tabbed_pane_ = AddChildView(std::move(tabbed_pane));
  } else {
    AddChildView(std::move(panes.front().second));
  }

  if (params.app_name == params.target_name) {
    description_label_->SetText(l10n_util::GetStringFUTF16(
        IDS_DESKTOP_MEDIA_PICKER_TEXT, params.app_name));
  } else {
    description_label_->SetText(
        l10n_util::GetStringFUTF16(IDS_DESKTOP_MEDIA_PICKER_TEXT_DELEGATED,
                                   params.app_name, params.target_name));
  }

  DCHECK(!source_types_.empty());

  OnSourceTypeSwitched(0);

  // If |params.web_contents| is set and it's not a background page then the
  // picker will be shown modal to the web contents. Otherwise the picker is
  // shown in a separate window.
  views::Widget* widget = nullptr;
  bool modal_dialog = params.web_contents &&
                      !params.web_contents->GetDelegate()->IsNeverComposited(
                          params.web_contents);
  if (modal_dialog) {
    widget =
        constrained_window::ShowWebModalDialogViews(this, params.web_contents);
  } else {
#if defined(OS_MAC)
    // On Mac, MODAL_TYPE_CHILD with a null parent isn't allowed - fall back to
    // MODAL_TYPE_WINDOW.
    SetModalType(ui::MODAL_TYPE_WINDOW);
#endif
    widget = CreateDialogWidget(this, params.context, nullptr);
    widget->Show();
  }
  chrome::RecordDialogCreation(chrome::DialogIdentifier::DESKTOP_MEDIA_PICKER);

#if defined(OS_MAC)
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
#elif defined(OS_MAC)
    // On Mac, the window_id in DesktopMediaID is the same as the actual native
    // window ID. Note that assuming this is a bit of a layering violation; the
    // fact that this code makes that assumption is documented at the code that
    // causes it to hold, so hopefully nobody changes that :)
    dialog_window_id.id = dialog_window_id.window_id;
#endif
  }

  for (const auto& list_controller : list_controllers_)
    list_controller->StartUpdating(dialog_window_id);
}

DesktopMediaPickerDialogView::~DesktopMediaPickerDialogView() {}

DialogSource DesktopMediaPickerDialogView::GetDialogSource() const {
  return dialog_source_;
}

void DesktopMediaPickerDialogView::TabSelectedAt(int index) {
  OnSourceTypeSwitched(index);
  list_controllers_[index]->FocusView();
  DialogModelChanged();
}

void DesktopMediaPickerDialogView::OnSourceTypeSwitched(int index) {
  // Set whether the checkbox is visible based on the source type.
  if (audio_share_checkbox_) {
    switch (source_types_[index]) {
      case DesktopMediaList::Type::kScreen:
        audio_share_checkbox_->SetVisible(
            DesktopMediaPickerViews::kScreenAudioShareSupportedOnPlatform);
        break;
      case DesktopMediaList::Type::kWindow:
        audio_share_checkbox_->SetVisible(false);
        break;
      case DesktopMediaList::Type::kWebContents:
      case DesktopMediaList::Type::kCurrentTab:
        audio_share_checkbox_->SetVisible(true);
        break;
      case DesktopMediaList::Type::kNone:
        NOTREACHED();
        break;
    }
  }
}

int DesktopMediaPickerDialogView::GetSelectedTabIndex() const {
  return tabbed_pane_ ? tabbed_pane_->GetSelectedTabIndex() : 0;
}

const DesktopMediaListController*
DesktopMediaPickerDialogView::GetSelectedController() const {
  return list_controllers_[GetSelectedTabIndex()].get();
}

DesktopMediaListController*
DesktopMediaPickerDialogView::GetSelectedController() {
  return list_controllers_[GetSelectedTabIndex()].get();
}

DesktopMediaList::Type DesktopMediaPickerDialogView::GetSelectedSourceListType()
    const {
  const int index = GetSelectedTabIndex();
  DCHECK_GE(index, 0);
  DCHECK_LT(static_cast<size_t>(index), source_types_.size());
  return source_types_[index];
}

void DesktopMediaPickerDialogView::DetachParent() {
  parent_ = nullptr;
}

gfx::Size DesktopMediaPickerDialogView::CalculatePreferredSize() const {
  static const size_t kDialogViewWidth = 600;
  return gfx::Size(kDialogViewWidth, GetHeightForWidth(kDialogViewWidth));
}

std::u16string DesktopMediaPickerDialogView::GetWindowTitle() const {
  int title_id = IDS_DESKTOP_MEDIA_PICKER_TITLE;

  if (!tabbed_pane_) {
    switch (source_types_.front()) {
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
    ui::DialogButton button) const {
  return button != ui::DIALOG_BUTTON_OK ||
         GetSelectedController()->GetSelection().has_value() ||
         accepted_source_.has_value();
}

views::View* DesktopMediaPickerDialogView::GetInitiallyFocusedView() {
  return GetCancelButton();
}

bool DesktopMediaPickerDialogView::Accept() {
  DCHECK(IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  // Ok button should only be enabled when a source is selected.
  base::Optional<DesktopMediaID> source_optional =
      accepted_source_.has_value() ? accepted_source_
                                   : GetSelectedController()->GetSelection();
  DesktopMediaID source = source_optional.value();
  source.audio_share = audio_share_checkbox_ &&
                       audio_share_checkbox_->GetVisible() &&
                       audio_share_checkbox_->GetChecked();
  if (source.audio_share &&
      dialog_source_ == DialogSource::kGetCurrentBrowsingContextMedia) {
    source.web_contents_id.disable_local_echo = true;
  }

  if (source.type == DesktopMediaID::TYPE_WEB_CONTENTS) {
    // Activate the selected tab and bring the browser window for the selected
    // tab to the front.
    content::WebContents* tab = content::WebContents::FromRenderFrameHost(
        content::RenderFrameHost::FromID(
            source.web_contents_id.render_process_id,
            source.web_contents_id.main_render_frame_id));
    if (tab) {
      tab->GetDelegate()->ActivateContents(tab);
      Browser* browser = chrome::FindBrowserWithWebContents(tab);
      if (browser && browser->window())
        browser->window()->Activate();
    }
  } else if (source.type == DesktopMediaID::TYPE_WINDOW) {
#if defined(USE_AURA)
    aura::Window* window = DesktopMediaID::GetNativeWindowById(source);
    Browser* browser = chrome::FindBrowserWithWindow(window);
    if (browser && browser->window())
      browser->window()->Activate();
#endif
  }

  RecordUmaSelection(dialog_source_, web_contents_, source,
                     GetSelectedSourceListType());

  if (parent_)
    parent_->NotifyDialogResult(source);

  bool notify_projector_session_start =
      presenter_tools_checkbox_ && presenter_tools_checkbox_->GetChecked() &&
      IsProjectorEnabled();
  if (notify_projector_session_start) {
    StartProjectorSession(source);
  }

  // Return true to close the window.
  return true;
}

bool DesktopMediaPickerDialogView::Cancel() {
  RecordUmaCancellation(dialog_source_);
  bool hide_projector_tools =
      presenter_tools_checkbox_ && presenter_tools_checkbox_->GetChecked();
  // If the user cancels while the projector tools are visible, hide them.
  if (hide_projector_tools)
    SetProjectorToolsVisible(false);
  return views::DialogDelegateView::Cancel();
}

bool DesktopMediaPickerDialogView::ShouldShowCloseButton() const {
  return false;
}

void DesktopMediaPickerDialogView::DeleteDelegate() {
  // If the dialog is being closed then notify the parent about it.
  if (parent_)
    parent_->NotifyDialogResult(DesktopMediaID());
  delete this;
}

void DesktopMediaPickerDialogView::OnSelectionChanged() {
  DialogModelChanged();
}

void DesktopMediaPickerDialogView::AcceptSource() {
  // This will call Accept() and close the dialog.
  AcceptDialog();
}

void DesktopMediaPickerDialogView::AcceptSpecificSource(DesktopMediaID source) {
  accepted_source_ = base::Optional<DesktopMediaID>(source);
  AcceptSource();
}

void DesktopMediaPickerDialogView::Reject() {
  CancelDialog();
}

void DesktopMediaPickerDialogView::OnSourceListLayoutChanged() {
  PreferredSizeChanged();
  // TODO(pbos): Ideally this would use shared logic similar to
  // BubbleDialogDelegateView::SizeToContents() instead of implementing sizing
  // logic in-place.
  const gfx::Size new_size = GetWidget()->GetRootView()->GetPreferredSize();
  if (GetModalType() == ui::ModalType::MODAL_TYPE_CHILD) {
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

BEGIN_METADATA(DesktopMediaPickerDialogView, views::DialogDelegateView)
END_METADATA

constexpr bool DesktopMediaPickerViews::kScreenAudioShareSupportedOnPlatform;

DesktopMediaPickerViews::DesktopMediaPickerViews() : dialog_(nullptr) {}

DesktopMediaPickerViews::~DesktopMediaPickerViews() {
  if (dialog_) {
    RecordUmaDismissal(dialog_->GetDialogSource());
    dialog_->DetachParent();
    dialog_->GetWidget()->Close();
  }
}

void DesktopMediaPickerViews::Show(
    const DesktopMediaPicker::Params& params,
    std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
    DoneCallback done_callback) {
  DesktopMediaPickerManager::Get()->OnShowDialog();

  callback_ = std::move(done_callback);
  dialog_ =
      new DesktopMediaPickerDialogView(params, this, std::move(source_lists));
}

void DesktopMediaPickerViews::NotifyDialogResult(DesktopMediaID source) {
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
  if (request && DesktopMediaList::kConfirmationOnlyDialogSupported &&
      request->video_type ==
          blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB) {
    return std::make_unique<GetCurrentBrowsingContextMediaDialog>();
  }
  return std::make_unique<DesktopMediaPickerViews>();
}
