// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/importer/import_lock_dialog_view.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/importer/importer_lock_dialog.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

using base::UserMetricsAction;

namespace importer {

void ShowImportLockDialog(gfx::NativeWindow parent,
                          base::OnceCallback<void(bool)> callback) {
  ImportLockDialogView::Show(parent, std::move(callback));
}

}  // namespace importer

// static
void ImportLockDialogView::Show(gfx::NativeWindow parent,
                                base::OnceCallback<void(bool)> callback) {
  views::DialogDelegate::CreateDialogWidget(
      new ImportLockDialogView(std::move(callback)), nullptr, nullptr)
      ->Show();
  base::RecordAction(UserMetricsAction("ImportLockDialogView_Shown"));
}

ImportLockDialogView::ImportLockDialogView(
    base::OnceCallback<void(bool)> callback)
    : callback_(std::move(callback)) {
  SetTitle(IDS_IMPORTER_LOCK_TITLE);

  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_IMPORTER_LOCK_OK));

  auto done_callback = [](ImportLockDialogView* dialog, bool accepted) {
    if (dialog->callback_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(dialog->callback_), accepted));
    }
  };
  SetAcceptCallback(
      base::BindOnce(done_callback, base::Unretained(this), true));
  SetCancelCallback(
      base::BindOnce(done_callback, base::Unretained(this), false));
  SetCloseCallback(
      base::BindOnce(done_callback, base::Unretained(this), false));

  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  views::Label* description_label =
      new views::Label(l10n_util::GetStringUTF16(IDS_IMPORTER_LOCK_TEXT));
  description_label->SetBorder(views::CreateEmptyBorder(
      ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kText)));
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(description_label);
}

ImportLockDialogView::~ImportLockDialogView() = default;

BEGIN_METADATA(ImportLockDialogView, views::DialogDelegateView)
END_METADATA
