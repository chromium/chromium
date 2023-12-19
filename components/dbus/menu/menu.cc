// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/menu/menu.h"

#include <limits>
#include <memory>
#include <set>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dbus/properties/dbus_properties.h"
#include "components/dbus/properties/success_barrier_callback.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/simple_menu_model.h"

namespace {

// Interfaces.
const char kInterfaceDbusMenu[] = "com.canonical.dbusmenu";

// Methods.
const char kMethodAboutToShow[] = "AboutToShow";
const char kMethodAboutToShowGroup[] = "AboutToShowGroup";
const char kMethodEvent[] = "Event";
const char kMethodEventGroup[] = "EventGroup";
const char kMethodGetGroupProperties[] = "GetGroupProperties";
const char kMethodGetLayout[] = "GetLayout";
const char kMethodGetProperty[] = "GetProperty";

// Properties.
const char kPropertyIconThemePath[] = "IconThemePath";
const char kPropertyMenuStatus[] = "Status";
const char kPropertyTextDirection[] = "TextDirection";
const char kPropertyVersion[] = "Version";

// Property values.
const char kPropertyValueStatusNormal[] = "normal";
uint32_t kPropertyValueVersion = 3;

// Signals.
const char kSignalItemsPropertiesUpdated[] = "ItemsPropertiesUpdated";
const char kSignalLayoutUpdated[] = "LayoutUpdated";

// Creates a variant with the default value for |property_name|, or an empty
// variant if |property_name| is invalid.
DbusVariant CreateDefaultPropertyValue(const std::string& property_name) {
  if (property_name == "type")
    return MakeDbusVariant(DbusString("standard"));
  if (property_name == "label")
    return MakeDbusVariant(DbusString(""));
  if (property_name == "enabled")
    return MakeDbusVariant(DbusBoolean(true));
  if (property_name == "visible")
    return MakeDbusVariant(DbusBoolean(true));
  if (property_name == "icon-name")
    return MakeDbusVariant(DbusString(""));
  if (property_name == "icon-data")
    return MakeDbusVariant(DbusByteArray());
  if (property_name == "shortcut")
    return MakeDbusVariant(DbusArray<DbusArray<DbusString>>());
  if (property_name == "toggle-type")
    return MakeDbusVariant(DbusString(""));
  if (property_name == "toggle-state")
    return MakeDbusVariant(DbusInt32(-1));
  if (property_name == "children-display")
    return MakeDbusVariant(DbusString(""));
  return DbusVariant();
}

DbusString DbusTextDirection() {
  return DbusString(base::i18n::IsRTL() ? "rtl " : "ltr");
}

void WriteRemovedProperties(dbus::MessageWriter* writer,
                            const MenuPropertyChanges& removed_props) {
  dbus::MessageWriter removed_props_writer(nullptr);
  writer->OpenArray("(ias)", &removed_props_writer);
  for (const auto& pair : removed_props) {
    dbus::MessageWriter struct_writer(nullptr);
    removed_props_writer.OpenStruct(&struct_writer);
    struct_writer.AppendInt32(pair.first);
    struct_writer.AppendArrayOfStrings(pair.second);
    removed_props_writer.CloseContainer(&struct_writer);
  }
  writer->CloseContainer(&removed_props_writer);
}

}  // namespace

DbusMenu::MenuItem::MenuItem(int32_t id,
                             std::map<std::string, DbusVariant>&& properties,
                             std::vector<int32_t>&& children,
                             ui::MenuModel* menu,
                             ui::MenuModel* containing_menu,
                             size_t containing_menu_index)
    : id(id),
      properties(std::move(properties)),
      children(std::move(children)),
      menu(menu),
      containing_menu(containing_menu),
      containing_menu_index(containing_menu_index) {}

DbusMenu::MenuItem::~MenuItem() = default;

DbusMenu::ScopedMethodResponse::ScopedMethodResponse(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender)
    : method_call_(method_call),
      response_sender_(std::move(response_sender)),
      reader_(method_call_) {}

DbusMenu::ScopedMethodResponse::~ScopedMethodResponse() {
  std::move(response_sender_).Run(std::move(response_));
}

