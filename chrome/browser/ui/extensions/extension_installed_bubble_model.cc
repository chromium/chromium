// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_installed_bubble_model.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/omnibox/omnibox_handler.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/command.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

std::optional<extensions::Command> CommandForExtensionAction(
    const extensions::Extension* extension,
    Profile* profile) {
  const auto* info = extensions::ActionInfo::GetExtensionActionInfo(extension);

  if (!info)
    return std::nullopt;

  auto* service = extensions::CommandService::Get(profile);
  extensions::Command command;

  if (service->GetExtensionActionCommand(extension->id(), info->type,
                                         extensions::CommandService::ACTIVE,
                                         &command, nullptr)) {
    return command;
  }

  return std::nullopt;
}

std::u16string MakeHowToUseText(const extensions::ActionInfo* action,
                                std::optional<extensions::Command> command,
                                const std::string& keyword) {
  std::u16string extra;
  if (command.has_value())
    extra = command->accelerator().GetShortcutText();

  int message_id = 0;
  if (action && action->type == extensions::ActionInfo::Type::kBrowser) {
    message_id =
        extra.empty()
            ? IDS_EXTENSION_INSTALLED_BROWSER_ACTION_INFO
            : IDS_EXTENSION_INSTALLED_BROWSER_ACTION_INFO_WITH_SHORTCUT;
  } else if (action && action->type == extensions::ActionInfo::Type::kPage) {
    message_id = extra.empty()
                     ? IDS_EXTENSION_INSTALLED_PAGE_ACTION_INFO
                     : IDS_EXTENSION_INSTALLED_PAGE_ACTION_INFO_WITH_SHORTCUT;
  } else if (!keyword.empty()) {
    extra = base::UTF8ToUTF16(keyword);
    message_id = IDS_EXTENSION_INSTALLED_OMNIBOX_KEYWORD_INFO;
  }

  if (!message_id)
    return std::u16string();

  return extra.empty() ? l10n_util::GetStringUTF16(message_id)
                       : l10n_util::GetStringFUTF16(message_id, extra);
}

}  // namespace

ExtensionInstalledBubbleModel::ExtensionInstalledBubbleModel(
    Profile* profile,
    const extensions::Extension* extension,
    const SkBitmap& icon)
    : icon_(icon),
      extension_id_(extension->id()),
      extension_name_(extension->name()) {
  const std::string& keyword = extensions::OmniboxInfo::GetKeyword(extension);
  std::optional<extensions::Command> command =
      CommandForExtensionAction(extension, profile);
  const auto* action_info =
      extensions::ActionInfo::GetExtensionActionInfo(extension);

  const bool toolbar_action = !!action_info;

  anchor_to_action_ = toolbar_action;
  anchor_to_omnibox_ = !toolbar_action && !keyword.empty();

  show_how_to_use_ =
      (toolbar_action && !action_info->synthesized) || !keyword.empty();
  // If there's a shortcut, don't show the how-to-manage text because it
  // clutters the bubble.
  show_how_to_manage_ = !command.has_value() || anchor_to_omnibox_;
  show_key_binding_ = command.has_value();

  show_sign_in_promo_ = extensions::util::ShouldSync(extension, profile) &&
                        signin::ShouldShowSyncPromo(*profile);

  if (show_how_to_use_)
    how_to_use_text_ = MakeHowToUseText(action_info, command, keyword);
}

ExtensionInstalledBubbleModel::~ExtensionInstalledBubbleModel() = default;

std::u16string ExtensionInstalledBubbleModel::GetHowToUseText() const {
  DCHECK(show_how_to_use_);
  return how_to_use_text_;
}

gfx::ImageSkia ExtensionInstalledBubbleModel::MakeIconOfSize(
    const gfx::Size& wanted) const {
  gfx::Size size(icon_.width(), icon_.height());
  if (size.width() > wanted.width() || size.height() > wanted.height())
    size.SetSize(wanted.width(), wanted.height());

  return gfx::ImageSkiaOperations::CreateResizedImage(
      gfx::ImageSkia::CreateFrom1xBitmap(icon_),
      skia::ImageOperations::RESIZE_BEST, size);
}
