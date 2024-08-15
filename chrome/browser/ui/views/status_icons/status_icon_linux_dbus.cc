// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_linux_dbus.h"

#include <dbus/dbus-shared.h>

#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/nix/xdg_util.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "components/dbus/menu/menu.h"
#include "components/dbus/properties/dbus_properties.h"
#include "components/dbus/properties/success_barrier_callback.h"
#include "components/dbus/properties/types.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/linux/status_icon_linux.h"

namespace {

// Service names.
const char kServiceStatusNotifierWatcher[] = "org.kde.StatusNotifierWatcher";

// Interfaces.
// If/when the StatusNotifierItem spec gets accepted AND widely used, replace
// "kde" with "freedesktop".
const char kInterfaceStatusNotifierItem[] = "org.kde.StatusNotifierItem";
const char kInterfaceStatusNotifierWatcher[] = "org.kde.StatusNotifierWatcher";

// Object paths.
const char kPathStatusNotifierItem[] = "/StatusNotifierItem";
const char kPathStatusNotifierWatcher[] = "/StatusNotifierWatcher";
const char kPathDbusMenu[] = "/com/canonical/dbusmenu";

// Methods.
const char kMethodNameHasOwner[] = "NameHasOwner";
const char kMethodRegisterStatusNotifierItem[] = "RegisterStatusNotifierItem";
const char kMethodActivate[] = "Activate";
const char kMethodContextMenu[] = "ContextMenu";
const char kMethodScroll[] = "Scroll";
const char kMethodSecondaryActivate[] = "SecondaryActivate";
const char kMethodGet[] = "Get";

// Properties.
const char kPropertyIsStatusNotifierHostRegistered[] =
    "IsStatusNotifierHostRegistered";
const char kPropertyItemIsMenu[] = "ItemIsMenu";
const char kPropertyWindowId[] = "WindowId";
const char kPropertyMenu[] = "Menu";
const char kPropertyAttentionIconName[] = "AttentionIconName";
const char kPropertyAttentionMovieName[] = "AttentionMovieName";
const char kPropertyCategory[] = "Category";
const char kPropertyIconName[] = "IconName";
const char kPropertyIconThemePath[] = "IconThemePath";
const char kPropertyId[] = "Id";
const char kPropertyOverlayIconName[] = "OverlayIconName";
const char kPropertyStatus[] = "Status";
const char kPropertyTitle[] = "Title";
const char kPropertyAttentionIconPixmap[] = "AttentionIconPixmap";
const char kPropertyIconPixmap[] = "IconPixmap";
const char kPropertyOverlayIconPixmap[] = "OverlayIconPixmap";
const char kPropertyToolTip[] = "ToolTip";

// Signals.
const char kSignalNewIcon[] = "NewIcon";
const char kSignalNewIconThemePath[] = "NewIconThemePath";
const char kSignalNewToolTip[] = "NewToolTip";

// Property values.
const char kPropertyValueCategory[] = "ApplicationStatus";
const char kPropertyValueStatus[] = "Active";

int NextServiceId() {
  static int status_icon_count = 0;
  return ++status_icon_count;
}

std::string PropertyIdFromId(int service_id) {
  return "chrome_status_icon_" + base::NumberToString(service_id);
}

auto MakeDbusImage(const gfx::ImageSkia& image) {
  const SkBitmap* bitmap = image.bitmap();
  int width = bitmap->width();
  int height = bitmap->height();
  std::vector<uint8_t> color_data;
  auto size = base::CheckedNumeric<size_t>(4) * width * height;
  color_data.reserve(size.ValueOrDie());
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      SkColor color = bitmap->getColor(x, y);
      color_data.push_back(SkColorGetA(color));
      color_data.push_back(SkColorGetR(color));
      color_data.push_back(SkColorGetG(color));
      color_data.push_back(SkColorGetB(color));
    }
  }
  return MakeDbusArray(MakeDbusStruct(
      DbusInt32(width), DbusInt32(height),
      DbusByteArray(base::RefCountedBytes::TakeVector(&color_data))));
}

auto MakeDbusToolTip(const std::string& text) {
  return MakeDbusStruct(
      DbusString(""),
      DbusArray<DbusStruct<DbusInt32, DbusInt32, DbusByteArray>>(),
      DbusString(text), DbusString(""));
}

