// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/first_run_dialog.h"

#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/first_run/first_run_dialog.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/crash/core/app/crashpad.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace first_run {

void ShowFirstRunDialog() {
  // Don't show first run dialog when running in headless mode since this
  // would effectively block the UI because there is no one to interact with
  // the dialog.
  if (headless::IsHeadlessMode())
    return;

#if BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kViewsFirstRunDialog))
    ShowFirstRunDialogViews();
  else
    ShowFirstRunDialogCocoa();
#else
  ShowFirstRunDialogViews();
#endif
}

void ShowFirstRunDialogViews() {
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  FirstRunDialog::Show(
      base::BindRepeating(&platform_util::OpenExternal,
                          GURL(chrome::kLearnMoreReportingURL)),
      run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace first_run

// static
void FirstRunDialog::Show(base::RepeatingClosure learn_more_callback,
                          base::RepeatingClosure quit_runloop) {
  FirstRunDialog* dialog = new FirstRunDialog(std::move(learn_more_callback),
                                              std::move(quit_runloop));
  views::DialogDelegate::CreateDialogWidget(dialog, NULL, NULL)->Show();
}

FirstRunDialog::FirstRunDialog(base::RepeatingClosure learn_more_callback,
                               base::RepeatingClosure quit_runloop)
    : quit_runloop_(quit_runloop) {
  SetTitle(l10n_util::GetStringUTF16(IDS_FIRST_RUN_DIALOG_WINDOW_TITLE));
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetExtraView(
      std::make_unique<views::Link>(l10n_util::GetStringUTF16(IDS_LEARN_MORE)))
      ->SetCallback(std::move(learn_more_callback));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kControl,
          views::DialogContentType::kControl),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  make_default_ = AddChildView(std::make_unique<views::Checkbox>(
      l10n_util::GetStringUTF16(IDS_FR_CUSTOMIZE_DEFAULT_BROWSER)));
  make_default_->SetChecked(true);

  report_crashes_ = AddChildView(std::make_unique<views::Checkbox>(
      l10n_util::GetStringUTF16(IDS_FR_ENABLE_LOGGING)));
  // Having this box checked means the user has to opt-out of metrics recording.
  report_crashes_->SetChecked(true);
}

FirstRunDialog::~FirstRunDialog() {
}

void FirstRunDialog::Done() {
  CHECK(!quit_runloop_.is_null());

  if (!closed_through_accept_button_) {
    ChangeMetricsReportingState(false);
  }

  quit_runloop_.Run();
}

bool FirstRunDialog::Accept() {
  GetWidget()->Hide();
  closed_through_accept_button_ = true;

  ChangeMetricsReportingState(report_crashes_->GetChecked());

  if (make_default_->GetChecked())
    shell_integration::SetAsDefaultBrowser();

  Done();
  return true;
}

void FirstRunDialog::WindowClosing() {
  Done();
}

BEGIN_METADATA(FirstRunDialog)
END_METADATA
