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
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/nix/xdg_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "components/dbus/menu/menu.h"
#include "components/dbus/properties/dbus_properties.h"
#include "components/dbus/properties/success_barrier_callback.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/bind_weak_ptr_for_export_method.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/export_method.h"
#include "components/dbus/utils/signature.h"
#include "components/dbus/utils/variant.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/svg/SkSVGCanvas.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_span_util.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/linux/status_icon_linux.h"
#include "ui/menus/simple_menu_model.h"

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
const char kPathDbusMenu[] = "/org/chromium/DbusMenu";

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

class RefCountedSkData final : public base::RefCountedMemory {
 public:
  explicit RefCountedSkData(const sk_sp<SkData>& data) : data_(data) {}

  RefCountedSkData(const RefCountedSkData&) = delete;
  RefCountedSkData& operator=(const RefCountedSkData&) = delete;

 private:
  friend class RefCountedThreadSafe<RefCountedSkData>;

  // RefCountedMemory:
  base::span<const uint8_t> AsSpan() const LIFETIME_BOUND override {
    return gfx::SkDataToSpan(data_);
  }

  ~RefCountedSkData() override = default;

  sk_sp<SkData> const data_;
};

int NextServiceId() {
  static int status_icon_count = 0;
  return ++status_icon_count;
}

std::string PropertyIdFromId(int service_id) {
  return "chrome_status_icon_" + base::NumberToString(service_id);
}

dbus::ObjectPath ObjectPathFromId(const std::string& path, int service_id) {
  return dbus::ObjectPath(
      base::StrCat({path, "/", base::NumberToString(service_id)}));
}

using DbusImage = std::tuple</*width=*/int32_t,
                             /*height=*/int32_t,
                             /*pixels=*/std::vector<uint8_t>>;

std::vector<DbusImage> MakeDbusImage(const gfx::ImageSkia& image) {
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
  std::vector<DbusImage> images;
  images.emplace_back(width, height, std::move(color_data));
  return images;
}

using DbusToolTip = std::tuple</*icon=*/std::string,
                               /*image=*/std::vector<DbusImage>,
                               /*title=*/std::string,
                               /*subtitle=*/std::string>;

DbusToolTip MakeDbusToolTip(const std::string& text) {
  return {"", {}, text, ""};
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
    case base::nix::DESKTOP_ENVIRONMENT_COSMIC:
      return false;
  }
  NOTREACHED();
}

base::FilePath WriteIconFile(size_t icon_file_id,
                             scoped_refptr<base::RefCountedMemory> data,
                             bool is_vector_icon) {
  // Some StatusNotifierHosts require both the theme directory and icon name to
  // change in order to update, so we need a new temporary directory and a
  // unique base name for the file.
  base::FilePath temp_dir;
  if (!base::CreateNewTempDirectory("", &temp_dir)) {
    return {};
  }

  // If this is a gfx::VectorIcon, the icon will be colored with the theme
  // foreground color. The icon must be named with the "-symbolic" suffix to
  // indicate this. See: https://wiki.gnome.org/Design/OS/SymbolicIcons
  const char* suffix = is_vector_icon ? "-symbolic.svg" : ".png";
  base::FilePath file_path = temp_dir.Append(
      "status_icon_" + base::NumberToString(icon_file_id) + suffix);
  if (!base::WriteFile(file_path, *data)) {
    base::DeletePathRecursively(temp_dir);
    return {};
  }

  return file_path;
}

}  // namespace

class StatusIconLinuxDbus::Multiplexer {
 public:
  static Multiplexer* Get() {
    static base::NoDestructor<Multiplexer> instance;
    return instance.get();
  }

  static dbus::ExportedObject* GetMultiplexerItem() {
    return Get()->item_.get();
  }

