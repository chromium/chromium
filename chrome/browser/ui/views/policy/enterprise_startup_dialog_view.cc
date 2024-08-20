// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/policy/enterprise_startup_dialog_view.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"

#if BUILDFLAG(IS_MAC)
#include "base/task/current_thread.h"
#include "chrome/browser/ui/views/policy/enterprise_startup_dialog_mac_util.h"
#endif

namespace policy {
namespace {

constexpr int kDialogContentHeight = 190;  // The height of dialog content area.
constexpr int kDialogContentWidth = 670;   // The width of dialog content area.
constexpr int kIconSize = 24;      // The size of throbber and error icon.
constexpr int kLineHeight = 22;    // The height of text line.
constexpr int kFontSizeDelta = 3;  // The font size of text.

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr int kLogoHeight = 20;  // The height of Chrome enterprise logo.
#endif

gfx::Insets GetDialogInsets() {
  return ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kText);
}

std::unique_ptr<views::Label> CreateText(const std::u16string& message) {
  auto text = std::make_unique<views::Label>(
      message, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY);
  text->SetFontList(gfx::FontList().Derive(kFontSizeDelta, gfx::Font::NORMAL,
                                           gfx::Font::Weight::MEDIUM));
  text->SetLineHeight(kLineHeight);
  return text;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
class LogoView : public views::ImageView {
  METADATA_HEADER(LogoView, views::ImageView)

 public:
  LogoView() {
    SetTooltipText(
        l10n_util::GetStringUTF16(IDS_PRODUCT_LOGO_ENTERPRISE_ALT_TEXT));
    SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  }

  void OnThemeChanged() override {
    ImageView::OnThemeChanged();
    SetImage(ui::ResourceBundle::GetSharedInstance()
                 .GetImageNamed((GetNativeTheme()->ShouldUseDarkColors())
                                    ? IDR_PRODUCT_LOGO_ENTERPRISE_WHITE
                                    : IDR_PRODUCT_LOGO_ENTERPRISE)
                 .AsImageSkia());
    const gfx::Rect logo_bounds = GetImageBounds();
    SetImageSize(gfx::Size(
        logo_bounds.width() * kLogoHeight / logo_bounds.height(), kLogoHeight));
  }
};

BEGIN_METADATA(LogoView)
END_METADATA
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Alternate implementation of the EnterpriseStartupDialog which is used when
// the headless mode is in effect. It does not display anything and when error
// is set immediately calls back with not accepted condition.
class HeadlessEnterpriseStartupDialogImpl : public EnterpriseStartupDialog {
 public:
  explicit HeadlessEnterpriseStartupDialogImpl(DialogResultCallback callback)
      : callback_(std::move(callback)) {}

  HeadlessEnterpriseStartupDialogImpl(
      const HeadlessEnterpriseStartupDialogImpl&) = delete;
  HeadlessEnterpriseStartupDialogImpl& operator=(
      const HeadlessEnterpriseStartupDialogImpl&) = delete;

  ~HeadlessEnterpriseStartupDialogImpl() override {
    if (callback_) {
      // ChromeBrowserCloudManagementRegisterWatcher dismisses the dialog
      // without displaying an error messgae (in which case we would not
      // have the outstanding callback) in case of successful enrollment,
      // so allow it to show the browser window using the callback.
      std::move(callback_).Run(/*was_accepted=*/false,
                               /*can_show_browser_window_=*/true);
    }
  }

  // Override EnterpriseStartupDialog
  void DisplayLaunchingInformationWithThrobber(
      const std::u16string& information) override {}

  void DisplayErrorMessage(
      const std::u16string& error_message,
      const std::optional<std::u16string>& accept_button) override {
    if (callback_) {
      // In headless mode the dialog is invisible, therefore there is
      // no one to accept or dismiss it. So just dismiss the dialog
      // right away without accepting the prompt and not allowing
      // browser to show its window.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback_), /*was_accepted=*/false,
                         /*can_show_browser_window_=*/false));
    }
  }

  bool IsShowing() override { return true; }

 private:
  DialogResultCallback callback_;
};

}  // namespace

EnterpriseStartupDialogView::EnterpriseStartupDialogView(
    EnterpriseStartupDialog::DialogResultCallback callback)
    : callback_(std::move(callback)) {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_between_child_spacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING));

  set_draggable(true);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Show Google Chrome Enterprise logo only for official build.
  SetExtraView(std::make_unique<LogoView>());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  SetModalType(ui::mojom::ModalType::kNone);
  SetAcceptCallback(
      base::BindOnce(&EnterpriseStartupDialogView::RunDialogCallback,
                     base::Unretained(this), true));
  SetCancelCallback(
      base::BindOnce(&EnterpriseStartupDialogView::RunDialogCallback,
                     base::Unretained(this), false));
  SetCloseCallback(
      base::BindOnce(&EnterpriseStartupDialogView::RunDialogCallback,
                     base::Unretained(this), false));
  SetBorder(views::CreateEmptyBorder(GetDialogInsets()));
  CreateDialogWidget(this, nullptr, nullptr)->Show();
#if BUILDFLAG(IS_MAC)
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&EnterpriseStartupDialogView::StartModalDialog,
                                weak_factory_.GetWeakPtr()));
#endif
}

EnterpriseStartupDialogView::~EnterpriseStartupDialogView() {}

