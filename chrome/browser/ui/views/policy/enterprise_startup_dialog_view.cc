// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/policy/enterprise_startup_dialog_view.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

#if defined(OS_MAC)
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
      views::CONTROL, views::TEXT);
}

std::unique_ptr<views::Label> CreateText(const std::u16string& message) {
  auto text = std::make_unique<views::Label>(message);
  text->SetFontList(gfx::FontList().Derive(kFontSizeDelta, gfx::Font::NORMAL,
                                           gfx::Font::Weight::MEDIUM));
  text->SetEnabledColor(
      views::style::GetColor(*text, views::style::CONTEXT_DIALOG_BODY_TEXT,
                             views::style::STYLE_PRIMARY));
  text->SetLineHeight(kLineHeight);
  return text;
}

std::unique_ptr<views::View> CreateLogoView() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Show Google Chrome Enterprise logo only for official build.
  auto logo_image = std::make_unique<views::ImageView>();
  logo_image->SetImage(
      ui::ResourceBundle::GetSharedInstance()
          .GetImageNamed((logo_image->GetNativeTheme()->ShouldUseDarkColors())
                             ? IDR_PRODUCT_LOGO_ENTERPRISE_WHITE
                             : IDR_PRODUCT_LOGO_ENTERPRISE)
          .AsImageSkia());
  logo_image->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_PRODUCT_LOGO_ENTERPRISE_ALT_TEXT));
  gfx::Rect logo_bounds = logo_image->GetImageBounds();
  logo_image->SetImageSize(gfx::Size(
      logo_bounds.width() * kLogoHeight / logo_bounds.height(), kLogoHeight));
  logo_image->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  return logo_image;
#else
  return nullptr;
#endif
}

}  // namespace

EnterpriseStartupDialogView::EnterpriseStartupDialogView(
    EnterpriseStartupDialog::DialogResultCallback callback)
    : callback_(std::move(callback)) {
  set_draggable(true);
  SetButtons(ui::DIALOG_BUTTON_OK);
  SetExtraView(CreateLogoView());
  SetModalType(ui::MODAL_TYPE_NONE);
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
#if defined(OS_MAC)
  base::ThreadTaskRunnerHandle::Get()->PostTask(
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

  SetupLayout(std::move(throbber), std::move(text));
}

void EnterpriseStartupDialogView::DisplayErrorMessage(
    const std::u16string& error_message,
    const base::Optional<std::u16string>& accept_button) {
  ResetDialog(accept_button.has_value());
  std::unique_ptr<views::Label> text = CreateText(error_message);
  auto error_icon = std::make_unique<views::ImageView>();
  error_icon->SetImage(
      gfx::CreateVectorIcon(kBrowserToolsErrorIcon, kIconSize,
                            GetNativeTheme()->GetSystemColor(
                                ui::NativeTheme::kColorId_AlertSeverityHigh)));

  if (accept_button) {
    // TODO(ellyjones): This should use SetButtonLabel()
    // instead of changing the button text directly - this might break the
    // dialog's layout.
    GetOkButton()->SetText(*accept_button);
  }
  SetupLayout(std::move(error_icon), std::move(text));
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
#if defined(OS_MAC)
  base::CurrentThread::ScopedNestableTaskAllower allow_nested;
  StartModal(GetWidget()->GetNativeWindow());
#endif
}

void EnterpriseStartupDialogView::RunDialogCallback(bool was_accepted) {
#if defined(OS_MAC)
  // On mac, we need to stop the modal message loop before returning the result
  // to the caller who controls its own run loop.
  StopModal();
  if (can_show_browser_window_) {
    std::move(callback_).Run(was_accepted, can_show_browser_window_);
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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

gfx::Size EnterpriseStartupDialogView::CalculatePreferredSize() const {
  return gfx::Size(kDialogContentWidth, kDialogContentHeight);
}

void EnterpriseStartupDialogView::ResetDialog(bool show_accept_button) {
  DCHECK(GetOkButton());

  GetOkButton()->SetVisible(show_accept_button);
  RemoveAllChildViews(true);
}

void EnterpriseStartupDialogView::SetupLayout(
    std::unique_ptr<views::View> icon,
    std::unique_ptr<views::View> text) {
  // Padding between icon and text
  int text_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING);

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  auto* columnset = layout->AddColumnSet(0);
  // Horizontally centre the content.
  columnset->AddPaddingColumn(1.0, 0);
  columnset->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                       views::GridLayout::kFixedSize,
                       views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  columnset->AddPaddingColumn(views::GridLayout::kFixedSize, text_padding);
  columnset->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                       views::GridLayout::kFixedSize,
                       views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  columnset->AddPaddingColumn(1.0, 0);

  layout->AddPaddingRow(1.0, 0);
  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(std::move(icon));
  layout->AddView(std::move(text));
  layout->AddPaddingRow(1.0, 0);

  // TODO(ellyjones): Why is this being done here?
  GetWidget()->GetRootView()->Layout();
  GetWidget()->GetRootView()->SchedulePaint();
}

BEGIN_METADATA(EnterpriseStartupDialogView, views::DialogDelegateView)
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
    const base::Optional<std::u16string>& accept_button) {
  if (dialog_view_)
    dialog_view_->DisplayErrorMessage(error_message, accept_button);
}
bool EnterpriseStartupDialogImpl::IsShowing() {
  return dialog_view_;
}

// views::WidgetObserver:
void EnterpriseStartupDialogImpl::OnWidgetClosing(views::Widget* widget) {
  dialog_view_->RemoveWidgetObserver(this);
  dialog_view_ = nullptr;
}

/*
 * EnterpriseStartupDialog
 */

// static
std::unique_ptr<EnterpriseStartupDialog>
EnterpriseStartupDialog::CreateAndShowDialog(DialogResultCallback callback) {
  return std::make_unique<EnterpriseStartupDialogImpl>(std::move(callback));
}

}  // namespace policy