bool ShouldWriteIconToFile() {
  auto env = base::Environment::Create();
  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
      // gnome-shell-extension-appindicator doesn't downsize icons when they're
      // given as DBus pixmaps.  But it does when icons are given as files.
    case base::nix::DESKTOP_ENVIRONMENT_PANTHEON:
      // wingpanel-indicator-ayatana only supports file icons.
      return true;
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
    case base::nix::DESKTOP_ENVIRONMENT_CINNAMON:
    case base::nix::DESKTOP_ENVIRONMENT_DEEPIN:
    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
    case base::nix::DESKTOP_ENVIRONMENT_KDE5:
    case base::nix::DESKTOP_ENVIRONMENT_KDE6:
    case base::nix::DESKTOP_ENVIRONMENT_UKUI:
    case base::nix::DESKTOP_ENVIRONMENT_UNITY:
    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
    case base::nix::DESKTOP_ENVIRONMENT_LXQT:
      return false;
  }
  NOTREACHED();
}

base::FilePath WriteIconFile(size_t icon_file_id,
                             scoped_refptr<base::RefCountedMemory> data) {
  // Some StatusNotifierHosts require both the theme directory and icon name to
  // change in order to update, so we need a new temporary directory and a
  // unique base name for the file.
  base::FilePath temp_dir;
  if (!base::CreateNewTempDirectory("", &temp_dir))
    return {};

  base::FilePath file_path = temp_dir.Append(
      "status_icon_" + base::NumberToString(icon_file_id) + ".png");
  if (!base::WriteFile(file_path, *data)) {
    base::DeletePathRecursively(temp_dir);
    return {};
  }

  return file_path;
}

}  // namespace

StatusIconLinuxDbus::StatusIconLinuxDbus()
    : should_write_icon_to_file_(ShouldWriteIconToFile()),
      icon_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dbus::Bus::Options bus_options;
  bus_options.bus_type = dbus::Bus::SESSION;
  bus_options.connection_type = dbus::Bus::PRIVATE;
  bus_options.dbus_task_runner = dbus_thread_linux::GetTaskRunner();
  bus_ = base::MakeRefCounted<dbus::Bus>(bus_options);
  CheckStatusNotifierWatcherHasOwner();
}

void StatusIconLinuxDbus::SetIcon(const gfx::ImageSkia& image) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetIconImpl(image, true);
}

void StatusIconLinuxDbus::SetToolTip(const std::u16string& tool_tip) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!properties_)
    return;

  UpdateMenuImpl(delegate_->GetMenuModel(), true);

  properties_->SetProperty(
      kInterfaceStatusNotifierItem, kPropertyToolTip,
      MakeDbusToolTip(base::UTF16ToUTF8(delegate_->GetToolTip())));
  dbus::Signal signal(kInterfaceStatusNotifierItem, kSignalNewToolTip);
  item_->SendSignal(&signal);
}

void StatusIconLinuxDbus::UpdatePlatformContextMenu(ui::MenuModel* model) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  UpdateMenuImpl(model, true);
}

void StatusIconLinuxDbus::RefreshPlatformContextMenu() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // This codepath gets called for property changes like changed labels or
  // icons, but also for layout changes like deleted items.
  // TODO(thomasanderson): Split this into two methods so we can avoid
  // rebuilding the menu for simple property changes.
  UpdateMenuImpl(delegate_->GetMenuModel(), true);
}

void StatusIconLinuxDbus::ExecuteCommand(int command_id, int event_flags) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(command_id, 0);
  delegate_->OnClick();
}

StatusIconLinuxDbus::~StatusIconLinuxDbus() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bus_->GetDBusTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&dbus::Bus::ShutdownAndBlock, bus_));
  CleanupIconFile();
}

void StatusIconLinuxDbus::CheckStatusNotifierWatcherHasOwner() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dbus::ObjectProxy* bus_proxy =
      bus_->GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
  dbus::MethodCall method_call(DBUS_INTERFACE_DBUS, kMethodNameHasOwner);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kServiceStatusNotifierWatcher);
  bus_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&StatusIconLinuxDbus::OnNameHasOwnerResponse,
                     weak_factory_.GetWeakPtr()));
}

void StatusIconLinuxDbus::OnNameHasOwnerResponse(dbus::Response* response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dbus::MessageReader reader(response);
  bool owned = false;
  if (!response || !reader.PopBool(&owned) || !owned) {
    delegate_->OnImplInitializationFailed();
    return;
  }

  watcher_ = bus_->GetObjectProxy(kServiceStatusNotifierWatcher,
                                  dbus::ObjectPath(kPathStatusNotifierWatcher));

  dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES, kMethodGet);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kInterfaceStatusNotifierWatcher);
  writer.AppendString(kPropertyIsStatusNotifierHostRegistered);
  watcher_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&StatusIconLinuxDbus::OnHostRegisteredResponse,
                     weak_factory_.GetWeakPtr()));
}