dbus::MessageWriter& DbusMenu::ScopedMethodResponse::Writer() {
  EnsureResponse();
  if (!writer_)
    writer_ = std::make_unique<dbus::MessageWriter>(response_.get());
  return *writer_;
}

void DbusMenu::ScopedMethodResponse::EnsureResponse() {
  if (!response_)
    response_ = dbus::Response::FromMethodCall(method_call_);
}

DbusMenu::DbusMenu(dbus::ExportedObject* exported_object,
                   InitializedCallback callback)
    : menu_(exported_object) {
  SetModel(nullptr, false);

  static constexpr struct {
    const char* name;
    void (DbusMenu::*callback)(ScopedMethodResponse*);
  } methods[7] = {
      {kMethodAboutToShow, &DbusMenu::OnAboutToShow},
      {kMethodAboutToShowGroup, &DbusMenu::OnAboutToShowGroup},
      {kMethodEvent, &DbusMenu::OnEvent},
      {kMethodEventGroup, &DbusMenu::OnEventGroup},
      {kMethodGetGroupProperties, &DbusMenu::OnGetGroupProperties},
      {kMethodGetLayout, &DbusMenu::OnGetLayout},
      {kMethodGetProperty, &DbusMenu::OnGetProperty},
  };

  // std::size(methods) calls for method export, 1 call for properties
  // initialization.
  barrier_ =
      SuccessBarrierCallback(std::size(methods) + 1, std::move(callback));
  for (const auto& method : methods) {
    menu_->ExportMethod(
        kInterfaceDbusMenu, method.name,
        WrapMethodCallback(
            base::BindRepeating(method.callback, weak_factory_.GetWeakPtr())),
        base::BindRepeating(&DbusMenu::OnExported, weak_factory_.GetWeakPtr()));
  }

  properties_ = std::make_unique<DbusProperties>(menu_, barrier_);
  properties_->RegisterInterface(kInterfaceDbusMenu);
  auto set_property = [&](const std::string& property_name, auto&& value) {
    properties_->SetProperty(kInterfaceDbusMenu, property_name,
                             std::forward<decltype(value)>(value), false);
  };
  set_property(kPropertyIconThemePath, DbusArray<DbusString>());
  set_property(kPropertyMenuStatus, DbusString(kPropertyValueStatusNormal));
  set_property(kPropertyTextDirection, DbusTextDirection());
  set_property(kPropertyVersion, DbusUint32(kPropertyValueVersion));
}

DbusMenu::~DbusMenu() = default;

void DbusMenu::SetModel(ui::MenuModel* model, bool send_signal) {
  items_.clear();

  std::map<std::string, DbusVariant> properties;
  std::vector<int32_t> children;
  if (model) {
    properties["children-display"] = MakeDbusVariant(DbusString("submenu"));
    children = ConvertMenu(model);
  }
  items_[0] = std::make_unique<MenuItem>(
      0, std::move(properties), std::move(children), nullptr, nullptr, -1);

  if (send_signal)
    SendLayoutChangedSignal(0);
}

void DbusMenu::MenuLayoutUpdated(ui::MenuModel* model) {
  MenuItem* item = FindMenuItemForModel(model, items_[0].get());
  DCHECK(item);
  DeleteItemChildren(item);
  item->children = ConvertMenu(model);
  SendLayoutChangedSignal(item->id);
}

void DbusMenu::MenuItemsPropertiesUpdated(
    const std::vector<MenuItemReference>& menu_items) {
  if (menu_items.empty())
    return;

  MenuPropertyChanges updated_props;
  MenuPropertyChanges removed_props;
  for (const auto& menu_item : menu_items) {
    ui::MenuModel* menu = menu_item.first;
    size_t index = menu_item.second;
    MenuItem* parent = FindMenuItemForModel(menu, items_[0].get());
    MenuItem* item = nullptr;
    for (int32_t id : parent->children) {
      MenuItem* child = items_[id].get();
      DCHECK_EQ(child->containing_menu, menu);
      if (child->containing_menu_index == index) {
        item = child;
        break;
      }
    }
    DCHECK(item);

    auto old_properties = std::move(item->properties);
    item->properties = ComputeMenuPropertiesForMenuItem(menu, index);
    MenuPropertyList item_updated_props;
    MenuPropertyList item_removed_props;
    ComputeMenuPropertyChanges(old_properties, item->properties,
                               &item_updated_props, &item_removed_props);
    if (!item_updated_props.empty())
      updated_props[item->id] = std::move(item_updated_props);
    if (!item_removed_props.empty())
      removed_props[item->id] = std::move(item_removed_props);
  }

  dbus::Signal signal(kInterfaceDbusMenu, kSignalItemsPropertiesUpdated);
  dbus::MessageWriter writer(&signal);
  WriteUpdatedProperties(&writer, updated_props);
  WriteRemovedProperties(&writer, removed_props);
  menu_->SendSignal(&signal);
}