  void Register(const std::string& service_name,
                StatusIconLinuxDbus* icon,
                dbus::Bus* bus) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (icons_.empty()) {
      ExportMethods(bus);
    }
    icons_[service_name] = icon;
  }

  void Unregister(const std::string& service_name) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // Unregister may be called for a service name that was never registered
    // if initialization failed before registration.
    icons_.erase(service_name);
    if (icons_.empty()) {
      UnexportMethods();
    }
  }

  StatusIconLinuxDbus* GetIcon(const std::string& service_name) {
    auto it = icons_.find(service_name);
    return it != icons_.end() ? it->second : nullptr;
  }

 private:
  friend class base::NoDestructor<Multiplexer>;
  Multiplexer() = default;
  ~Multiplexer() = default;

  void ExportMethods(dbus::Bus* bus) {
    item_ = bus->GetExportedObject(dbus::ObjectPath(kPathStatusNotifierItem));

    item_->ExportMethod(kInterfaceStatusNotifierItem, kMethodActivate,
                        base::BindRepeating(&Multiplexer::OnActivate),
                        base::DoNothing());
    item_->ExportMethod(kInterfaceStatusNotifierItem, kMethodContextMenu,
                        base::BindRepeating(&Multiplexer::OnContextMenu),
                        base::DoNothing());
    item_->ExportMethod(kInterfaceStatusNotifierItem, kMethodScroll,
                        base::BindRepeating(&Multiplexer::OnScroll),
                        base::DoNothing());
    item_->ExportMethod(kInterfaceStatusNotifierItem, kMethodSecondaryActivate,
                        base::BindRepeating(&Multiplexer::OnSecondaryActivate),
                        base::DoNothing());
    item_->ExportMethod(DBUS_INTERFACE_PROPERTIES, "Get",
                        base::BindRepeating(&Multiplexer::OnGetProperty),
                        base::DoNothing());
    item_->ExportMethod(DBUS_INTERFACE_PROPERTIES, "GetAll",
                        base::BindRepeating(&Multiplexer::OnGetAllProperties),
                        base::DoNothing());
  }

  void UnexportMethods() {
    if (item_) {
      item_->Unregister();
      item_ = nullptr;
    }
  }

  static void OnActivate(dbus::MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender) {
    std::string destination = method_call->GetDestination();
    auto* icon = Get()->GetIcon(destination);
    if (icon) {
      dbus::MessageReader reader(method_call);
      int32_t x = 0;
      int32_t y = 0;
      if (reader.PopInt32(&x) && reader.PopInt32(&y)) {
        (void)icon->OnActivate(x, y);
      }
    }
    std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
  }

  static void OnContextMenu(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender) {
    std::string destination = method_call->GetDestination();
    auto* icon = Get()->GetIcon(destination);
    if (icon) {
      dbus::MessageReader reader(method_call);
      int32_t x = 0;
      int32_t y = 0;
      if (reader.PopInt32(&x) && reader.PopInt32(&y)) {
        (void)icon->OnContextMenu(x, y);
      }
    }
    std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
  }

  static void OnScroll(dbus::MethodCall* method_call,
                       dbus::ExportedObject::ResponseSender response_sender) {
    std::string destination = method_call->GetDestination();
    auto* icon = Get()->GetIcon(destination);
    if (icon) {
      dbus::MessageReader reader(method_call);
      int32_t delta = 0;
      std::string orientation;
      if (reader.PopInt32(&delta) && reader.PopString(&orientation)) {
        (void)icon->OnScroll(delta, orientation);
      }
    }
    std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
  }

  static void OnSecondaryActivate(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender) {
    std::string destination = method_call->GetDestination();
    auto* icon = Get()->GetIcon(destination);
    if (icon) {
      dbus::MessageReader reader(method_call);
      int32_t x = 0;
      int32_t y = 0;
      if (reader.PopInt32(&x) && reader.PopInt32(&y)) {
        (void)icon->OnSecondaryActivate(x, y);
      }
    }
    std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
  }

  static void OnGetProperty(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender) {
    std::string destination = method_call->GetDestination();
    auto* icon = Get()->GetIcon(destination);
    if (!icon) {
      std::move(response_sender).Run(nullptr);
      return;
    }

    dbus::MessageReader reader(method_call);
    std::string interface;
    std::string property_name;
    if (!reader.PopString(&interface) || !reader.PopString(&property_name)) {
      std::move(response_sender).Run(nullptr);
      return;
    }

    if (interface != kInterfaceStatusNotifierItem) {
      std::move(response_sender).Run(nullptr);
      return;
    }

    std::unique_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());

    auto prop_it = icon->properties_.find(property_name);
    if (prop_it != icon->properties_.end()) {
      prop_it->second.Write(writer);
      std::move(response_sender).Run(std::move(response));
    } else {
      std::move(response_sender).Run(nullptr);
    }
  }

  static void OnGetAllProperties(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender) {
    std::string destination = method_call->GetDestination();
    auto* icon = Get()->GetIcon(destination);
    if (!icon) {
      std::move(response_sender).Run(nullptr);
      return;
    }

    dbus::MessageReader reader(method_call);
    std::string interface;
    if (!reader.PopString(&interface)) {
      std::move(response_sender).Run(nullptr);
      return;
    }

    if (interface != kInterfaceStatusNotifierItem) {
      std::move(response_sender).Run(nullptr);
      return;
    }

    std::unique_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());

    dbus_utils::WriteValue(writer, icon->properties_);
    std::move(response_sender).Run(std::move(response));
  }

  // A map of registered status icons, keyed by their unique D-Bus service name.
  std::map<std::string, StatusIconLinuxDbus*> icons_;
  scoped_refptr<dbus::ExportedObject> item_;
};

