// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_dialog_view.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

// static
views::Widget* RelaunchRequiredDialogView::Show(
    Browser* browser,
    base::Time deadline,
    base::RepeatingClosure on_accept) {
  views::Widget* widget = constrained_window::CreateBrowserModalDialogViews(
      new RelaunchRequiredDialogView(deadline, std::move(on_accept)),
      browser->window()->GetNativeWindow());
  widget->Show();
  return widget;
}

RelaunchRequiredDialogView::~RelaunchRequiredDialogView() = default;

// static
RelaunchRequiredDialogView* RelaunchRequiredDialogView::FromWidget(
    views::Widget* widget) {
  return static_cast<RelaunchRequiredDialogView*>(
      widget->widget_delegate()->AsDialogDelegate());
}

void RelaunchRequiredDialogView::SetDeadline(base::Time deadline) {
  relaunch_required_timer_.SetDeadline(deadline);
}

bool RelaunchRequiredDialogView::Cancel() {
  base::RecordAction(base::UserMetricsAction("RelaunchRequired_Close"));

  return true;
}

bool RelaunchRequiredDialogView::Accept() {
  base::RecordAction(base::UserMetricsAction("RelaunchRequired_Accept"));

  on_accept_.Run();

  // Keep the dialog open in case shutdown is prevented for some reason so that
  // the user can try again if needed.
  return false;
}

ui::ModalType RelaunchRequiredDialogView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 RelaunchRequiredDialogView::GetWindowTitle() const {
  return relaunch_required_timer_.GetWindowTitle();
}

bool RelaunchRequiredDialogView::ShouldShowCloseButton() const {
  return false;
}

gfx::ImageSkia RelaunchRequiredDialogView::GetWindowIcon() {
  return gfx::CreateVectorIcon(gfx::IconDescription(
      vector_icons::kBusinessIcon, kTitleIconSize, gfx::kChromeIconGrey,
      base::TimeDelta(), gfx::kNoneIcon));
}

bool RelaunchRequiredDialogView::ShouldShowWindowIcon() const {
  return true;
}

gfx::Size RelaunchRequiredDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

// |relaunch_required_timer_| automatically starts for the next time the title
// needs to be updated (e.g., from "2 days" to "3 days").
RelaunchRequiredDialogView::RelaunchRequiredDialogView(
    base::Time deadline,
    base::RepeatingClosure on_accept)
    : on_accept_(on_accept),
      relaunch_required_timer_(
          deadline,
          base::BindRepeating(&RelaunchRequiredDialogView::UpdateWindowTitle,
                              base::Unretained(this))) {
  DialogDelegate::set_default_button(ui::DIALOG_BUTTON_NONE);
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_RELAUNCH_ACCEPT_BUTTON));
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_RELAUNCH_REQUIRED_CANCEL_BUTTON));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  chrome::RecordDialogCreation(chrome::DialogIdentifier::RELAUNCH_REQUIRED);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));

  auto label = std::make_unique<views::Label>(
      l10n_util::GetPluralStringFUTF16(IDS_RELAUNCH_REQUIRED_BODY,
                                       BrowserList::GetIncognitoBrowserCount()),
      views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Align the body label with the left edge of the dialog's title.
  // TODO(bsep): Remove this when fixing https://crbug.com/810970.
  int title_offset = 2 * views::LayoutProvider::Get()
                             ->GetInsetsMetric(views::INSETS_DIALOG_TITLE)
                             .left() +
                     kTitleIconSize;
  label->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(0, title_offset - margins().left(), 0, 0)));

  AddChildView(std::move(label));

  base::RecordAction(base::UserMetricsAction("RelaunchRequiredShown"));
}

void RelaunchRequiredDialogView::UpdateWindowTitle() {
  GetWidget()->UpdateWindowTitle();
}
