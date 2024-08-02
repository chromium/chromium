// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/wayland/zcr_notification_shell.h"

#include <notification-shell-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include <optional>
#include <string>

#include "base/atomic_sequence_num.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "components/exo/notification.h"
#include "components/exo/notification_surface.h"
#include "components/exo/notification_surface_manager.h"
#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {

namespace {

// Notification id and notifier id used for NotificationShell.
constexpr char kNotificationShellNotificationIdFormat[] =
    "exo-notification-shell.%d.%s";
constexpr char kNotificationShellNotifierId[] = "exo-notification-shell";

// Incremental id for notification shell instance.
base::AtomicSequenceNumber g_next_notification_shell_id;

////////////////////////////////////////////////////////////////////////////////
// zcr_notification_shell_notification_v1 interface:

// Implements notification interface.
class WaylandNotificationShellNotification {
 public:
  WaylandNotificationShellNotification(const std::string& title,
                                       const std::string& message,
                                       const std::string& display_source,
                                       const std::string& notification_id,
                                       const std::vector<std::string>& buttons,
                                       wl_resource* resource)
      : resource_(resource) {
    notification_ = std::make_unique<Notification>(
        title, message, display_source, notification_id,
        kNotificationShellNotifierId, buttons,
        base::BindRepeating(&WaylandNotificationShellNotification::OnClose,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&WaylandNotificationShellNotification::OnClick,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  WaylandNotificationShellNotification(
      const WaylandNotificationShellNotification&) = delete;
  WaylandNotificationShellNotification& operator=(
      const WaylandNotificationShellNotification&) = delete;

  void Close() { notification_->Close(); }

 private:
  void OnClose(bool by_user) {
    zcr_notification_shell_notification_v1_send_closed(resource_, by_user);
    wl_client_flush(wl_resource_get_client(resource_));
  }

  void OnClick(const std::optional<int>& button_index) {
    int32_t index = button_index ? *button_index : -1;
    zcr_notification_shell_notification_v1_send_clicked(resource_, index);
    wl_client_flush(wl_resource_get_client(resource_));
  }

  const raw_ptr<wl_resource> resource_;
  std::unique_ptr<Notification> notification_;

  base::WeakPtrFactory<WaylandNotificationShellNotification> weak_ptr_factory_{
      this};
};

void notification_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void notification_close(wl_client* client, wl_resource* resource) {
  GetUserDataAs<WaylandNotificationShellNotification>(resource)->Close();
}

const struct zcr_notification_shell_notification_v1_interface
    notification_implementation = {notification_destroy, notification_close};

////////////////////////////////////////////////////////////////////////////////
// zcr_notification_shell_v1 interface:

// Implements notification shell interface.
class WaylandNotificationShell {
 public:
  WaylandNotificationShell() : id_(g_next_notification_shell_id.GetNext()) {}

  WaylandNotificationShell(const WaylandNotificationShell&) = delete;
  WaylandNotificationShell& operator=(const WaylandNotificationShell&) = delete;

  ~WaylandNotificationShell() = default;

  // Creates a notification on message center from textual information.
  std::unique_ptr<WaylandNotificationShellNotification> CreateNotification(
      const std::string& title,
      const std::string& message,
      const std::string& display_source,
      const std::string& notification_key,
      const std::vector<std::string>& buttons,
      wl_resource* notification_resource) {
    auto notification_id = base::StringPrintf(
        kNotificationShellNotificationIdFormat, id_, notification_key.c_str());

    return std::make_unique<WaylandNotificationShellNotification>(
        title, message, display_source, notification_id, buttons,
        notification_resource);
  }

 private:
  // Id for this notification shell instance.
  const uint32_t id_;
};

void notification_shell_create_notification(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t id,
                                            const char* title,
                                            const char* message,
                                            const char* display_source,
                                            const char* notification_key,
                                            wl_array* buttons) {
  wl_resource* notification_resource = wl_resource_create(
      client, &zcr_notification_shell_notification_v1_interface,
      wl_resource_get_version(resource), id);

  // Converts wl_array of strings into std::vector<std::string>. All elements
  // are 0-terminated so we use it as the mark of the element's end.
  std::vector<std::string> button_strings;
  const char* data = static_cast<const char*>(buttons->data);
  int len = 0;
  for (const char *pos = data; pos < data + buttons->size; ++pos, ++len) {
    if (*pos == '\0') {
      button_strings.emplace_back(std::string(pos - len, len));
      len = 0;
    }
  }

  std::unique_ptr<WaylandNotificationShellNotification> notification =
      GetUserDataAs<WaylandNotificationShell>(resource)->CreateNotification(
          title, message, display_source, notification_key, button_strings,
          notification_resource);

  SetImplementation(notification_resource, &notification_implementation,
                    std::move(notification));
}

void notification_shell_get_notification_surface(wl_client* client,
                                                 wl_resource* resource,
                                                 uint32_t id,
                                                 wl_resource* surface,
                                                 const char* notification_key) {
  NOTIMPLEMENTED();
}

const struct zcr_notification_shell_v1_interface
    zcr_notification_shell_v1_implementation = {
        notification_shell_create_notification,
        notification_shell_get_notification_surface,
};

}  // namespace

void bind_notification_shell(wl_client* client,
                             void* data,
                             uint32_t version,
                             uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_notification_shell_v1_interface, 1, id);

  SetImplementation(resource, &zcr_notification_shell_v1_implementation,
                    std::make_unique<WaylandNotificationShell>());
}

}  // namespace wayland
}  // namespace exo
