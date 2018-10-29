// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/policy/enterprise_startup_dialog_view.h"

#include <memory>
#include <utility>

#include "base/i18n/message_formatter.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
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
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/window/dialog_client_view.h"

#if defined(OS_MACOSX)
#include "base/message_loop/message_loop_current.h"
#include "chrome/browser/ui/views/policy/enterprise_startup_dialog_mac_util.h"
#endif

namespace policy {
namespace {

constexpr int kDialogContentHeight = 190;  // The height of dialog content area.
constexpr int kDialogContentWidth = 670;   // The width of dialog content area.
constexpr int kIconSize = 24;      // The size of throbber and error icon.
constexpr int kLineHeight = 22;    // The height of text line.
constexpr int kFontSizeDelta = 3;  // The font size of text.

#if defined(GOOGLE_CHROME_BUILD)
constexpr int kLogoHeight = 20;  // The height of Chrome enterprise logo.
#endif

gfx::Insets GetDialogInsets() {
  return ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::CONTROL, views::TEXT);
}

views::Label* CreateText(const base::string16& message) {
  views::Label* text = new views::Label(message);
  text->SetFontList(gfx::FontList().Derive(kFontSizeDelta, gfx::Font::NORMAL,
                                           gfx::Font::Weight::MEDIUM));
  text->SetEnabledColor(gfx::kGoogleGrey700);
  text->SetLineHeight(kLineHeight);
  return text;
}

}  // namespace

EnterpriseStartupDialogView::EnterpriseStartupDialogView(
    EnterpriseStartupDialog::DialogResultCallback callback)
    : callback_(std::move(callback)),
      can_show_browser_window_(false),
      weak_factory_(this) {
  SetBorder(views::CreateEmptyBorder(GetDialogInsets()));
  CreateDialogWidget(this, nullptr, nullptr)->Show();
#if defined(OS_MACOSX)
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&EnterpriseStartupDialogView::StartModalDialog,
                                weak_factory_.GetWeakPtr()));
#endif
}

EnterpriseStartupDialogView::~EnterpriseStartupDialogView() {}

void EnterpriseStartupDialogView::DisplayLaunchingInformationWithThrobber(
    const base::string16& information) {
  ResetDialog(false);

  views::Label* text = CreateText(information);
  views::Throbber* throbber = new views::Throbber();
  gfx::Size throbber_size = gfx::Size(kIconSize, kIconSize);
  throbber->SetPreferredSize(throbber_size);
  throbber->Start();

  SetupLayout(throbber, text);
}

void EnterpriseStartupDialogView::DisplayErrorMessage(
    const base::string16& error_message,
    const base::Optional<base::string16>& accept_button) {
  ResetDialog(accept_button.has_value());
  views::Label* text = CreateText(error_message);
  views::ImageView* error_icon = new views::ImageView();
  error_icon->SetImage(gfx::CreateVectorIcon(kBrowserToolsErrorIcon, kIconSize,
                                             gfx::kGoogleRed700));

  if (accept_button)
    GetDialogClientView()->ok_button()->SetText(*accept_button);
  SetupLayout(error_icon, text);
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
#if defined(OS_MACOSX)
  base::MessageLoopCurrent::ScopedNestableTaskAllower allow_nested;
  StartModal(GetWidget()->GetNativeWindow());
#endif
}

void EnterpriseStartupDialogView::RunDialogCallback(bool was_accepted) {
#if defined(OS_MACOSX)
  // On mac, we need to stop the modal message loop before returning the result
  // to the caller who controls its own run loop.
  StopModal();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), was_accepted,
                                can_show_browser_window_));
#else
  std::move(callback_).Run(was_accepted, can_show_browser_window_);
#endif
}

bool EnterpriseStartupDialogView::Accept() {
  RunDialogCallback(true);
  return true;
}
bool EnterpriseStartupDialogView::Cancel() {
  RunDialogCallback(false);
  return true;
}

bool EnterpriseStartupDialogView::Close() {
  return Cancel();
}

bool EnterpriseStartupDialogView::ShouldShowWindowTitle() const {
  return false;
}

ui::ModalType EnterpriseStartupDialogView::GetModalType() const {
  return ui::MODAL_TYPE_NONE;
}

views::View* EnterpriseStartupDialogView::CreateExtraView() {
#if defined(GOOGLE_CHROME_BUILD)
  // Show Google Chrome Enterprise logo only for official build.
  views::ImageView* logo_image = new views::ImageView();
  logo_image->SetImage(ui::ResourceBundle::GetSharedInstance()
                           .GetImageNamed(IDR_PRODUCT_LOGO_ENTERPRISE)
                           .AsImageSkia());
  logo_image->set_tooltip_text(
      l10n_util::GetStringUTF16(IDS_PRODUCT_LOGO_ENTERPRISE_ALT_TEXT));
  gfx::Rect logo_bounds = logo_image->GetImageBounds();
  logo_image->SetImageSize(gfx::Size(
      logo_bounds.width() * kLogoHeight / logo_bounds.height(), kLogoHeight));
  logo_image->SetVerticalAlignment(views::ImageView::CENTER);
  return logo_image;
#else
  return nullptr;
#endif
}

int EnterpriseStartupDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

gfx::Size EnterpriseStartupDialogView::CalculatePreferredSize() const {
  return gfx::Size(kDialogContentWidth, kDialogContentHeight);
}

void EnterpriseStartupDialogView::ResetDialog(bool show_accept_button) {
  DCHECK(GetDialogClientView()->ok_button());

  GetDialogClientView()->ok_button()->SetVisible(show_accept_button);
  RemoveAllChildViews(true);
}

void EnterpriseStartupDialogView::SetupLayout(views::View* icon,
                                              views::View* text) {
  // Padding between icon and text
  int text_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING);

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));
  auto* columnset = layout->AddColumnSet(0);
  // Horizontally centre the content.
  columnset->AddPaddingColumn(1.0, 0);
  columnset->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                       views::GridLayout::kFixedSize,
                       views::GridLayout::USE_PREF, 0, 0);
  columnset->AddPaddingColumn(views::GridLayout::kFixedSize, text_padding);
  columnset->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                       views::GridLayout::kFixedSize,
                       views::GridLayout::USE_PREF, 0, 0);
  columnset->AddPaddingColumn(1.0, 0);

  layout->AddPaddingRow(1.0, 0);
  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(icon);
  layout->AddView(text);
  layout->AddPaddingRow(1.0, 0);

  GetDialogClientView()->Layout();
  GetDialogClientView()->SchedulePaint();
}

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
}

void EnterpriseStartupDialogImpl::DisplayLaunchingInformationWithThrobber(
    const base::string16& information) {
  if (dialog_view_)
    dialog_view_->DisplayLaunchingInformationWithThrobber(information);
}

void EnterpriseStartupDialogImpl::DisplayErrorMessage(
    const base::string16& error_message,
    const base::Optional<base::string16>& accept_button) {
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
