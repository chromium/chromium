// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_dialog/app_block_dialog_view.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

AppBlockDialogView* g_app_block_dialog_view = nullptr;

}  // namespace

namespace apps {

// static
void AppServiceProxy::CreateBlockDialog(const std::string& app_name,
                                        const gfx::ImageSkia& image,
                                        Profile* profile) {
  views::DialogDelegate::CreateDialogWidget(
      new AppBlockDialogView(app_name, image, profile), nullptr, nullptr)
      ->Show();
}

}  // namespace apps

AppBlockDialogView::AppBlockDialogView(const std::string& app_name,
                                       const gfx::ImageSkia& image,
                                       Profile* profile)
    : AppDialogView(ui::ImageModel::FromImageSkia(image)) {
  InitializeView();
  AddTitle(l10n_util::GetStringFUTF16(IDS_APP_BLOCK_PROMPT_TITLE,
                                      base::UTF8ToUTF16(app_name)));

  std::u16string subtitle_text = l10n_util::GetStringFUTF16(
      profile->IsChild() ? IDS_APP_BLOCK_HEADING_FOR_CHILD
                         : IDS_APP_BLOCK_HEADING,
      base::UTF8ToUTF16(app_name));

  AddSubtitle(subtitle_text);

  g_app_block_dialog_view = this;
}

AppBlockDialogView::~AppBlockDialogView() {
  g_app_block_dialog_view = nullptr;
}

// static
AppBlockDialogView* AppBlockDialogView::GetActiveViewForTesting() {
  return g_app_block_dialog_view;
}