void StatusIconLinuxDbus::OnHostRegisteredResponse(dbus::Response* response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response) {
    delegate_->OnImplInitializationFailed();
    return;
  }

  dbus::MessageReader reader(response);
  bool registered = false;
  if (!reader.PopVariantOfBool(&registered) || !registered) {
    delegate_->OnImplInitializationFailed();
    return;
  }

  service_id_ = NextServiceId();

  static constexpr struct {
    const char* name;
    void (StatusIconLinuxDbus::*callback)(dbus::MethodCall*,
                                          dbus::ExportedObject::ResponseSender);
  } methods[4] = {
      {kMethodActivate, &StatusIconLinuxDbus::OnActivate},
      {kMethodContextMenu, &StatusIconLinuxDbus::OnContextMenu},
      {kMethodScroll, &StatusIconLinuxDbus::OnScroll},
      {kMethodSecondaryActivate, &StatusIconLinuxDbus::OnSecondaryActivate},
  };

  // The barrier requires std::size(methods) + 2 calls.  std::size(methods)
  // for each method exported, 1 for |properties_| initialization, and 1 for
  // |menu_| initialization.
  barrier_ =
      SuccessBarrierCallback(std::size(methods) + 2,
                             base::BindOnce(&StatusIconLinuxDbus::OnInitialized,
                                            weak_factory_.GetWeakPtr()));

  item_ = bus_->GetExportedObject(dbus::ObjectPath(kPathStatusNotifierItem));
  for (const auto& method : methods) {
    item_->ExportMethod(
        kInterfaceStatusNotifierItem, method.name,
        base::BindRepeating(method.callback, weak_factory_.GetWeakPtr()),
        base::BindOnce(&StatusIconLinuxDbus::OnExported,
                       weak_factory_.GetWeakPtr()));
  }

  menu_ = std::make_unique<DbusMenu>(
      bus_->GetExportedObject(dbus::ObjectPath(kPathDbusMenu)), barrier_);
  UpdateMenuImpl(delegate_->GetMenuModel(), false);

  properties_ = std::make_unique<DbusProperties>(item_, barrier_);
  properties_->RegisterInterface(kInterfaceStatusNotifierItem);
  auto set_property = [&](const std::string& property_name, auto&& value) {
    properties_->SetProperty(kInterfaceStatusNotifierItem, property_name,
                             std::forward<decltype(value)>(value), false);
  };
  set_property(kPropertyItemIsMenu, DbusBoolean(false));
  set_property(kPropertyWindowId, DbusInt32(0));
  set_property(kPropertyMenu, DbusObjectPath(dbus::ObjectPath(kPathDbusMenu)));
  set_property(kPropertyAttentionIconName, DbusString(""));
  set_property(kPropertyAttentionMovieName, DbusString(""));
  set_property(kPropertyCategory, DbusString(kPropertyValueCategory));
  set_property(kPropertyId, DbusString(PropertyIdFromId(service_id_)));
  set_property(kPropertyOverlayIconName, DbusString(""));
  set_property(kPropertyStatus, DbusString(kPropertyValueStatus));
  set_property(kPropertyTitle, DbusString(""));
  set_property(kPropertyAttentionIconPixmap,
               DbusArray<DbusStruct<DbusInt32, DbusInt32, DbusByteArray>>());
  set_property(kPropertyOverlayIconPixmap,
               DbusArray<DbusStruct<DbusInt32, DbusInt32, DbusByteArray>>());
  set_property(kPropertyToolTip,
               MakeDbusToolTip(base::UTF16ToUTF8(delegate_->GetToolTip())));
  SetIconImpl(delegate_->GetImage(), false);
}

void StatusIconLinuxDbus::OnExported(const std::string& interface_name,
                                     const std::string& method_name,
                                     bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  barrier_.Run(success);
}

void StatusIconLinuxDbus::OnInitialized(bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    delegate_->OnImplInitializationFailed();
    return;
  }

  watcher_->SetNameOwnerChangedCallback(
      base::BindRepeating(&StatusIconLinuxDbus::OnNameOwnerChangedReceived,
                          weak_factory_.GetWeakPtr()));
  RegisterStatusNotifierItem();
}

void StatusIconLinuxDbus::RegisterStatusNotifierItem() {
  dbus::MethodCall method_call(kInterfaceStatusNotifierWatcher,
                               kMethodRegisterStatusNotifierItem);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(bus_->GetConnectionName());
  watcher_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&StatusIconLinuxDbus::OnRegistered,
                                      weak_factory_.GetWeakPtr()));
}

void StatusIconLinuxDbus::OnRegistered(dbus::Response* response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response)
    delegate_->OnImplInitializationFailed();
}

