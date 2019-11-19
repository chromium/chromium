// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_action/action_info.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/api/commands/commands_handler.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler_helpers.h"

namespace extensions {

namespace errors = manifest_errors;
namespace keys = manifest_keys;

namespace {

constexpr char kEnabled[] = "enabled";
constexpr char kDisabled[] = "disabled";

// The manifest data container for the ActionInfos for BrowserActions and
// PageActions.
struct ActionInfoData : public Extension::ManifestData {
  explicit ActionInfoData(std::unique_ptr<ActionInfo> action_info);
  ~ActionInfoData() override;

  // The action associated with the BrowserAction.
  std::unique_ptr<ActionInfo> action_info;
};

ActionInfoData::ActionInfoData(std::unique_ptr<ActionInfo> info)
    : action_info(std::move(info)) {}

ActionInfoData::~ActionInfoData() {
}

static const ActionInfo* GetActionInfo(const Extension* extension,
                                       const std::string& key) {
  ActionInfoData* data = static_cast<ActionInfoData*>(
      extension->GetManifestData(key));
  return data ? data->action_info.get() : NULL;
}

}  // namespace

ActionInfo::ActionInfo(Type type) : type(type), synthesized(false) {
  switch (type) {
    case TYPE_PAGE:
      default_state = STATE_DISABLED;
      break;
    case TYPE_BROWSER:
    case TYPE_ACTION:
      default_state = STATE_ENABLED;
      break;
  }
}

ActionInfo::ActionInfo(const ActionInfo& other) = default;

ActionInfo::~ActionInfo() {
}

// static
std::unique_ptr<ActionInfo> ActionInfo::Load(const Extension* extension,
                                             Type type,
                                             const base::DictionaryValue* dict,
                                             base::string16* error) {
  auto result = std::make_unique<ActionInfo>(type);

  // Read the page action |default_icon| (optional).
  // The |default_icon| value can be either dictionary {icon size -> icon path}
  // or non empty string value.
  if (dict->HasKey(keys::kActionDefaultIcon)) {
    const base::DictionaryValue* icons_value = NULL;
    std::string default_icon;
    if (dict->GetDictionary(keys::kActionDefaultIcon, &icons_value)) {
      if (!manifest_handler_helpers::LoadIconsFromDictionary(
              icons_value, &result->default_icon, error)) {
        return nullptr;
      }
    } else if (dict->GetString(keys::kActionDefaultIcon, &default_icon) &&
               manifest_handler_helpers::NormalizeAndValidatePath(
                   &default_icon)) {
      // Choose the most optimistic (highest) icon density regardless of the
      // actual icon resolution, whatever that happens to be. Code elsewhere
      // knows how to scale down to 19.
      result->default_icon.Add(extension_misc::EXTENSION_ICON_GIGANTOR,
                               default_icon);
    } else {
      *error = base::ASCIIToUTF16(errors::kInvalidActionDefaultIcon);
      return nullptr;
    }
  }

  // Read the page action title from |default_title| if present, |name| if not
  // (both optional).
  if (dict->HasKey(keys::kActionDefaultTitle)) {
    if (!dict->GetString(keys::kActionDefaultTitle, &result->default_title)) {
      *error = base::ASCIIToUTF16(errors::kInvalidActionDefaultTitle);
      return nullptr;
    }
  }

  // Read the action's default popup (optional).
  if (dict->HasKey(keys::kActionDefaultPopup)) {
    std::string url_str;
    if (!dict->GetString(keys::kActionDefaultPopup, &url_str)) {
      *error = base::ASCIIToUTF16(errors::kInvalidActionDefaultPopup);
      return nullptr;
    }

    if (!url_str.empty()) {
      // An empty string is treated as having no popup.
      result->default_popup_url = Extension::GetResourceURL(extension->url(),
                                                            url_str);
      if (!result->default_popup_url.is_valid()) {
        *error = base::ASCIIToUTF16(errors::kInvalidActionDefaultPopup);
        return nullptr;
      }
    } else {
      DCHECK(result->default_popup_url.is_empty())
          << "Shouldn't be possible for the popup to be set.";
    }
  }

  if (dict->HasKey(keys::kActionDefaultState)) {
    // The default_state key is only valid for TYPE_ACTION; throw an error for
    // others.
    if (type != TYPE_ACTION) {
      *error = base::ASCIIToUTF16(errors::kDefaultStateShouldNotBeSet);
      return nullptr;
    }

    std::string default_state;
    if (!dict->GetString(keys::kActionDefaultState, &default_state) ||
        !(default_state == kEnabled || default_state == kDisabled)) {
      *error = base::ASCIIToUTF16(errors::kInvalidActionDefaultState);
      return nullptr;
    }
    result->default_state = default_state == kEnabled
                                ? ActionInfo::STATE_ENABLED
                                : ActionInfo::STATE_DISABLED;
  }

  return result;
}

// static
const ActionInfo* ActionInfo::GetAnyActionInfo(const Extension* extension) {
  // TODO(devlin): Since all actions are mutually exclusive, we can store
  // them all under the same key. For now, we don't do that because some callers
  // need to differentiate between action types.
  const ActionInfo* info = GetActionInfo(extension, keys::kBrowserAction);
  if (info)
    return info;
  info = GetActionInfo(extension, keys::kPageAction);
  if (info)
    return info;
  return GetActionInfo(extension, keys::kAction);
}

// static
const ActionInfo* ActionInfo::GetExtensionActionInfo(
    const Extension* extension) {
  return GetActionInfo(extension, keys::kAction);
}

// static
const ActionInfo* ActionInfo::GetBrowserActionInfo(const Extension* extension) {
  return GetActionInfo(extension, keys::kBrowserAction);
}

const ActionInfo* ActionInfo::GetPageActionInfo(const Extension* extension) {
  return GetActionInfo(extension, keys::kPageAction);
}

// static
void ActionInfo::SetExtensionActionInfo(Extension* extension,
                                        std::unique_ptr<ActionInfo> info) {
  extension->SetManifestData(keys::kAction,
                             std::make_unique<ActionInfoData>(std::move(info)));
}

// static
void ActionInfo::SetBrowserActionInfo(Extension* extension,
                                      std::unique_ptr<ActionInfo> info) {
  extension->SetManifestData(keys::kBrowserAction,
                             std::make_unique<ActionInfoData>(std::move(info)));
}

// static
void ActionInfo::SetPageActionInfo(Extension* extension,
                                   std::unique_ptr<ActionInfo> info) {
  extension->SetManifestData(keys::kPageAction,
                             std::make_unique<ActionInfoData>(std::move(info)));
}

// static
bool ActionInfo::IsVerboseInstallMessage(const Extension* extension) {
  const ActionInfo* page_action_info = GetPageActionInfo(extension);
  return page_action_info &&
      (CommandsInfo::GetPageActionCommand(extension) ||
       !page_action_info->default_icon.empty());
}

}  // namespace extensions
