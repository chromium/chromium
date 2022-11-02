// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/menu/menu_property_list.h"

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/menu_label_accelerator_util_linux.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/image/image.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"       // nogncheck
#include "ui/ozone/public/platform_menu_utils.h"  // nogncheck
#endif

namespace {

std::string ToDBusKeySym(ui::KeyboardCode code) {
#if BUILDFLAG(IS_OZONE)
  if (const auto* const platorm_menu_utils =
          ui::OzonePlatform::GetInstance()->GetPlatformMenuUtils()) {
    return platorm_menu_utils->ToDBusKeySym(code);
  }
#endif
  return {};
}

std::vector<DbusString> GetDbusMenuShortcut(ui::Accelerator accelerator) {
  auto dbus_key_sym = ToDBusKeySym(accelerator.key_code());
  if (dbus_key_sym.empty())
    return {};

  std::vector<DbusString> parts;
  if (accelerator.IsCtrlDown())
    parts.emplace_back("Control");
  if (accelerator.IsAltDown())
    parts.emplace_back("Alt");
  if (accelerator.IsShiftDown())
    parts.emplace_back("Shift");
  if (accelerator.IsCmdDown())
    parts.emplace_back("Super");
  parts.emplace_back(dbus_key_sym);
  return parts;
}

}  // namespace

MenuItemProperties ComputeMenuPropertiesForMenuItem(ui::MenuModel* menu,
                                                    size_t i) {
  // Properties should only be set if they differ from the default values.
  MenuItemProperties properties;

  // The dbusmenu interface has no concept of a "sublabel", "minor text", or
  // "minor icon" like MenuModel has.  Ignore these rather than trying to
  // merge them with the regular label and icon.
  std::u16string label = menu->GetLabelAt(i);
  if (!label.empty()) {
    properties["label"] = MakeDbusVariant(DbusString(
        ui::ConvertAcceleratorsFromWindowsStyle(base::UTF16ToUTF8(label))));
  }

  if (!menu->IsEnabledAt(i))
    properties["enabled"] = MakeDbusVariant(DbusBoolean(false));
  if (!menu->IsVisibleAt(i))
    properties["visible"] = MakeDbusVariant(DbusBoolean(false));

  ui::ImageModel icon = menu->GetIconAt(i);
  if (icon.IsImage()) {
    properties["icon-data"] =
        MakeDbusVariant(DbusByteArray(icon.GetImage().As1xPNGBytes()));
  }

  ui::Accelerator accelerator;
  if (menu->GetAcceleratorAt(i, &accelerator)) {
    auto parts = GetDbusMenuShortcut(accelerator);
    if (!parts.empty()) {
      properties["shortcut"] = MakeDbusVariant(
          MakeDbusArray(DbusArray<DbusString>(std::move(parts))));
    }
  }

  switch (menu->GetTypeAt(i)) {
    case ui::MenuModel::TYPE_COMMAND:
    case ui::MenuModel::TYPE_HIGHLIGHTED:
    case ui::MenuModel::TYPE_TITLE:
      // Nothing special to do.
      break;
    case ui::MenuModel::TYPE_CHECK:
    case ui::MenuModel::TYPE_RADIO:
      properties["toggle-type"] = MakeDbusVariant(DbusString(
          menu->GetTypeAt(i) == ui::MenuModel::TYPE_CHECK ? "checkmark"
                                                          : "radio"));
      properties["toggle-state"] =
          MakeDbusVariant(DbusInt32(menu->IsItemCheckedAt(i) ? 1 : 0));
      break;
    case ui::MenuModel::TYPE_SEPARATOR:
      // The dbusmenu interface doesn't have multiple types of separators like
      // MenuModel.  Just use a regular separator in all cases.
      properties["type"] = MakeDbusVariant(DbusString("separator"));
      break;
    case ui::MenuModel::TYPE_BUTTON_ITEM:
      // This type of menu represents a row of buttons, but the dbusmenu
      // interface has no equivalent of this.  Ignore these items for now
      // since there's currently no uses of it that plumb into this codepath.
      // If there are button menu items in the future, we'd have to fake them
      // with multiple menu items.
      NOTIMPLEMENTED();
      break;
    case ui::MenuModel::TYPE_SUBMENU:
    case ui::MenuModel::TYPE_ACTIONABLE_SUBMENU:
      properties["children-display"] = MakeDbusVariant(DbusString("submenu"));
      break;
  }

  return properties;
}

void ComputeMenuPropertyChanges(const MenuItemProperties& old_properties,
                                const MenuItemProperties& new_properties,
                                MenuPropertyList* item_updated_props,
                                MenuPropertyList* item_removed_props) {
  // Compute updated and removed properties.
  for (const auto& pair : old_properties) {
    const std::string& key = pair.first;
    auto new_it = new_properties.find(key);
    if (new_it != new_properties.end()) {
      if (new_it->second != pair.second)
        item_updated_props->push_back(key);
    } else {
      item_removed_props->push_back(key);
    }
  }
  // Compute added properties.
  for (const auto& pair : new_properties) {
    const std::string& key = pair.first;
    if (!base::Contains(old_properties, key))
      item_updated_props->push_back(key);
  }
}