StatusIconLinuxDbus::StatusIconLinuxDbus()
    : bus_(dbus_thread_linux::GetSharedSessionBus()),
      should_write_icon_to_file_(ShouldWriteIconToFile()),
      icon_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CheckStatusNotifierWatcherHasOwner();
}

void StatusIconLinuxDbus::SetImage(const gfx::ImageSkia& image) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetImageImpl(image, /*send_signals=*/true);
}

void StatusIconLinuxDbus::SetIcon(const gfx::VectorIcon& icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (properties_.empty()) {
    return;
  }

  const int size = gfx::GetDefaultSizeOfVectorIcon(icon);
  SkDynamicMemoryWStream svg_stream;
  // Scope these variables so the stream gets flushed before detaching the data.
  {
    SkRect bounds = SkRect::MakeIWH(size, size);
    std::unique_ptr<SkCanvas> svg_canvas =
        SkSVGCanvas::Make(bounds, &svg_stream);
    cc::SkiaPaintCanvas paint_canvas(svg_canvas.get());
    gfx::Canvas gfx_canvas(&paint_canvas, 1);
    gfx::PaintVectorIcon(&gfx_canvas, icon, SK_ColorBLACK);
  }
  auto data = base::MakeRefCounted<RefCountedSkData>(svg_stream.detachAsData());

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(WriteIconFile, icon_file_id_++, data,
                     /*is_vector_icon=*/true),
      base::BindOnce(&StatusIconLinuxDbus::OnIconFileWritten, this));
}

void StatusIconLinuxDbus::SetToolTip(const std::u16string& tool_tip) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (properties_.empty()) {
    return;
  }

  UpdateMenuImpl(delegate_->GetMenuModel(), true);

  SetProperty<"(sa(iiay)ss)">(
      kPropertyToolTip,
      MakeDbusToolTip(base::UTF16ToUTF8(delegate_->GetToolTip())));
  if (auto* multiplexer_item = Multiplexer::GetMultiplexerItem()) {
    dbus::Signal signal(kInterfaceStatusNotifierItem, kSignalNewToolTip);
    multiplexer_item->SendSignal(&signal);
  }
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
  CleanupIconFile();

  Multiplexer::Get()->Unregister(service_name_);
  if (!service_name_.empty()) {
    bus_->ReleaseOwnership(service_name_);
  }

  if (menu_) {
    menu_.reset();
    bus_->UnregisterExportedObject(
        ObjectPathFromId(kPathDbusMenu, service_id_));
  }
}

void StatusIconLinuxDbus::CheckStatusNotifierWatcherHasOwner() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dbus::ObjectProxy* bus_proxy =
      bus_->GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
  dbus_utils::CallMethod<"s", "b">(
      bus_proxy, DBUS_INTERFACE_DBUS, kMethodNameHasOwner,
      base::BindOnce(&StatusIconLinuxDbus::OnNameHasOwnerResponse,
                     weak_factory_.GetWeakPtr()),
      kServiceStatusNotifierWatcher);
}

void StatusIconLinuxDbus::OnNameHasOwnerResponse(
    dbus_utils::CallMethodResultSig<"b"> response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response.has_value() || !std::get<0>(*response)) {
    delegate_->OnImplInitializationFailed();
    return;
  }

  watcher_ = bus_->GetObjectProxy(kServiceStatusNotifierWatcher,
                                  dbus::ObjectPath(kPathStatusNotifierWatcher));

  dbus_utils::CallMethod<"ss", "v">(
      watcher_, DBUS_INTERFACE_PROPERTIES, kMethodGet,
      base::BindOnce(&StatusIconLinuxDbus::OnHostRegisteredResponse,
                     weak_factory_.GetWeakPtr()),
      kInterfaceStatusNotifierWatcher, kPropertyIsStatusNotifierHostRegistered);
}