void EnterpriseStartupDialogView::DisplayLaunchingInformationWithThrobber(
    const std::u16string& information) {
  ResetDialog(false);

  std::unique_ptr<views::Label> text = CreateText(information);
  auto throbber = std::make_unique<views::Throbber>();
  gfx::Size throbber_size = gfx::Size(kIconSize, kIconSize);
  throbber->SetPreferredSize(throbber_size);
  throbber->Start();

  AddContent(std::move(throbber), std::move(text));
}

void EnterpriseStartupDialogView::DisplayErrorMessage(
    const std::u16string& error_message,
    const std::optional<std::u16string>& accept_button) {
  ResetDialog(accept_button.has_value());
  std::unique_ptr<views::Label> text = CreateText(error_message);
  auto error_icon =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          kBrowserToolsErrorIcon, ui::kColorAlertHighSeverity, kIconSize));

  if (accept_button) {
    // TODO(ellyjones): This should use SetButtonLabel()
    // instead of changing the button text directly - this might break the
    // dialog's layout.
    GetOkButton()->SetText(*accept_button);
  }
  AddContent(std::move(error_icon), std::move(text));
}

void EnterpriseStartupDialogView::CloseDialog() {
  can_show_browser_window_ = true;
  GetWidget()->Close();
}

void EnterpriseStartupDialogView::AddWidgetObserver(
    views::WidgetObserver* observer) {
  GetWidget()->AddObserver(observer);
}
void EnterpriseStartupDialogView::RemoveWidgetObserver(
    views::WidgetObserver* observer) {
  GetWidget()->RemoveObserver(observer);
}

void EnterpriseStartupDialogView::StartModalDialog() {
#if BUILDFLAG(IS_MAC)
  base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
  StartModal(GetWidget()->GetNativeWindow());
#endif
}

void EnterpriseStartupDialogView::RunDialogCallback(bool was_accepted) {
#if BUILDFLAG(IS_MAC)
  // On mac, we need to stop the modal message loop before returning the result
  // to the caller who controls its own run loop.
  StopModal();
  if (can_show_browser_window_) {
    std::move(callback_).Run(was_accepted, can_show_browser_window_);
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), was_accepted,
                                  can_show_browser_window_));
  }
#else
  std::move(callback_).Run(was_accepted, can_show_browser_window_);
#endif
}

bool EnterpriseStartupDialogView::ShouldShowWindowTitle() const {
  return false;
}

gfx::Size EnterpriseStartupDialogView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kDialogContentWidth, kDialogContentHeight);
}

void EnterpriseStartupDialogView::ResetDialog(bool show_accept_button) {
  DCHECK(GetOkButton());

  GetOkButton()->SetVisible(show_accept_button);
  RemoveAllChildViews();
}

void EnterpriseStartupDialogView::AddContent(
    std::unique_ptr<views::View> icon,
    std::unique_ptr<views::View> text) {
  AddChildView(std::move(icon));
  AddChildView(std::move(text));

  // TODO(weili): The child views are added after the dialog shows. So it
  // requires relayout and repaint. Consider a refactoring to add content
  // before showing.
  GetWidget()->GetRootView()->DeprecatedLayoutImmediately();
  GetWidget()->GetRootView()->SchedulePaint();
}

BEGIN_METADATA(EnterpriseStartupDialogView)
END_METADATA

/*
 * EnterpriseStartupDialogImpl
 */

EnterpriseStartupDialogImpl::EnterpriseStartupDialogImpl(
    DialogResultCallback callback) {
  dialog_view_ = new EnterpriseStartupDialogView(std::move(callback));
  dialog_view_->AddWidgetObserver(this);
}

EnterpriseStartupDialogImpl::~EnterpriseStartupDialogImpl() {
  if (dialog_view_) {
    dialog_view_->RemoveWidgetObserver(this);
    dialog_view_->CloseDialog();
  }
  CHECK(!IsInObserverList());
}

void EnterpriseStartupDialogImpl::DisplayLaunchingInformationWithThrobber(
    const std::u16string& information) {
  if (dialog_view_)
    dialog_view_->DisplayLaunchingInformationWithThrobber(information);
}

void EnterpriseStartupDialogImpl::DisplayErrorMessage(
    const std::u16string& error_message,
    const std::optional<std::u16string>& accept_button) {
  if (dialog_view_)
    dialog_view_->DisplayErrorMessage(error_message, accept_button);
}

bool EnterpriseStartupDialogImpl::IsShowing() {
  return dialog_view_;
}

// views::WidgetObserver:
void EnterpriseStartupDialogImpl::OnWidgetDestroying(views::Widget* widget) {
  dialog_view_->RemoveWidgetObserver(this);
  dialog_view_ = nullptr;
}

/*
 * EnterpriseStartupDialog
 */

// static
std::unique_ptr<EnterpriseStartupDialog>
EnterpriseStartupDialog::CreateAndShowDialog(DialogResultCallback callback) {
  // If running in headless mode use an alternate version of the enterprise
  // startup dialog.
  if (headless::IsHeadlessMode()) {
    return std::make_unique<HeadlessEnterpriseStartupDialogImpl>(
        std::move(callback));
  }

  return std::make_unique<EnterpriseStartupDialogImpl>(std::move(callback));
}

}  // namespace policy