// static
dbus::ExportedObject::MethodCallCallback DbusMenu::WrapMethodCallback(
    base::RepeatingCallback<void(ScopedMethodResponse*)> callback) {
  return base::BindRepeating(
      [](base::RepeatingCallback<void(ScopedMethodResponse*)> bound_callback,
         dbus::MethodCall* method_call,
         dbus::ExportedObject::ResponseSender response_sender) {
        ScopedMethodResponse response(method_call, std::move(response_sender));
        bound_callback.Run(&response);
      },
      callback);
}

void DbusMenu::OnExported(const std::string& interface_name,
                          const std::string& method_name,
                          bool success) {
  barrier_.Run(success);
}

void DbusMenu::OnAboutToShow(ScopedMethodResponse* response) {
  int32_t id;
  if (!response->reader().PopInt32(&id))
    return;

  if (!AboutToShowImpl(id))
    return;

  response->Writer().AppendBool(false);
}

void DbusMenu::OnAboutToShowGroup(ScopedMethodResponse* response) {
  dbus::MessageReader array_reader(nullptr);
  if (!response->reader().PopArray(&array_reader))
    return;
  std::vector<int32_t> ids;
  while (array_reader.HasMoreData()) {
    int32_t id;
    if (!array_reader.PopInt32(&id))
      return;
    ids.push_back(id);
  }

  std::vector<int32_t> id_errors;
  for (int32_t id : ids) {
    if (!AboutToShowImpl(id))
      id_errors.push_back(id);
  }

  // IDs of updates needed (none).
  response->Writer().AppendArrayOfInt32s({});
  // Invalid IDs.
  response->Writer().AppendArrayOfInt32s(id_errors);
}

void DbusMenu::OnEvent(ScopedMethodResponse* response) {
  if (!EventImpl(&response->reader(), nullptr))
    return;

  response->EnsureResponse();
}

void DbusMenu::OnEventGroup(ScopedMethodResponse* response) {
  dbus::MessageReader array_reader(nullptr);
  if (!response->reader().PopArray(&array_reader))
    return;
  std::vector<int32_t> id_errors;
  while (array_reader.HasMoreData()) {
    dbus::MessageReader struct_reader(nullptr);
    array_reader.PopStruct(&struct_reader);

    int32_t id_error = -1;
    if (!EventImpl(&struct_reader, &id_error)) {
      if (id_error < 0)
        return;
      id_errors.push_back(id_error);
    }
  }

  response->Writer().AppendArrayOfInt32s(id_errors);
}