void StatusIconLinuxDbus::OnHostRegisteredResponse(
    dbus_utils::CallMethodResultSig<"v"> response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response.has_value()) {
    delegate_->OnImplInitializationFailed();
    return;
  }

  auto registered = std::move(std::get<0>(*response)).Take<bool>();
  if (!registered.value_or(false)) {
    delegate_->OnImplInitializationFailed();
    return;
  }

  service_id_ = NextServiceId();

  service_name_ =
      base::StrCat({"org.freedesktop.StatusNotifierItem-",
                    base::NumberToString(base::Process::Current().Pid()), "-",
                    base::NumberToString(service_id_)});

  // The barrier only requires 1 call (for `menu_` initialization).
  barrier_ = SuccessBarrierCallback(
      1, base::BindOnce(&StatusIconLinuxDbus::OnInitialized,
                        weak_factory_.GetWeakPtr()));

  menu_ = std::make_unique<DbusMenu>(
      bus_->GetExportedObject(ObjectPathFromId(kPathDbusMenu, service_id_)),
      barrier_);
  UpdateMenuImpl(delegate_->GetMenuModel(), false);

  // Initialize properties map.
  SetProperty<"b">(kPropertyItemIsMenu, false, false);
  SetProperty<"i">(kPropertyWindowId, 0, false);
  SetProperty<"o">(kPropertyMenu, ObjectPathFromId(kPathDbusMenu, service_id_),
                   false);
  SetProperty<"s">(kPropertyAttentionIconName, "", false);
  SetProperty<"s">(kPropertyAttentionMovieName, "", false);
  SetProperty<"s">(kPropertyCategory, kPropertyValueCategory, false);
  SetProperty<"s">(kPropertyId, PropertyIdFromId(service_id_), false);
  SetProperty<"s">(kPropertyOverlayIconName, "", false);
  SetProperty<"s">(kPropertyStatus, kPropertyValueStatus, false);
  SetProperty<"s">(kPropertyTitle, "", false);
  SetProperty<"a(iiay)">(kPropertyAttentionIconPixmap, {}, false);
  SetProperty<"a(iiay)">(kPropertyOverlayIconPixmap, {}, false);
  SetProperty<"(sa(iiay)ss)">(
      kPropertyToolTip,
      MakeDbusToolTip(base::UTF16ToUTF8(delegate_->GetToolTip())), false);

  if (delegate_->GetIcon() && !delegate_->GetIcon()->is_empty()) {
    SetIcon(*delegate_->GetIcon());
  } else if (!delegate_->GetImage().isNull()) {
    SetImageImpl(delegate_->GetImage(), /*send_signals=*/false);
  }
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

  Multiplexer::Get()->Register(service_name_, this, bus_.get());

  bus_->RequestOwnership(
      service_name_, dbus::Bus::REQUIRE_PRIMARY,
      base::BindOnce(&StatusIconLinuxDbus::OnOwnershipAcquired,
                     weak_factory_.GetWeakPtr()));
}

void StatusIconLinuxDbus::OnOwnershipAcquired(const std::string& service_name,
                                              bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    delegate_->OnImplInitializationFailed();
    return;
  }

  RegisterStatusNotifierItem();
}

void StatusIconLinuxDbus::RegisterStatusNotifierItem() {
  dbus_utils::CallMethod<"s", "">(
      watcher_, kInterfaceStatusNotifierWatcher,
      kMethodRegisterStatusNotifierItem,
      base::BindOnce(&StatusIconLinuxDbus::OnRegistered,
                     weak_factory_.GetWeakPtr()),
      service_name_);
}

void StatusIconLinuxDbus::OnRegistered(
    dbus_utils::CallMethodResultSig<""> response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response.has_value()) {
    delegate_->OnImplInitializationFailed();
  }
}

void StatusIconLinuxDbus::OnNameOwnerChangedReceived(
    const std::string& old_owner,
    const std::string& new_owner) {
  // Re-register the item when the StatusNotifierWatcher has a new owner.
  if (!new_owner.empty()) {
    RegisterStatusNotifierItem();
  }
}

dbus_utils::ExportMethodResult<> StatusIconLinuxDbus::OnActivate(int32_t x,
                                                                 int32_t y) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delegate_->OnClick();
  return std::make_tuple();
}

dbus_utils::ExportMethodResult<> StatusIconLinuxDbus::OnContextMenu(int32_t x,
                                                                    int32_t y) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!menu_runner_) {
    menu_runner_ = std::make_unique<views::MenuRunner>(
        concat_menu_.get(), views::MenuRunner::HAS_MNEMONICS |
                                views::MenuRunner::CONTEXT_MENU |
                                views::MenuRunner::FIXED_ANCHOR);
  }
  menu_runner_->RunMenuAt(
      nullptr, nullptr, gfx::Rect(gfx::Point(x, y), gfx::Size()),
      views::MenuAnchorPosition::kTopRight, ui::mojom::MenuSourceType::kMouse);
  return std::make_tuple();
}