void StatusIconLinuxDbus::OnNameOwnerChangedReceived(
    const std::string& old_owner,
    const std::string& new_owner) {
  // Re-register the item when the StatusNotifierWatcher has a new owner.
  if (!new_owner.empty()) {
    RegisterStatusNotifierItem();
  }
}

void StatusIconLinuxDbus::OnActivate(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delegate_->OnClick();
  std::move(sender).Run(dbus::Response::FromMethodCall(method_call));
}

void StatusIconLinuxDbus::OnContextMenu(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dbus::MessageReader reader(method_call);
  int32_t x;
  int32_t y;
  if (!reader.PopInt32(&x) || !reader.PopInt32(&y)) {
    std::move(sender).Run(nullptr);
    return;
  }

  if (!menu_runner_) {
    menu_runner_ = std::make_unique<views::MenuRunner>(
        concat_menu_.get(), views::MenuRunner::HAS_MNEMONICS |
                                views::MenuRunner::CONTEXT_MENU |
                                views::MenuRunner::FIXED_ANCHOR);
  }
  menu_runner_->RunMenuAt(
      nullptr, nullptr, gfx::Rect(gfx::Point(x, y), gfx::Size()),
      views::MenuAnchorPosition::kTopRight, ui::MENU_SOURCE_MOUSE);
  std::move(sender).Run(dbus::Response::FromMethodCall(method_call));
}

void StatusIconLinuxDbus::OnScroll(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Ignore scroll events.
  std::move(sender).Run(dbus::Response::FromMethodCall(method_call));
}

void StatusIconLinuxDbus::OnSecondaryActivate(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Intentionally ignore secondary activations.  In the future, we may decide
  // to run the same handler as regular activations.
  std::move(sender).Run(dbus::Response::FromMethodCall(method_call));
}

void StatusIconLinuxDbus::UpdateMenuImpl(ui::MenuModel* model,
                                         bool send_signal) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!menu_)
    return;

  if (!model) {
    empty_menu_ = std::make_unique<ui::SimpleMenuModel>(nullptr);
    model = empty_menu_.get();
  }

  click_action_menu_ = std::make_unique<ui::SimpleMenuModel>(this);
  if (delegate_->HasClickAction() && !delegate_->GetToolTip().empty()) {
    click_action_menu_->AddItem(0, delegate_->GetToolTip());
    if (model->GetItemCount())
      click_action_menu_->AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
  }

  concat_menu_ =
      std::make_unique<ConcatMenuModel>(click_action_menu_.get(), model);
  menu_->SetModel(concat_menu_.get(), send_signal);
  menu_runner_.reset();
}

void StatusIconLinuxDbus::SetIconImpl(const gfx::ImageSkia& image,
                                      bool send_signals) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!properties_)
    return;

  if (should_write_icon_to_file_) {
    icon_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(WriteIconFile, icon_file_id_++,
                       gfx::Image(image).As1xPNGBytes()),
        base::BindOnce(&StatusIconLinuxDbus::OnIconFileWritten, this));
  } else {
    properties_->SetProperty(kInterfaceStatusNotifierItem, kPropertyIconPixmap,
                             MakeDbusImage(image), send_signals, false);
    if (send_signals) {
      dbus::Signal signal(kInterfaceStatusNotifierItem, kSignalNewIcon);
      item_->SendSignal(&signal);
    }
  }
}

void StatusIconLinuxDbus::OnIconFileWritten(const base::FilePath& icon_file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CleanupIconFile();
  icon_file_ = icon_file;
  if (icon_file_.empty())
    return;

  properties_->SetProperty(kInterfaceStatusNotifierItem, kPropertyIconThemePath,
                           DbusString(icon_file_.DirName().value()), false);
  properties_->SetProperty(
      kInterfaceStatusNotifierItem, kPropertyIconName,
      DbusString(icon_file_.BaseName().RemoveExtension().value()), false);

  dbus::Signal new_icon_theme_path_signal(kInterfaceStatusNotifierItem,
                                          kSignalNewIconThemePath);
  dbus::MessageWriter writer(&new_icon_theme_path_signal);
  writer.AppendString(icon_file_.DirName().value());
  item_->SendSignal(&new_icon_theme_path_signal);
  dbus::Signal new_icon_signal(kInterfaceStatusNotifierItem, kSignalNewIcon);
  item_->SendSignal(&new_icon_signal);
}

void StatusIconLinuxDbus::CleanupIconFile() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!icon_file_.empty()) {
    icon_task_runner_->PostTask(
        FROM_HERE,
        (base::GetDeletePathRecursivelyCallback(icon_file_.DirName())));
  }
}
