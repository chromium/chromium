// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_XDG_BACKGROUND_PORTAL_H_
#define COMPONENTS_DBUS_XDG_BACKGROUND_PORTAL_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/dbus/xdg/request.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace dbus_xdg {

// Result returned by `RequestBackground`.
struct COMPONENT_EXPORT(COMPONENTS_DBUS) BackgroundResult {
  bool background_allowed = false;
  bool autostart_enabled = false;
};

// Options for `RequestBackground`.
struct COMPONENT_EXPORT(COMPONENTS_DBUS) BackgroundOptions {
  BackgroundOptions();
  BackgroundOptions(const BackgroundOptions&);
  BackgroundOptions& operator=(const BackgroundOptions&);
  BackgroundOptions(BackgroundOptions&&);
  BackgroundOptions& operator=(BackgroundOptions&&);
  ~BackgroundOptions();

  std::string reason;
  bool autostart = false;
  std::vector<std::string> commandline;
  bool dbus_activatable = false;
};

using BackgroundResultCallback =
    base::OnceCallback<void(base::expected<BackgroundResult, ResponseError>)>;

using SetStatusCallback = base::OnceCallback<void(bool success)>;

// Pure virtual client interface for org.freedesktop.portal.Background.
// https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Background.html
class COMPONENT_EXPORT(COMPONENTS_DBUS) BackgroundPortalClient {
 public:
  virtual ~BackgroundPortalClient() = default;

  // Requests background running permissions and optional autostart capability.
  virtual void RequestBackground(const std::string& parent_window,
                                 const BackgroundOptions& options,
                                 BackgroundResultCallback callback) = 0;

  // Sets the status message of the application running in the background.
  virtual void SetStatus(const std::string& message,
                         SetStatusCallback callback) = 0;
};

// Default D-Bus portal client implementation of `BackgroundPortalClient`.
class COMPONENT_EXPORT(COMPONENTS_DBUS) BackgroundPortal
    : public BackgroundPortalClient {
 public:
  BackgroundPortal();
  explicit BackgroundPortal(scoped_refptr<dbus::Bus> bus);
  BackgroundPortal(const BackgroundPortal&) = delete;
  BackgroundPortal& operator=(const BackgroundPortal&) = delete;
  ~BackgroundPortal() override;

  // BackgroundPortalClient implementation:
  void RequestBackground(const std::string& parent_window,
                         const BackgroundOptions& options,
                         BackgroundResultCallback callback) override;

  void SetStatus(const std::string& message,
                 SetStatusCallback callback) override;

 private:
  void OnPortalReadyForRequestBackground(uint64_t generation,
                                         const std::string& parent_window,
                                         const BackgroundOptions& options,
                                         BackgroundResultCallback callback,
                                         uint32_t version);
  void OnRequestFinished(
      uint64_t generation,
      BackgroundResultCallback callback,
      base::expected<BackgroundResult, ResponseError> result);

  void OnPortalReadyForSetStatus(const std::string& message,
                                 SetStatusCallback callback,
                                 uint32_t version);

  scoped_refptr<dbus::Bus> bus_;
  std::unique_ptr<Request> active_request_;
  uint64_t request_generation_ = 0;
  base::WeakPtrFactory<BackgroundPortal> weak_ptr_factory_{this};
};

}  // namespace dbus_xdg

#endif  // COMPONENTS_DBUS_XDG_BACKGROUND_PORTAL_H_