dbus_utils::ExportMethodResult<> StatusIconLinuxDbus::OnScroll(
    int32_t delta,
    std::string orientation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Ignore scroll events.
  return std::make_tuple();
}

dbus_utils::ExportMethodResult<> StatusIconLinuxDbus::OnSecondaryActivate(
    int32_t x,
    int32_t y) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // gnome-shell-extension-appindicator requires a double-click to activate
  // which is non-obvious, so allow middle-click to activate which is slightly
  // more obvious.
  delegate_->OnClick();
  return std::make_tuple();
}

void StatusIconLinuxDbus::UpdateMenuImpl(ui::MenuModel* model,
                                         bool send_signal) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!menu_) {
    return;
  }

  if (!model) {
    empty_menu_ = std::make_unique<ui::SimpleMenuModel>(nullptr);
    model = empty_menu_.get();
  }

  // `menu_` keeps a raw pointer to `click_action_menu_`. Clear that pointer.
  menu_->SetModel(nullptr, /*send_signal=*/false);
  click_action_menu_ = std::make_unique<ui::SimpleMenuModel>(this);
  if (delegate_->HasClickAction() && !delegate_->GetToolTip().empty()) {
    click_action_menu_->AddItem(0, delegate_->GetToolTip());
    if (model->GetItemCount()) {
      click_action_menu_->AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
    }
  }

  concat_menu_ =
      std::make_unique<ConcatMenuModel>(click_action_menu_.get(), model);
  menu_->SetModel(concat_menu_.get(), send_signal);
  menu_runner_.reset();
}

void StatusIconLinuxDbus::SetImageImpl(const gfx::ImageSkia& image,
                                       bool send_signals) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (properties_.empty()) {
    return;
  }

  if (should_write_icon_to_file_) {
    icon_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(WriteIconFile, icon_file_id_++,
                       gfx::Image(image).As1xPNGBytes(),
                       /*is_vector_icon=*/false),
        base::BindOnce(&StatusIconLinuxDbus::OnIconFileWritten, this));
  } else {
    SetProperty<"a(iiay)">(kPropertyIconPixmap, MakeDbusImage(image),
                           send_signals);
    if (send_signals) {
      if (auto* multiplexer_item = Multiplexer::GetMultiplexerItem()) {
        dbus::Signal signal(kInterfaceStatusNotifierItem, kSignalNewIcon);
        multiplexer_item->SendSignal(&signal);
      }
    }
  }
}

void StatusIconLinuxDbus::OnIconFileWritten(const base::FilePath& icon_file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CleanupIconFile();
  icon_file_ = icon_file;
  if (icon_file_.empty()) {
    return;
  }

  SetProperty<"s">(kPropertyIconThemePath, icon_file_.DirName().value(), false);
  SetProperty<"s">(kPropertyIconName,
                   icon_file_.BaseName().RemoveExtension().value(), false);

  if (auto* multiplexer_item = Multiplexer::GetMultiplexerItem()) {
    dbus::Signal new_icon_theme_path_signal(kInterfaceStatusNotifierItem,
                                            kSignalNewIconThemePath);
    dbus::MessageWriter writer(&new_icon_theme_path_signal);
    writer.AppendString(icon_file_.DirName().value());
    multiplexer_item->SendSignal(&new_icon_theme_path_signal);
    dbus::Signal new_icon_signal(kInterfaceStatusNotifierItem, kSignalNewIcon);
    multiplexer_item->SendSignal(&new_icon_signal);
  }
}

void StatusIconLinuxDbus::CleanupIconFile() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!icon_file_.empty()) {
    icon_task_runner_->PostTask(
        FROM_HERE,
        (base::GetDeletePathRecursivelyCallback(icon_file_.DirName())));
  }
}

void StatusIconLinuxDbus::PropertyUpdated(const std::string& property_name) {
  dbus::Signal signal(DBUS_INTERFACE_PROPERTIES, "PropertiesChanged");
  dbus::MessageWriter writer(&signal);
  writer.AppendString(kInterfaceStatusNotifierItem);

  // Changed properties.
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter dict_entry_writer(nullptr);
  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(property_name);
  properties_[property_name].Write(dict_entry_writer);
  array_writer.CloseContainer(&dict_entry_writer);
  writer.CloseContainer(&array_writer);

  // Invalidated properties.
  writer.AppendArrayOfStrings({});

  if (auto* multiplexer_item = Multiplexer::GetMultiplexerItem()) {
    multiplexer_item->SendSignal(&signal);
  }
}
