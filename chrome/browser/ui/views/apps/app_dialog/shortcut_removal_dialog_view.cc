// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_dialog/shortcut_removal_dialog_view.h"

#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

ShortcutRemovalDialogView* g_shortcut_removal_dialog_view_for_testing = nullptr;

std::u16string GetWindowTitleForShortcut(Profile* profile,
                                         const apps::ShortcutId& shortcut_id) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  apps::ShortcutView shortcut =
      proxy->ShortcutRegistryCache()->GetShortcut(shortcut_id);
  std::u16string shortcut_name = base::UTF8ToUTF16(shortcut->name.value());
  std::u16string host_app_name;
  proxy->AppRegistryCache().ForOneApp(
      shortcut->host_app_id, [&host_app_name](const apps::AppUpdate& update) {
        host_app_name = base::UTF8ToUTF16(update.ShortName());
      });
  return l10n_util::GetStringFUTF16(IDS_PROMP_SHORTCUT_REMOVAL_TITLE,
                                    shortcut_name, host_app_name);
}

}  // namespace

// static
base::WeakPtr<views::Widget> apps::ShortcutRemovalDialog::Create(
    Profile* profile,
    const apps::ShortcutId& shortcut_id,
    gfx::ImageSkia icon_with_badge,
    gfx::NativeWindow parent_window,
    base::WeakPtr<apps::ShortcutRemovalDialog> shortcut_removal_dialog) {
  auto* dialog_view = new ShortcutRemovalDialogView(
      profile, shortcut_id, icon_with_badge, shortcut_removal_dialog);
  views::Widget* widget = constrained_window::CreateBrowserModalDialogViews(
      dialog_view, parent_window);
  widget->Show();
  return widget->GetWeakPtr();
}

ShortcutRemovalDialogView::ShortcutRemovalDialogView(
    Profile* profile,
    const apps::ShortcutId& shortcut_id,
    gfx::ImageSkia icon_with_badge,
    base::WeakPtr<apps::ShortcutRemovalDialog> shortcut_removal_dialog)
    : AppDialogView(ui::ImageModel::FromImageSkia(icon_with_badge)),
      profile_(profile),
      shortcut_removal_dialog_(shortcut_removal_dialog) {
  profile_observation_.Observe(profile);

  SetModalType(ui::MODAL_TYPE_WINDOW);
  SetTitle(GetWindowTitleForShortcut(profile, shortcut_id));

  SetCloseCallback(base::BindOnce(&ShortcutRemovalDialogView::OnDialogCancelled,
                                  base::Unretained(this)));
  SetCancelCallback(base::BindOnce(
      &ShortcutRemovalDialogView::OnDialogCancelled, base::Unretained(this)));
  SetAcceptCallback(base::BindOnce(&ShortcutRemovalDialogView::OnDialogAccepted,
                                   base::Unretained(this)));

  InitializeView(profile, shortcut_id);

  g_shortcut_removal_dialog_view_for_testing = this;
}

ShortcutRemovalDialogView::~ShortcutRemovalDialogView() {
  g_shortcut_removal_dialog_view_for_testing = nullptr;
}

// static
ShortcutRemovalDialogView*
ShortcutRemovalDialogView::GetLastCreatedViewForTesting() {
  return g_shortcut_removal_dialog_view_for_testing;
}

void ShortcutRemovalDialogView::OnProfileWillBeDestroyed(Profile* profile) {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ShortcutRemovalDialogView::InitializeView(
    Profile* profile,
    const apps::ShortcutId& shortcut_id) {
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_PROMP_REMOVE_SHORTCUT_BUTTON));
}

void ShortcutRemovalDialogView::OnDialogCancelled() {
  if (shortcut_removal_dialog()) {
    shortcut_removal_dialog()->OnDialogClosed(false /* remove */);
  }
}

void ShortcutRemovalDialogView::OnDialogAccepted() {
  if (shortcut_removal_dialog()) {
    shortcut_removal_dialog()->OnDialogClosed(true /* remove */);
  }
}
