// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/reload_page_dialog_controller.h"

#include "base/barrier_closure.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/ui/extensions/extension_dialog_utils.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_icon_placeholder.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

// Whether the dialog should be accepted without showing it on tests.
std::optional<bool> g_accept_bubble_for_testing_ = std::nullopt;

// The size of the extension icon.
constexpr int kIconSize = extension_misc::EXTENSION_ICON_SMALLISH;

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
    content::WebContents* web_contents,
    content::BrowserContext* browser_context)
    : web_contents_(web_contents), browser_context_(browser_context) {}

ReloadPageDialogController::~ReloadPageDialogController() = default;

void ReloadPageDialogController::TriggerShow(
    const std::vector<const Extension*>& extensions) {
  // For testing, callers can use AcceptDialogForTesting() to pre-determine
  // the dialog's result. This bypasses showing the dialog.
  if (g_accept_bubble_for_testing_.has_value()) {
    if (*g_accept_bubble_for_testing_) {
      OnAcceptSelected();
    }
    return;
  }

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

  int extensions_count = extensions.size();
#if BUILDFLAG(IS_ANDROID)
  // On Android, the multi-extension reload page message uses a generic
  // puzzle-piece icon, so we don't need to load the individual extension icons.
  // We can just show the message immediately.
  if (extensions.size() > 1) {
    for (const Extension* extension : extensions) {
      ExtensionInfo extension_info;
      extension_info.id = extension->id();
      extension_info.name = extension->name();
      extensions_info_.push_back(extension_info);
    }
    Show();
    return;
  }
#endif

  // We need to load the icon for each extension before showing the dialog.
  // Since icon loading is asynchronous, we use a BarrierClosure. It acts as
  // a counter and will call this->Show() only after all icon-loading callbacks
  // have completed.
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      extensions_count, base::BindOnce(&ReloadPageDialogController::Show,
                                       weak_ptr_factory_.GetWeakPtr()));
  auto* image_loader = ImageLoader::Get(browser_context_);

  for (const Extension* extension : extensions) {
    ExtensionResource icon = IconsInfo::GetIconResource(
        extension, kIconSize, ExtensionIconSet::Match::kBigger);
    if (icon.empty()) {
      gfx::Image placeholder_icon =
          ExtensionIconPlaceholder::CreateImage(kIconSize, extension->name());
      OnExtensionIconLoaded(extension->id(), extension->name(), barrier_closure,
                            placeholder_icon);
    } else {
      gfx::Size max_size(kIconSize, kIconSize);
      image_loader->LoadImageAsync(
          extension, icon, max_size,
          base::BindOnce(&ReloadPageDialogController::OnExtensionIconLoaded,
                         weak_ptr_factory_.GetWeakPtr(), extension->id(),
                         extension->name(), barrier_closure));
    }
  }
}

// static
base::AutoReset<std::optional<bool>>
ReloadPageDialogController::AcceptDialogForTesting(bool accept_dialog) {
  return base::AutoReset<std::optional<bool>>(&g_accept_bubble_for_testing_,
                                              accept_dialog);
}

void ReloadPageDialogController::Show() {
#if BUILDFLAG(IS_ANDROID)
  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::RELOAD_PAGE,
      base::BindOnce(&ReloadPageDialogController::OnAcceptSelected,
                     weak_ptr_factory_.GetWeakPtr()),
      base::DoNothing());

  message_->SetTitle(GetTitle(extensions_info_));
  message_->SetPrimaryButtonText(
      l10n_util::GetStringUTF16(IDS_EXTENSION_RELOAD_PAGE_BUBBLE_OK_BUTTON));

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    int extensions_count = extensions_info_.size();
    if (extensions_count == 1 && !extensions_info_[0].icon.IsEmpty()) {
      message_->SetIcon(extensions_info_[0].icon.AsBitmap());
    } else {
      // For multiple extensions, set the icon to the extensions puzzle icon.
      message_->SetIcon(
          gfx::Image(
              ui::ImageModel::FromVectorIcon(vector_icons::kExtensionIcon,
                                             ui::kColorIcon, kIconSize)
                  .Rasterize(&web_contents_->GetColorProvider()))
              .AsBitmap());
    }
  }

  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents_, messages::MessageScopeType::NAVIGATION,
      messages::MessagePriority::kNormal);

#else
  ui::DialogModel::Builder dialog_builder;
  dialog_builder.SetTitle(GetTitle(extensions_info_))
      .AddOkButton(base::BindOnce(&ReloadPageDialogController::OnAcceptSelected,
                                  weak_ptr_factory_.GetWeakPtr()),
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
    } else {
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

  ShowDialog(web_contents_->GetTopLevelNativeWindow(), extension_ids,
             dialog_builder.Build());
#endif  // BUILDFLAG(IS_ANDROID)
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

void ReloadPageDialogController::OnAcceptSelected() {
  web_contents_->GetController().Reload(content::ReloadType::NORMAL, false);
}

}  // namespace extensions
