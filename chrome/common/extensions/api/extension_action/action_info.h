// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_ACTION_INFO_H_
#define CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_ACTION_INFO_H_

#include <memory>
#include <string>

#include "base/strings/string16.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace extensions {

class Extension;

struct ActionInfo {
  // The types of extension actions.
  enum Type {
    TYPE_ACTION,
    TYPE_BROWSER,
    TYPE_PAGE,
  };

  enum DefaultState {
    STATE_ENABLED,
    STATE_DISABLED,
  };

  explicit ActionInfo(Type type);
  ActionInfo(const ActionInfo& other);
  ~ActionInfo();

  // Loads an ActionInfo from the given DictionaryValue.
  static std::unique_ptr<ActionInfo> Load(const Extension* extension,
                                          Type type,
                                          const base::DictionaryValue* dict,
                                          base::string16* error);

  // Returns any action associated with the extension, whether it's specified
  // under the "page_action", "browser_action", or "action" key.
  // TODO(devlin): This is a crutch while moving away from the distinct action
  // types. Remove it when that's done.
  static const ActionInfo* GetAnyActionInfo(const Extension* extension);

  // Returns the action specified under the "action" key, if any.
  static const ActionInfo* GetExtensionActionInfo(const Extension* extension);

  // Returns the extension's browser action, if any.
  static const ActionInfo* GetBrowserActionInfo(const Extension* extension);

  // Returns the extension's page action, if any.
  static const ActionInfo* GetPageActionInfo(const Extension* extension);

  // Sets the extension's action.
  static void SetExtensionActionInfo(Extension* extension,
                                     std::unique_ptr<ActionInfo> info);

  // Sets the extension's browser action.
  static void SetBrowserActionInfo(Extension* extension,
                                   std::unique_ptr<ActionInfo> info);

  // Sets the extension's page action.
  static void SetPageActionInfo(Extension* extension,
                                std::unique_ptr<ActionInfo> info);

  // Returns true if the extension needs a verbose install message because
  // of its page action.
  static bool IsVerboseInstallMessage(const Extension* extension);

  // The key this action corresponds to. NOTE: You should only use this if you
  // care about the actual manifest key. Use the other members (like
  // |default_state| for querying general info.
  const Type type;

  // Empty implies the key wasn't present.
  ExtensionIconSet default_icon;
  std::string default_title;
  GURL default_popup_url;

  // Specifies if the action applies to all web pages ("enabled") or
  // only specific pages ("disabled"). Only applies to the "action" key.
  DefaultState default_state;
  // Whether or not this action was synthesized to force visibility.
  bool synthesized;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_ACTION_INFO_H_
