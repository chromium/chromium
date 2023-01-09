// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zwp_idle_inhibit_manager.h"

#include <idle-inhibit-unstable-v1-server-protocol.h>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/exo/wayland/server_util.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"
#include "wayland-server-core.h"

constexpr char kPowerSaveBlockerDescription[] =
    "Power save blocker created by exo wayland idle inhibitor";

namespace exo {
namespace wayland {

namespace {

////////////////////////////////////////////////////////////////////////////////
// zwp_idle_inhibitor_v1_interface:

class IdleInhibitor {
 public:
  IdleInhibitor(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner)
      : power_save_blocker_(std::make_unique<device::PowerSaveBlocker>(
            /*type=*/device::mojom::WakeLockType::kPreventDisplaySleep,
            /*reason=*/device::mojom::WakeLockReason::kOther,
            kPowerSaveBlockerDescription,
            ui_task_runner,
            blocking_task_runner)) {}

  IdleInhibitor(const IdleInhibitor&) = delete;
  IdleInhibitor& operator=(const IdleInhibitor&) = delete;

 private:
  std::unique_ptr<device::PowerSaveBlocker> power_save_blocker_;
};

void zwp_idle_inhibitor_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zwp_idle_inhibitor_v1_interface
    zwp_idle_inhibitor_v1_implementation = {zwp_idle_inhibitor_destroy};

////////////////////////////////////////////////////////////////////////////////
// zwp_idle_inhibit_manager_v1_interface:

class IdleInhibitManager {
 public:
  IdleInhibitManager()
      : ui_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
        blocking_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {}

  IdleInhibitManager(const IdleInhibitManager&) = delete;
  IdleInhibitManager& operator=(const IdleInhibitManager&) = delete;

  std::unique_ptr<IdleInhibitor> CreateInhibitor() {
    return std::make_unique<IdleInhibitor>(ui_task_runner_,
                                           blocking_task_runner_);
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner_;
};

void zwp_idle_inhibit_manager_destroy(wl_client* client,
                                      wl_resource* resource) {
  wl_resource_destroy(resource);
}

void zwp_idle_inhibit_manager_create_inhibitor(wl_client* client,
                                               wl_resource* resource,
                                               uint32_t id,
                                               wl_resource* surface) {
  wl_resource* inhibitor_resource =
      wl_resource_create(client, &zwp_idle_inhibitor_v1_interface,
                         wl_resource_get_version(resource), id);
  std::unique_ptr<IdleInhibitor> inhibitor =
      GetUserDataAs<IdleInhibitManager>(resource)->CreateInhibitor();
  SetImplementation(inhibitor_resource, &zwp_idle_inhibitor_v1_implementation,
                    std::move(inhibitor));
}

const struct zwp_idle_inhibit_manager_v1_interface
    zwp_idle_inhibit_manager_v1_implementation = {
        zwp_idle_inhibit_manager_destroy,
        zwp_idle_inhibit_manager_create_inhibitor};

}  // namespace

void bind_zwp_idle_inhibit_manager(wl_client* client,
                                   void* data,
                                   uint32_t version,
                                   uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zwp_idle_inhibit_manager_v1_interface, version, id);
  SetImplementation(resource, &zwp_idle_inhibit_manager_v1_implementation,
                    std::make_unique<IdleInhibitManager>());
}

}  // namespace wayland
}  // namespace exo