void DbusMenu::OnGetGroupProperties(ScopedMethodResponse* response) {
  dbus::MessageReader id_reader(nullptr);
  if (!response->reader().PopArray(&id_reader))
    return;
  std::vector<int32_t> ids;
  while (id_reader.HasMoreData()) {
    int32_t id;
    if (!id_reader.PopInt32(&id))
      return;
    ids.push_back(id);
  }

  std::set<std::string> property_filter;
  dbus::MessageReader property_reader(nullptr);
  if (!response->reader().PopArray(&property_reader))
    return;
  while (property_reader.HasMoreData()) {
    std::string property;
    if (!property_reader.PopString(&property))
      return;
    property_filter.insert(property);
  }

  dbus::MessageWriter& writer = response->Writer();
  dbus::MessageWriter item_writer(nullptr);
  writer.OpenArray("(ia{sv})", &item_writer);

  auto write_item = [&](int32_t id, const MenuItem& item) {
    dbus::MessageWriter struct_writer(nullptr);
    item_writer.OpenStruct(&struct_writer);
    struct_writer.AppendInt32(id);
    dbus::MessageWriter property_writer(nullptr);
    struct_writer.OpenArray("{sv}", &property_writer);
    for (const auto& property_pair : item.properties) {
      if (!property_filter.empty() &&
          !base::Contains(property_filter, property_pair.first)) {
        continue;
      }
      dbus::MessageWriter dict_entry_writer(nullptr);
      property_writer.OpenDictEntry(&dict_entry_writer);
      dict_entry_writer.AppendString(property_pair.first);
      property_pair.second.Write(&dict_entry_writer);
      property_writer.CloseContainer(&dict_entry_writer);
    }
    struct_writer.CloseContainer(&property_writer);
    item_writer.CloseContainer(&struct_writer);
  };

  if (ids.empty()) {
    for (const auto& item_pair : items_)
      write_item(item_pair.first, *item_pair.second);
  } else {
    for (int32_t id : ids) {
      auto it = items_.find(id);
      if (it != items_.end())
        write_item(id, *it->second);
    }
  }

  writer.CloseContainer(&item_writer);
}

void DbusMenu::OnGetLayout(ScopedMethodResponse* response) {
  dbus::MessageReader& reader = response->reader();
  int32_t id;
  int32_t depth;
  MenuPropertyList property_filter;
  if (!reader.PopInt32(&id) || !reader.PopInt32(&depth) || depth < -1 ||
      !reader.PopArrayOfStrings(&property_filter)) {
    return;
  }

  auto it = items_.find(id);
  if (it == items_.end())
    return;

  dbus::MessageWriter& writer = response->Writer();
  writer.AppendUint32(revision_);
  WriteMenuItem(it->second.get(), &writer, depth, property_filter);
}

void DbusMenu::OnGetProperty(ScopedMethodResponse* response) {
  dbus::MessageReader& reader = response->reader();
  int32_t id;
  std::string name;
  if (!reader.PopInt32(&id) || !reader.PopString(&name))
    return;

  auto item_it = items_.find(id);
  if (item_it == items_.end())
    return;

  MenuItem* item = item_it->second.get();
  auto property_it = item->properties.find(name);
  if (property_it == item->properties.end()) {
    DbusVariant default_value = CreateDefaultPropertyValue(name);
    if (default_value)
      default_value.Write(&response->Writer());
  } else {
    property_it->second.Write(&response->Writer());
  }
}

bool DbusMenu::AboutToShowImpl(int32_t id) {
  auto item = items_.find(id);
  if (item == items_.end())
    return false;

  ui::MenuModel* menu = item->second->menu;
  if (!menu)
    return false;

  menu->MenuWillShow();

  return true;
}

bool DbusMenu::EventImpl(dbus::MessageReader* reader, int32_t* id_error) {
  int32_t id;
  if (!reader->PopInt32(&id))
    return false;
  auto item_it = items_.find(id);
  if (item_it == items_.end()) {
    if (id_error)
      *id_error = id;
    return false;
  }

  std::string type;
  if (!reader->PopString(&type))
    return false;

  if (type == "clicked") {
    MenuItem* item = item_it->second.get();
    if (!item->containing_menu)
      return false;
    item->containing_menu->ActivatedAt(item->containing_menu_index);
  } else {
    DCHECK(type == "hovered" || type == "opened" || type == "closed")
        << "Unexpected type: " << type;
    // Nothing to do.
  }

  return true;
}

std::vector<int32_t> DbusMenu::ConvertMenu(ui::MenuModel* menu) {
  std::vector<int32_t> items;
  if (!menu)
    return items;
  items.reserve(menu->GetItemCount());

  for (size_t i = 0; i < menu->GetItemCount(); ++i) {
    ui::MenuModel* submenu = menu->GetSubmenuModelAt(i);
    std::vector<int32_t> children = ConvertMenu(submenu);

    int32_t id = NextItemId();
    items_[id] = std::make_unique<MenuItem>(
        id, ComputeMenuPropertiesForMenuItem(menu, i), std::move(children),
        submenu, menu, i);
    items.push_back(id);
  }

  return items;
}

