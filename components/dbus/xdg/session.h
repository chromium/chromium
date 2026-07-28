// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_XDG_SESSION_H_
#define COMPONENTS_DBUS_XDG_SESSION_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/xdg/portal_constants.h"
#include "components/dbus/xdg/request.h"
#include "dbus/object_path.h"

namespace dbus {
class Bus;
class ObjectProxy;
}  // namespace dbus

namespace dbus_xdg {

class Session;

// Manages the lifecycle of an XDG Desktop Portal Session
// (org.freedesktop.portal.Session).
// https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Session.html
class COMPONENT_EXPORT(COMPONENTS_DBUS) Session {
 public:
  using CreateCallback = base::OnceCallback<void(Session* session)>;

  // Asynchronously creates an XDG Desktop Portal Session for portals that use
  // the Request pattern (e.g. org.freedesktop.portal.GlobalShortcuts,
  // ScreenCast, RemoteDesktop).
  // `CreateSession` returns an org.freedesktop.portal.Request handle object
  // path, and the session handle is delivered via the Request::Response signal.
  // Calls `create_callback` with `this` on success, or nullptr on failure.
  static std::unique_ptr<Session> CreateWithRequest(
      scoped_refptr<dbus::Bus> bus,
      dbus::ObjectProxy* portal_proxy,
      const std::string& interface_name,
      Dictionary options,
      CreateCallback create_callback);

  // Asynchronously creates an XDG Desktop Portal Session for portals where
  // `CreateSession` directly returns the session handle object path in the
  // D-Bus method reply (e.g. org.freedesktop.portal.Location).
  // Calls `create_callback` with `this` on success, or nullptr on failure.
  static std::unique_ptr<Session> CreateDirect(
      scoped_refptr<dbus::Bus> bus,
      dbus::ObjectProxy* portal_proxy,
      const std::string& interface_name,
      Dictionary options,
      CreateCallback create_callback);

  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  Session(Session&&) = delete;
  Session& operator=(Session&&) = delete;

  ~Session();

  const dbus::ObjectPath& path() const { return session_path_; }
  dbus::ObjectProxy* proxy() const { return session_proxy_; }

 private:
  friend class SessionTest;
  FRIEND_TEST_ALL_PREFIXES(SessionTest, DestructorCallsClose);

  Session(scoped_refptr<dbus::Bus> bus, const std::string& portal_service_name);

  // Constructs a Session wrapper around `session_path`.
  // When destructed, automatically calls "Close" on the portal session.
  Session(scoped_refptr<dbus::Bus> bus, const dbus::ObjectPath& session_path);

  Session(scoped_refptr<dbus::Bus> bus,
          const dbus::ObjectPath& session_path,
          const std::string& portal_service_name);

  static void HandleCreatedSession(base::WeakPtr<Session> session,
                                   scoped_refptr<dbus::Bus> bus,
                                   const std::string& portal_service_name,
                                   CreateCallback create_callback,
                                   const dbus::ObjectPath& session_path);

  static void OnCreateSessionRequestResponse(
      base::WeakPtr<Session> session,
      scoped_refptr<dbus::Bus> bus,
      const std::string& portal_service_name,
      CreateCallback create_callback,
      Results results);

  static void OnCreateSessionDirectResponse(
      base::WeakPtr<Session> session,
      scoped_refptr<dbus::Bus> bus,
      const std::string& portal_service_name,
      CreateCallback create_callback,
      dbus_utils::CallMethodResultSig<"o"> result);

  void Initialize(const std::string& portal_service_name);

  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectPath session_path_;
  raw_ptr<dbus::ObjectProxy> session_proxy_ = nullptr;
  std::string portal_service_name_;
  std::unique_ptr<Request> create_request_;

  base::WeakPtrFactory<Session> weak_ptr_factory_{this};
};

}  // namespace dbus_xdg

#endif  // COMPONENTS_DBUS_XDG_SESSION_H_
