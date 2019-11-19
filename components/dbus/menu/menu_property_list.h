// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_MENU_MENU_PROPERTY_LIST_H_
#define COMPONENTS_DBUS_MENU_MENU_PROPERTY_LIST_H_

#include <map>
#include <string>
#include <vector>

#include "components/dbus/properties/types.h"

using MenuPropertyList = std::vector<std::string>;
using MenuItemProperties = std::map<std::string, DbusVariant>;
using MenuPropertyChanges = std::map<int32_t, MenuPropertyList>;

namespace ui {
class MenuModel;
}

// Computes properties for the menu item with index |i| in |menu|.
COMPONENT_EXPORT(DBUS)
MenuItemProperties ComputeMenuPropertiesForMenuItem(ui::MenuModel* menu, int i);

// Given inputs |old_properties| and |new_properties|, computes outputs
// |item_updated_props| and |item_removed_props| suitable for use in
// com.canonical.dbusmenu.ItemsPropertiesUpdated.
COMPONENT_EXPORT(DBUS)
void ComputeMenuPropertyChanges(const MenuItemProperties& old_properties,
                                const MenuItemProperties& new_properties,
                                MenuPropertyList* item_updated_props,
                                MenuPropertyList* item_removed_props);

#endif  // COMPONENTS_DBUS_MENU_MENU_PROPERTY_LIST_H_