int32_t DbusMenu::NextItemId() {
  return last_item_id_ = last_item_id_ == std::numeric_limits<int32_t>::max()
                             ? 1
                             : last_item_id_ + 1;
}

void DbusMenu::WriteMenuItem(const MenuItem* item,
                             dbus::MessageWriter* writer,
                             int32_t depth,
                             const MenuPropertyList& property_filter) const {
  dbus::MessageWriter struct_writer(nullptr);
  writer->OpenStruct(&struct_writer);
  struct_writer.AppendInt32(item->id);

  dbus::MessageWriter properties_writer(nullptr);
  struct_writer.OpenArray("{sv}", &properties_writer);
  for (const auto& property : item->properties) {
    if (property_filter.empty() ||
        base::Contains(property_filter, property.first)) {
      dbus::MessageWriter dict_entry_writer(nullptr);
      properties_writer.OpenDictEntry(&dict_entry_writer);
      dict_entry_writer.AppendString(property.first);
      property.second.Write(&dict_entry_writer);
      properties_writer.CloseContainer(&dict_entry_writer);
    }
  }
  struct_writer.CloseContainer(&properties_writer);

  dbus::MessageWriter children_writer(nullptr);
  struct_writer.OpenArray("v", &children_writer);
  if (depth != 0) {
    for (int32_t child : item->children) {
      dbus::MessageWriter variant_writer(nullptr);
      children_writer.OpenVariant("(ia{sv}av)", &variant_writer);
      WriteMenuItem(items_.find(child)->second.get(), &variant_writer,
                    depth == -1 ? -1 : depth - 1, property_filter);
      children_writer.CloseContainer(&variant_writer);
    }
  }
  struct_writer.CloseContainer(&children_writer);

  writer->CloseContainer(&struct_writer);
}

void DbusMenu::WriteUpdatedProperties(
    dbus::MessageWriter* writer,
    const MenuPropertyChanges& updated_props) const {
  dbus::MessageWriter updated_props_writer(nullptr);
  writer->OpenArray("(ia{sv})", &updated_props_writer);
  for (const auto& pair : updated_props) {
    int32_t id = pair.first;
    MenuItem* item = items_.find(id)->second.get();
    dbus::MessageWriter struct_writer(nullptr);
    updated_props_writer.OpenStruct(&struct_writer);
    struct_writer.AppendInt32(id);
    dbus::MessageWriter array_writer(nullptr);
    struct_writer.OpenArray("{sv}", &array_writer);
    for (const std::string& key : pair.second) {
      dbus::MessageWriter dict_entry_writer(nullptr);
      array_writer.OpenDictEntry(&dict_entry_writer);
      dict_entry_writer.AppendString(key);
      item->properties[key].Write(&dict_entry_writer);
      array_writer.CloseContainer(&dict_entry_writer);
    }
    struct_writer.CloseContainer(&array_writer);
    updated_props_writer.CloseContainer(&struct_writer);
  }
  writer->CloseContainer(&updated_props_writer);
}

DbusMenu::MenuItem* DbusMenu::FindMenuItemForModel(const ui::MenuModel* model,
                                                   MenuItem* item) const {
  if (item->menu == model)
    return item;
  for (int32_t id : item->children) {
    MenuItem* child = items_.find(id)->second.get();
    MenuItem* found = FindMenuItemForModel(model, child);
    if (found)
      return found;
  }
  return nullptr;
}

void DbusMenu::DeleteItem(MenuItem* item) {
  DeleteItemChildren(item);
  items_.erase(item->id);
}

void DbusMenu::DeleteItemChildren(MenuItem* item) {
  for (int32_t id : item->children)
    DeleteItem(items_.find(id)->second.get());
}

void DbusMenu::SendLayoutChangedSignal(int32_t id) {
  dbus::Signal signal(kInterfaceDbusMenu, kSignalLayoutUpdated);
  dbus::MessageWriter writer(&signal);
  writer.AppendUint32(++revision_);  // Revision of the new layout.
  writer.AppendInt32(id);            // Parent item whose layout changed.
  menu_->SendSignal(&signal);
}
