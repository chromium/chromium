// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/systemd.h"

#include <unistd.h>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/containers/circular_deque.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/nix/xdg_util.h"
#include "base/process/process_iterator.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/version_info/nix/version_extra_utils.h"
#include "build/branding_buildflags.h"
#include "components/dbus/utils/name_has_owner.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/utils/write_value.h"
#include "components/dbus/xdg/systemd_constants.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"

namespace dbus_xdg {

// Systemd dictionaries use structs instead of dict entries for some reason.
template <typename T>
using Dict = std::vector<std::tuple<std::string, T>>;
using VarDict = Dict<dbus_utils::Variant>;

using internal::SystemdUnitCallback;
using internal::SystemdUnitStatus;
using SystemdUnitCallbacks = std::vector<SystemdUnitCallback>;

namespace {

constexpr char kUnitNameFormat[] = "app-$1-$2.scope";

constexpr char kModeReplace[] = "replace";

// The number of times to scan for and migrate descendant processes that are
// still outside of the new scope.
constexpr int kMaxMigrationPasses = 3;

// Watches the object to become active and fires callbacks.
// The callbacks are fired whenever a response with the
// state being "active" or "failed" (or similar) comes.
//
// PS firing callbacks results in destroying this object. So any references
// to this become invalid.
class SystemdUnitActiveStateWatcher : public dbus::PropertySet {
 public:
  SystemdUnitActiveStateWatcher(scoped_refptr<dbus::Bus> bus,
                                dbus::ObjectProxy* object_proxy,
                                SystemdUnitCallback callback)
      : dbus::PropertySet(object_proxy,
                          kInterfaceSystemdUnit,
                          base::BindRepeating(
                              &SystemdUnitActiveStateWatcher::OnPropertyChanged,
                              base::Unretained(this))),
        bus_(bus),
        callback_(std::move(callback)) {
    RegisterProperty(kSystemdActiveStateProp, &active_state_);
    ConnectSignals();
    GetAll();
  }

  ~SystemdUnitActiveStateWatcher() override {
    bus_->RemoveObjectProxy(kServiceNameSystemd, object_proxy()->object_path(),
                            base::DoNothing());
  }

 private:
  void OnPropertyChanged(const std::string& property_name) {
    DCHECK(active_state_.is_valid());
    const std::string state_value = active_state_.value();
    if (!callback_ || state_value == "activating" ||
        state_value == "reloading") {
      // Ignore if callbacks have already been fired or continue waiting until
      // the state changes to something else.
      return;
    }

    // There are other states as failed, inactive, and deactivating. Treat all
    // of them as failed.
    std::move(callback_).Run(state_value == kSystemdStateActive
                                 ? SystemdUnitStatus::kUnitStarted
                                 : SystemdUnitStatus::kFailedToStart);
    MaybeDeleteSelf();
  }

  void OnGetAll(dbus::Response* response) override {
    dbus::PropertySet::OnGetAll(response);
    keep_alive_ = false;
    MaybeDeleteSelf();
  }

  void MaybeDeleteSelf() {
    if (!keep_alive_ && !callback_) {
      delete this;
    }
  }

  // Registered property that this listens updates to.
  dbus::Property<std::string> active_state_;

  // Control variable that helps to defer the destruction of |this| as deleting
  // self when the state changes to active during |OnGetAll| will result in a
  // segfault.
  bool keep_alive_ = true;

  scoped_refptr<dbus::Bus> bus_;
  SystemdUnitCallback callback_;
};

void OnGetPathResponse(scoped_refptr<dbus::Bus> bus,
                       SystemdUnitCallback callback,
                       dbus::Response* response) {
  dbus::MessageReader reader(response);
  dbus::ObjectPath obj_path;
  if (!response || !reader.PopObjectPath(&obj_path) || !obj_path.IsValid()) {
    // We didn't get a valid response. Treat this as failed service.
    std::move(callback).Run(SystemdUnitStatus::kFailedToStart);
    return;
  }

  dbus::ObjectProxy* unit_proxy =
      bus->GetObjectProxy(kServiceNameSystemd, obj_path);
  // Create the active state property watcher. It will destroy itself once
  // it gets notified about the state change.
  std::unique_ptr<SystemdUnitActiveStateWatcher> active_state_watcher =
      std::make_unique<SystemdUnitActiveStateWatcher>(bus, unit_proxy,
                                                      std::move(callback));
  active_state_watcher.release();
}

void WaitUnitActivateAndRunCallbacks(scoped_refptr<dbus::Bus> bus,
                                     std::string unit_name,
                                     SystemdUnitCallback callback) {
  // Get the path of the unit, which looks similar to
  // /org/freedesktop/systemd1/unit/app_2dorg_2echromium_2eChromium_2d3182191_2escope
  // and then wait for it activation.
  dbus::ObjectProxy* systemd = bus->GetObjectProxy(
      kServiceNameSystemd, dbus::ObjectPath(kObjectPathSystemd));

  dbus::MethodCall method_call(kInterfaceSystemdManager, kMethodGetUnit);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(unit_name);

  systemd->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OnGetPathResponse, std::move(bus), std::move(callback)));
}

