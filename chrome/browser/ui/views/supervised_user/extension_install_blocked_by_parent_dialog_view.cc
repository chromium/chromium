// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/supervised_user/extension_install_blocked_by_parent_dialog_view.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/i18n/message_formatter.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace chrome {

void ShowExtensionInstallBlockedByParentDialog(
    ExtensionInstalledBlockedByParentDialogAction action,
    const extensions::Extension* extension,
    content::WebContents* web_contents,
    base::OnceClosure done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto dialog = std::make_unique<ExtensionInstallBlockedByParentDialogView>(
      action, extension, std::move(done_callback));
  gfx::NativeWindow parent_window =
      web_contents ? web_contents->GetTopLevelNativeWindow() : nullptr;
  views::Widget* widget =
      parent_window ? constrained_window::CreateBrowserModalDialogViews(
                          dialog.release(), parent_window)
                    : views::DialogDelegate::CreateDialogWidget(
                          dialog.release(), nullptr, nullptr);
  widget->Show();
}

}  // namespace chrome

ExtensionInstallBlockedByParentDialogView::
    ExtensionInstallBlockedByParentDialogView(
        chrome::ExtensionInstalledBlockedByParentDialogAction action,
        const extensions::Extension* extension,
        base::OnceClosure done_callback)
    : extension_(extension),
      action_(action),
      done_callback_(std::move(done_callback)) {
  SetButtons(ui::DIALOG_BUTTON_CANCEL);
  SetDefaultButton(ui::DIALOG_BUTTON_CANCEL);
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, l10n_util::GetStringUTF16(IDS_OK));
  set_draggable(true);

  SetIcon(gfx::CreateVectorIcon(chromeos::kNotificationSupervisedUserIcon,
                                SK_ColorDKGRAY));
  SetShowIcon(true);
  ConfigureTitle();
  CreateContents();
}

ExtensionInstallBlockedByParentDialogView::
    ~ExtensionInstallBlockedByParentDialogView() {
  if (done_callback_)
    std::move(done_callback_).Run();
}

gfx::Size ExtensionInstallBlockedByParentDialogView::CalculatePreferredSize()
    const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

ui::ModalType ExtensionInstallBlockedByParentDialogView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

void ExtensionInstallBlockedByParentDialogView::ConfigureTitle() {
  base::string16 title_string;
  switch (action_) {
    case chrome::ExtensionInstalledBlockedByParentDialogAction::kAdd:
      // The user is trying to add/install the extension/app
      title_string = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_INSTALL_BLOCKED_BY_PARENT_PROMPT_TITLE,
          GetExtensionTypeString());
      break;
    case chrome::ExtensionInstalledBlockedByParentDialogAction::kEnable:
      // The user is trying to enable the extension/app
      title_string = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_ENABLE_BLOCKED_BY_PARENT_PROMPT_TITLE,
          GetExtensionTypeString());
      break;
  }
  SetTitle(title_string);
}

void ExtensionInstallBlockedByParentDialogView::CreateContents() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  base::string16 body_string;
  switch (action_) {
    case chrome::ExtensionInstalledBlockedByParentDialogAction::kAdd:
      // The user is trying to add/install the extension/app
      body_string = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_INSTALL_BLOCKED_BY_PARENT_PROMPT_MESSAGE,
          GetExtensionTypeString());
      break;
    case chrome::ExtensionInstalledBlockedByParentDialogAction::kEnable:
      // The user is trying to enable the extension/app
      body_string = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_ENABLE_BLOCKED_BY_PARENT_PROMPT_MESSAGE,
          GetExtensionTypeString());
      break;
  }

  icon_ = gfx::CreateVectorIcon(chromeos::kNotificationSupervisedUserIcon,
                                SK_ColorDKGRAY);

  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const gfx::Insets content_insets =
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT);

  set_margins(gfx::Insets(content_insets.top(), content_insets.left(),
                          content_insets.bottom(), content_insets.right()));

  auto* message_body_label = AddChildView(std::make_unique<views::Label>(
      body_string, views::style::CONTEXT_DIALOG_BODY_TEXT));
  message_body_label->SetMultiLine(true);
  message_body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

base::string16
ExtensionInstallBlockedByParentDialogView::GetExtensionTypeString() {
  return l10n_util::GetStringUTF16(
      extension_->is_app()
          ? IDS_PARENT_PERMISSION_PROMPT_EXTENSION_TYPE_APP
          : IDS_PARENT_PERMISSION_PROMPT_EXTENSION_TYPE_EXTENSION);
}
