// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/share_this_tab_dialog_views.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/config/chromebox_for_meetings/buildflags.h"  // PLATFORM_CFM
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_manager.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_utils.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"
#include "chrome/browser/ui/views/desktop_capture/share_this_tab_source_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window_tree_host.h"
#endif

namespace {

constexpr int kTitleTopMargin = 16;
constexpr gfx::Insets kAudioToggleInsets = gfx::Insets::VH(8, 16);
constexpr int kAudioToggleChildSpacing = 8;

void RecordUmaCancellation(base::TimeTicks dialog_open_time) {
  RecordUma(GDMPreferCurrentTabResult::kUserCancelled, dialog_open_time);
}

void RecordUmaSelection(base::TimeTicks dialog_open_time) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RecordUma(GDMPreferCurrentTabResult::kUserSelectedThisTab, dialog_open_time);
}

// The length of the initial delay during which the "Allow"-button is disabled
// in the share-this-tab dialog.
const base::FeatureParam<int> kShareThisTabDialogActivationDelayMs{
    &kShareThisTabDialog, "activation_delay_ms", 500};

bool ShouldAutoAcceptThisTabCapture() {
#if BUILDFLAG(PLATFORM_CFM)
  return true;
#else
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kThisTabCaptureAutoAccept);
#endif
}

}  // namespace

ShareThisTabDialogView::ShareThisTabDialogView(
    const DesktopMediaPicker::Params& params,
    ShareThisTabDialogViews* parent)
    : web_contents_(params.web_contents->GetWeakPtr()),
      app_name_(params.app_name),
      parent_(parent),
      auto_select_tab_(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kAutoSelectTabCaptureSourceByTitle)),
      auto_select_source_(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kAutoSelectDesktopCaptureSource)),
      auto_accept_this_tab_capture_(ShouldAutoAcceptThisTabCapture()),
      auto_reject_this_tab_capture_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kThisTabCaptureAutoReject)),
      dialog_open_time_(base::TimeTicks::Now()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!auto_accept_this_tab_capture_ || !auto_reject_this_tab_capture_);

  SetModalType(params.modality);
  RegisterDeleteDelegateCallback(base::BindOnce(
      [](ShareThisTabDialogView* dialog) {
        // If the dialog is being closed then notify the parent about it.
        if (dialog->parent_) {
          dialog->parent_->NotifyDialogResult(content::DesktopMediaID());
        }
      },
      this));

  const ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();
  gfx::Insets dialog_insets = provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText);
  dialog_insets.set_top(kTitleTopMargin);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, dialog_insets,
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL)));

  views::Label* title_label = AddChildView(std::make_unique<views::Label>());
  title_label->SetFontList(views::TypographyProvider::Get().GetFont(
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY));
  title_label->SetAllowCharacterBreak(true);
  title_label->SetMultiLine(true);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetText(
      l10n_util::GetStringFUTF16(IDS_SHARE_THIS_TAB_DIALOG_TITLE, app_name_));
  // TODO(crbug.com/40269137): Prevent non-initial focus of the title label.
  title_label->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  SetInitiallyFocusedView(title_label);

  views::Label* description_label =
      AddChildView(std::make_unique<views::Label>());
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_label->SetText(
      l10n_util::GetStringUTF16(IDS_SHARE_THIS_TAB_DIALOG_TEXT));

  SetupSourceView();

  if (params.request_audio) {
    SetupAudioToggle();
  }

  // Use no delay in tests that auto-accepts/rejects the dialog.
  const base::TimeDelta activation_delay =
      (ShouldAutoAccept() || ShouldAutoReject())
          ? base::Milliseconds(0)
          : base::Milliseconds(kShareThisTabDialogActivationDelayMs.Get());
  activation_timer_.Start(FROM_HERE, activation_delay,
                          base::BindOnce(&ShareThisTabDialogView::Activate,
                                         weak_factory_.GetWeakPtr()));

  // If |params.web_contents| is set and it's not a background page then the
  // picker will be shown modal to the web contents. Otherwise the picker is
  // shown in a separate window.
  if (params.web_contents &&
      !params.web_contents->GetDelegate()->IsNeverComposited(
          params.web_contents)) {
    const Browser* browser = chrome::FindBrowserWithTab(params.web_contents);
    // Close the extension popup to prevent spoofing.
    if (browser && browser->window() &&
        browser->window()->GetExtensionsContainer()) {
      browser->window()->GetExtensionsContainer()->HideActivePopup();
    }
    constrained_window::ShowWebModalDialogViews(this, params.web_contents);
  } else {
#if BUILDFLAG(IS_MAC)
    // On Mac, ModalType::kChild with a null parent isn't allowed - fall back to
    // ModalType::kWindow.
    SetModalType(ui::mojom::ModalType::kWindow);
#endif
    CreateDialogWidget(this, params.context, nullptr)->Show();
  }

  source_view_->SetBorder(views::CreateThemedRoundedRectBorder(
      1, 4, ui::kColorSysPrimaryContainer));

  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_SHARE_THIS_TAB_DIALOG_ALLOW));
  SetButtonEnabled(ui::mojom::DialogButton::kOk, false);
  SetButtonStyle(ui::mojom::DialogButton::kOk, ui::ButtonStyle::kTonal);
  SetButtonStyle(ui::mojom::DialogButton::kCancel, ui::ButtonStyle::kTonal);

  // Simply pressing ENTER without tab-key navigating to the button
  // must not accept the dialog, or else that'd be a security issue.
  SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kNone));
}

ShareThisTabDialogView::~ShareThisTabDialogView() = default;

void ShareThisTabDialogView::RecordUmaDismissal() const {
  RecordUma(GDMPreferCurrentTabResult::kDialogDismissed, dialog_open_time_);
}