void OnStartTransientUnitResponse(scoped_refptr<dbus::Bus> bus,
                                  std::string unit_name,
                                  SystemdUnitCallback callback,
                                  dbus::Response* response) {
  SystemdUnitStatus result = response ? SystemdUnitStatus::kUnitStarted
                                      : SystemdUnitStatus::kFailedToStart;
  // If the start of the unit failed, immediately notify the client. Otherwise,
  // wait for its activation.
  if (result == SystemdUnitStatus::kFailedToStart) {
    std::move(callback).Run(result);
  } else {
    WaitUnitActivateAndRunCallbacks(std::move(bus), unit_name,
                                    std::move(callback));
  }
}

void MigrateDescendantsToUnit(scoped_refptr<dbus::Bus> bus,
                              base::FilePath proc_dir,
                              std::string unit_name,
                              int passes_left);

void OnAttachProcessesResponses(scoped_refptr<dbus::Bus> bus,
                                base::FilePath proc_dir,
                                std::string unit_name,
                                int passes_left,
                                std::vector<bool> results) {
  // Children may have forked while their parents were being migrated, so scan
  // again, unless AttachProcessesToUnit() is not working at all (e.g. systemd
  // older than v238).
  if (std::ranges::contains(results, true)) {
    MigrateDescendantsToUnit(std::move(bus), std::move(proc_dir),
                             std::move(unit_name), passes_left - 1);
  }
}

void OnDescendantsFound(scoped_refptr<dbus::Bus> bus,
                        base::FilePath proc_dir,
                        std::string unit_name,
                        int passes_left,
                        std::vector<pid_t> pids) {
  if (pids.empty()) {
    return;
  }

  auto barrier = base::BarrierCallback<bool>(
      pids.size(), base::BindOnce(&OnAttachProcessesResponses, bus,
                                  std::move(proc_dir), unit_name, passes_left));
  auto* systemd = bus->GetObjectProxy(kServiceNameSystemd,
                                      dbus::ObjectPath(kObjectPathSystemd));
  // Attach the processes one at a time, since the whole call fails with ESRCH
  // if any of the processes has exited in the meantime. `pids` lists parents
  // before children, so long-lived parents like the zygotes are moved first
  // and their new children inherit the new scope.
  for (pid_t pid : pids) {
    dbus::MethodCall method_call(kInterfaceSystemdManager,
                                 kMethodAttachProcessesToUnit);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(unit_name);
    // Attach to the root cgroup of the unit.
    writer.AppendString("");
    const uint32_t pids_to_attach[] = {static_cast<uint32_t>(pid)};
    writer.AppendArrayOfUint32s(pids_to_attach);
    systemd->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                        base::BindOnce([](dbus::Response* response) {
                          return !!response;
                        }).Then(barrier));
  }
}

// Moves the descendants of this process that are still in the original cgroup
// (e.g. the zygotes, the GPU process and renderers that were created before
// the scope) into `unit_name`. Otherwise, stopping the original scope (e.g.
// when the terminal that launched the browser closes, or when the session
// ends) kills them while the browser process keeps running, which leads to a
// browser crash when the GPU process can't be relaunched (crbug.com/528177806).
void MigrateDescendantsToUnit(scoped_refptr<dbus::Bus> bus,
                              base::FilePath proc_dir,
                              std::string unit_name,
                              int passes_left) {
  if (passes_left <= 0) {
    return;
  }
  auto scan_task = base::BindOnce(&internal::GetDescendantsOutsideCgroup,
                                  proc_dir, getpid());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      std::move(scan_task),
      base::BindOnce(&OnDescendantsFound, std::move(bus), std::move(proc_dir),
                     std::move(unit_name), passes_left));
}

void OnUnitActive(scoped_refptr<dbus::Bus> bus,
                  base::FilePath proc_dir,
                  std::string unit_name,
                  SystemdUnitCallback callback,
                  SystemdUnitStatus status) {
  if (status == SystemdUnitStatus::kUnitStarted) {
    MigrateDescendantsToUnit(std::move(bus), std::move(proc_dir),
                             std::move(unit_name), kMaxMigrationPasses);
  }
  std::move(callback).Run(status);
}

