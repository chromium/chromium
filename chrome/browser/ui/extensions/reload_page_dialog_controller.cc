// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/reload_page_dialog_controller.h"

#include "base/barrier_closure.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/ui/extensions/extension_dialog_utils.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_icon_placeholder.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"

namespace {

std::u16string GetTitle(
    const std::vector<extensions::ReloadPageDialogController::ExtensionInfo>&
        extensions_info) {
  if (!base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl) ||
      extensions_info.empty()) {
    return l10n_util::GetStringUTF16(
        IDS_EXTENSION_SITE_RELOAD_PAGE_BUBBLE_HEADING);
  }

  if (extensions_info.size() == 1) {
    std::u16string extension_name =
        extensions::util::GetFixupExtensionNameForUIDisplay(
            base::UTF8ToUTF16(extensions_info[0].name));
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSION_RELOAD_PAGE_BUBBLE_ALLOW_SINGLE_EXTENSION_TITLE,
        extension_name);
  }

  return l10n_util::GetStringUTF16(
      IDS_EXTENSION_RELOAD_PAGE_BUBBLE_ALLOW_MULTIPLE_EXTENSIONS_TITLE);
}

}  // namespace

namespace extensions {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kReloadPageDialogOkButtonElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kReloadPageDialogCancelButtonElementId);

ReloadPageDialogController::ReloadPageDialogController(
    gfx::NativeWindow parent,
    content::BrowserContext* browser_context,
    base::OnceClosure callback)
    : parent_(parent),
      browser_context_(browser_context),
      on_dialog_accepted_(std::move(callback)) {}
ReloadPageDialogController::~ReloadPageDialogController() = default;

void ReloadPageDialogController::TriggerShow(
    const std::vector<const Extension*>& extensions) {
  if (!base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    for (const Extension* extension : extensions) {
      ExtensionInfo extension_info;
      extension_info.id = extension->id();
      extensions_info_.push_back(extension_info);
    }
    Show();
    return;
  }

  // We need to load the icon for each extension before showing the dialog.
  // Since icon loading is asynchronous, we use a BarrierClosure. It acts as
  // a counter and will call this->Show() only after all icon-loading callbacks
  // have completed.
  int extensions_count = extensions.size();
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      extensions_count, base::BindOnce(&ReloadPageDialogController::Show,
                                       weak_ptr_factory_.GetWeakPtr()));
  const int icon_size = extension_misc::EXTENSION_ICON_SMALLISH;
  auto* image_loader = ImageLoader::Get(browser_context_);

  for (const Extension* extension : extensions) {
    ExtensionResource icon = IconsInfo::GetIconResource(
        extension, icon_size, ExtensionIconSet::Match::kBigger);
    if (icon.empty()) {
      gfx::Image placeholder_icon =
          ExtensionIconPlaceholder::CreateImage(icon_size, extension->name());
      OnExtensionIconLoaded(extension->id(), extension->name(), barrier_closure,
                            placeholder_icon);
    } else {
      gfx::Size max_size(icon_size, icon_size);
      image_loader->LoadImageAsync(
          extension, icon, max_size,
          base::BindOnce(&ReloadPageDialogController::OnExtensionIconLoaded,
                         weak_ptr_factory_.GetWeakPtr(), extension->id(),
                         extension->name(), barrier_closure));
    }
  }
}

void ReloadPageDialogController::Show() {
  ui::DialogModel::Builder dialog_builder;
  dialog_builder.SetTitle(GetTitle(extensions_info_))
      .AddOkButton(base::BindOnce(std::move(on_dialog_accepted_)),
                   ui::DialogModel::Button::Params()
                       .SetLabel(l10n_util::GetStringUTF16(
                           IDS_EXTENSION_RELOAD_PAGE_BUBBLE_OK_BUTTON))
                       .SetId(kReloadPageDialogOkButtonElementId))
      .AddCancelButton(base::DoNothing(),
                       ui::DialogModel::Button::Params().SetId(
                           kReloadPageDialogCancelButtonElementId));

  int extensions_count = extensions_info_.size();

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    if (extensions_count == 1) {
      dialog_builder.SetIcon(
          ui::ImageModel::FromImage(extensions_info_[0].icon));
    } else if (extensions_count > 1) {
      for (auto extension_info : extensions_info_) {
        dialog_builder.AddMenuItem(
            ui::ImageModel::FromImage(extension_info.icon),
            util::GetFixupExtensionNameForUIDisplay(extension_info.name),
            base::DoNothing(),
            ui::DialogModelMenuItem::Params().SetIsEnabled(false));
      }
    }
  }

  std::vector<ExtensionId> extension_ids;
  extension_ids.reserve(extensions_info_.size());
  for (const auto& info : extensions_info_) {
    extension_ids.push_back(info.id);
  }

  ShowDialog(parent_, extension_ids, dialog_builder.Build());
}

void ReloadPageDialogController::OnExtensionIconLoaded(
    const ExtensionId& extension_id,
    const std::string& extension_name,
    base::OnceClosure done_callback,
    const gfx::Image& icon) {
  ExtensionInfo extension_info;
  extension_info.id = extension_id;
  extension_info.name = extension_name;
  extension_info.icon = icon;
  extensions_info_.push_back(extension_info);
  std::move(done_callback).Run();
}

}  // namespace extensions