void ShareThisTabDialogView::DetachParent() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  parent_ = nullptr;
}

gfx::Size ShareThisTabDialogView::CalculatePreferredSize(
    const views::SizeBounds& /*available_size*/) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static constexpr size_t kDialogViewWidth = 600;
  return gfx::Size(
      kDialogViewWidth,
      GetLayoutManager()->GetPreferredHeightForWidth(this, kDialogViewWidth));
}

bool ShareThisTabDialogView::ShouldShowWindowTitle() const {
  return false;
}

bool ShareThisTabDialogView::Accept() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!activation_timer_.IsRunning());
  CHECK(IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  source_view_->StopRefreshing();
  if (parent_ && web_contents_) {
    content::DesktopMediaID desktop_media_id(
        content::DesktopMediaID::TYPE_WEB_CONTENTS,
        content::DesktopMediaID::kNullId,
        content::WebContentsMediaCaptureId(
            web_contents_->GetPrimaryMainFrame()->GetProcess()->GetID(),
            web_contents_->GetPrimaryMainFrame()->GetRoutingID()));
    desktop_media_id.audio_share =
        audio_toggle_button_ && audio_toggle_button_->GetIsOn();
    parent_->NotifyDialogResult(desktop_media_id);
    RecordUmaSelection(dialog_open_time_);
  }

  // Return true to close the window.
  return true;
}

bool ShareThisTabDialogView::Cancel() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  source_view_->StopRefreshing();
  activation_timer_.Stop();
  RecordUmaCancellation(dialog_open_time_);
  return views::DialogDelegateView::Cancel();
}

bool ShareThisTabDialogView::ShouldShowCloseButton() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return false;
}

void ShareThisTabDialogView::SetupSourceView() {
  View* source_container = AddChildView(std::make_unique<views::View>());
  source_container->SetProperty(views::kMarginsKey,
                                gfx::Insets::TLBR(16, 0, 0, 0));

  views::BoxLayout* source_layout =
      source_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  source_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  source_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  source_view_ = source_container->AddChildView(
      std::make_unique<ShareThisTabSourceView>(web_contents_));
}

void ShareThisTabDialogView::SetupAudioToggle() {
  View* audio_toggle_container = AddChildView(std::make_unique<views::View>());
  audio_toggle_container->SetProperty(views::kMarginsKey,
                                      gfx::Insets::TLBR(8, 0, 0, 0));
  audio_toggle_container->SetBackground(
      views::CreateThemedRoundedRectBackground(ui::kColorSysSurface4, 8));

  views::ImageView* audio_icon_view = audio_toggle_container->AddChildView(
      std::make_unique<views::ImageView>());
  audio_icon_view->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kVolumeUpIcon, ui::kColorIcon,
      GetLayoutConstant(PAGE_INFO_ICON_SIZE)));

  views::Label* audio_toggle_label =
      audio_toggle_container->AddChildView(std::make_unique<views::Label>());
  audio_toggle_label->SetText(
      l10n_util::GetStringUTF16(IDS_SHARE_THIS_TAB_AUDIO_SHARE));
  audio_toggle_label->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);

  audio_toggle_button_ = audio_toggle_container->AddChildView(
      std::make_unique<views::ToggleButton>());
  audio_toggle_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_SHARE_THIS_TAB_AUDIO_SHARE));
  audio_toggle_button_->SetIsOn(
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kScreenCaptureAudioDefaultUnchecked));

  views::BoxLayout* audio_toggle_layout =
      audio_toggle_container->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal, kAudioToggleInsets));
  audio_toggle_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  audio_toggle_layout->set_between_child_spacing(kAudioToggleChildSpacing);
  audio_toggle_layout->SetFlexForView(audio_toggle_label, 1);
}

void ShareThisTabDialogView::Activate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  source_view_->Activate();
  SetButtonEnabled(ui::mojom::DialogButton::kOk, true);

  // In tests.
  if (ShouldAutoAccept()) {
    AcceptDialog();
  } else if (ShouldAutoReject()) {
    CancelDialog();
  }
}

bool ShareThisTabDialogView::ShouldAutoAccept() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!web_contents_) {
    return false;
  }

  if (auto_accept_this_tab_capture_) {
    return true;
  }

  if (!auto_select_tab_.empty() &&
      web_contents_->GetTitle().find(base::ASCIIToUTF16(auto_select_tab_)) !=
          std::u16string::npos) {
    return true;
  }

  return (!auto_select_source_.empty() &&
          web_contents_->GetTitle().find(
              base::ASCIIToUTF16(auto_select_source_)) != std::u16string::npos);
}

bool ShareThisTabDialogView::ShouldAutoReject() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return auto_reject_this_tab_capture_;
}

BEGIN_METADATA(ShareThisTabDialogView)
END_METADATA

ShareThisTabDialogViews::ShareThisTabDialogViews() : dialog_(nullptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ShareThisTabDialogViews::~ShareThisTabDialogViews() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (dialog_) {
    dialog_->RecordUmaDismissal();
    dialog_->DetachParent();
    dialog_->GetWidget()->Close();
  }
}

void ShareThisTabDialogViews::Show(
    const DesktopMediaPicker::Params& params,
    std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
    DoneCallback done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!callback_);
  CHECK(!dialog_);

  DesktopMediaPickerManager::Get()->OnShowDialog(params);
  callback_ = std::move(done_callback);
  dialog_ = new ShareThisTabDialogView(params, this);
}

void ShareThisTabDialogViews::NotifyDialogResult(
    const content::DesktopMediaID& source) {
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
      FROM_HERE, base::BindOnce(std::move(callback_), source));
}