void OnNameHasOwnerResponse(scoped_refptr<dbus::Bus> bus,
                            base::FilePath proc_dir,
                            SystemdUnitCallback callback,
                            std::optional<bool> name_has_owner) {
  if (!name_has_owner.value_or(false)) {
    std::move(callback).Run(SystemdUnitStatus::kNoSystemdService);
    return;
  }

  pid_t pid = getpid();
  if (pid <= 1) {
    std::move(callback).Run(SystemdUnitStatus::kInvalidPid);
    return;
  }

  std::string app_name =
      version_info::nix::GetAppName(*base::Environment::Create());

  // The unit naming format is specified in
  // https://systemd.io/DESKTOP_ENVIRONMENTS/
  auto unit_name = base::ReplaceStringPlaceholders(
      kUnitNameFormat,
      {internal::EscapeSystemdUnitName(app_name), base::NumberToString(pid)},
      nullptr);

  auto* systemd = bus->GetObjectProxy(kServiceNameSystemd,
                                      dbus::ObjectPath(kObjectPathSystemd));
  dbus::MethodCall method_call(kInterfaceSystemdManager,
                               kMethodStartTransientUnit);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(unit_name);
  writer.AppendString(kModeReplace);
  // Only this process is added when creating the scope. Its existing
  // descendants are attached afterwards, one at a time, since passing all of
  // them here fails the whole call if any of them exits in the meantime.
  std::vector<uint32_t> pids = {static_cast<uint32_t>(pid)};
  VarDict properties;
  properties.emplace_back("PIDs",
                          dbus_utils::Variant::Wrap<"au">(std::move(pids)));
  // AttachProcessesToUnit() requires a delegated unit.
  properties.emplace_back("Delegate", dbus_utils::Variant::Wrap<"b">(true));
  dbus_utils::WriteValue(writer, properties);
  // No auxiliary units.
  dbus_utils::WriteValue(writer, Dict<VarDict>());
  systemd->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OnStartTransientUnitResponse, bus, unit_name,
                     base::BindOnce(&OnUnitActive, bus, std::move(proc_dir),
                                    unit_name, std::move(callback))));
}

}  // namespace

namespace internal {

std::string EscapeSystemdUnitName(std::string_view name) {
  std::string escaped;
  for (char c : name) {
    if (c == '/') {
      escaped.push_back('-');
    } else if (base::IsAsciiAlphaNumeric(c) || c == ':' || c == '_') {
      escaped.push_back(c);
    } else if (c == '.') {
      if (escaped.empty()) {
        escaped.append("\\x2e");
      } else {
        escaped.push_back(c);
      }
    } else {
      escaped.append(
          base::StringPrintf("\\x%02x", static_cast<unsigned char>(c)));
    }
  }
  return escaped;
}

std::vector<pid_t> GetDescendantsOutsideCgroup(const base::FilePath& proc_dir,
                                               pid_t root) {
  std::string root_cgroup;
  if (!base::ReadFileToString(
          proc_dir.Append(base::NumberToString(root)).Append("cgroup"),
          &root_cgroup)) {
    return {};
  }

  std::map<pid_t, std::vector<pid_t>> children;
  base::ProcessIterator iter(/*filter=*/nullptr, proc_dir);
  while (const base::ProcessEntry* entry = iter.NextProcessEntry()) {
    children[entry->parent_pid()].push_back(entry->pid());
  }

  // Breadth-first, so that parents come before their children.
  std::vector<pid_t> result;
  std::set<pid_t> visited = {root};
  base::circular_deque<pid_t> queue = {root};
  while (!queue.empty()) {
    pid_t pid = queue.front();
    queue.pop_front();
    auto it = children.find(pid);
    if (it == children.end()) {
      continue;
    }
    std::ranges::sort(it->second);
    for (pid_t child : it->second) {
      if (!visited.insert(child).second) {
        continue;
      }
      queue.push_back(child);
      std::string cgroup;
      if (base::ReadFileToString(
              proc_dir.Append(base::NumberToString(child)).Append("cgroup"),
              &cgroup) &&
          cgroup != root_cgroup) {
        result.push_back(child);
      }
    }
  }
  return result;
}

void SetSystemdScopeUnitNameForXdgPortal(dbus::Bus* bus,
                                         const base::FilePath& proc_dir,
                                         SystemdUnitCallback callback) {
#if DCHECK_IS_ON()
  static base::SequenceChecker sequence_checker;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
#endif

  if (base::nix::GetAppSandbox() != base::nix::AppSandbox::kNone) {
    // xdg-desktop-portal has a separate reliable way of detecting the
    // application name for Flatpak and Snap environments, so the systemd unit
    // is not necessary in these cases.
    std::move(callback).Run(SystemdUnitStatus::kUnitNotNecessary);
    return;
  }

  // Check if the systemd service is available
  dbus_utils::NameHasOwner(
      bus, kServiceNameSystemd,
      base::BindOnce(&OnNameHasOwnerResponse, base::WrapRefCounted(bus),
                     proc_dir, std::move(callback)));
}

}  // namespace internal

}  // namespace dbus_xdg
