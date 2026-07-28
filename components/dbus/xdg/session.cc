// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/session.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/connect_to_signal.h"
#include "components/dbus/xdg/portal_constants.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace dbus_xdg {

namespace {

constexpr char kSessionInterfaceName[] = "org.freedesktop.portal.Session";
constexpr char kMethodCreateSession[] = "CreateSession";
constexpr char kMethodClose[] = "Close";

void CloseSession(scoped_refptr<dbus::Bus> bus,
                  const std::string& portal_service_name,
                  const dbus::ObjectPath& session_path) {
  if (!session_path.IsValid() || !bus) {
    return;
  }
  auto* session_proxy = bus->GetObjectProxy(portal_service_name, session_path);
  if (!session_proxy) {
    return;
  }
  dbus_utils::CallMethod<"", "">(
      session_proxy, kSessionInterfaceName, kMethodClose,
      base::BindOnce(
          [](scoped_refptr<dbus::Bus> bus,
             const std::string& portal_service_name,
             const dbus::ObjectPath& session_path,
             dbus_utils::CallMethodResultSig<""> result) {
            bus->RemoveObjectProxy(portal_service_name, session_path,
                                   base::DoNothing());
          },
          bus, portal_service_name, session_path));
}

void EnsureSessionHandleToken(Dictionary& options) {
  // Find "session_handle_token" to see if it is already provided (e.g. by tests
  // or a caller desiring a stable session handle, like
  // GlobalAcceleratorListener).
  auto it = options.find("session_handle_token");
  if (it == options.end()) {
    options.insert(
        it, std::make_pair("session_handle_token",
                           dbus_utils::Variant::Wrap<"s">(
                               base::UnguessableToken::Create().ToString())));
  }
}

}  // namespace

Session::Session(scoped_refptr<dbus::Bus> bus,
                 const std::string& portal_service_name)
    : bus_(std::move(bus)), portal_service_name_(portal_service_name) {
  CHECK(bus_);
}

// static
std::unique_ptr<Session> Session::CreateWithRequest(
    scoped_refptr<dbus::Bus> bus,
    dbus::ObjectProxy* portal_proxy,
    const std::string& interface_name,
    Dictionary options,
    CreateCallback create_callback) {
  EnsureSessionHandleToken(options);
  auto session =
      base::WrapUnique(new Session(std::move(bus), kPortalServiceName));
  session->create_request_ = Request::CreateWithPortalServiceName(
      session->bus_, portal_proxy, interface_name, kMethodCreateSession,
      std::move(options),
      base::BindOnce(&Session::OnCreateSessionRequestResponse,
                     session->weak_ptr_factory_.GetWeakPtr(), session->bus_,
                     session->portal_service_name_, std::move(create_callback)),
      session->portal_service_name_);
  return session;
}

// static
std::unique_ptr<Session> Session::CreateDirect(
    scoped_refptr<dbus::Bus> bus,
    dbus::ObjectProxy* portal_proxy,
    const std::string& interface_name,
    Dictionary options,
    CreateCallback create_callback) {
  EnsureSessionHandleToken(options);
  auto session =
      base::WrapUnique(new Session(std::move(bus), kPortalServiceName));
  dbus_utils::CallMethod<"a{sv}", "o">(
      portal_proxy, interface_name, kMethodCreateSession,
      base::BindOnce(&Session::OnCreateSessionDirectResponse,
                     session->weak_ptr_factory_.GetWeakPtr(), session->bus_,
                     session->portal_service_name_, std::move(create_callback)),
      std::move(options));
  return session;
}

Session::Session(scoped_refptr<dbus::Bus> bus,
                 const dbus::ObjectPath& session_path)
    : Session(std::move(bus), session_path, kPortalServiceName) {}

Session::Session(scoped_refptr<dbus::Bus> bus,
                 const dbus::ObjectPath& session_path,
                 const std::string& portal_service_name)
    : bus_(std::move(bus)),
      session_path_(session_path),
      portal_service_name_(portal_service_name) {
  CHECK(bus_);
  Initialize(portal_service_name_);
}

Session::~Session() {
  CloseSession(std::move(bus_), portal_service_name_, session_path_);
}

// static
void Session::HandleCreatedSession(base::WeakPtr<Session> session,
                                   scoped_refptr<dbus::Bus> bus,
                                   const std::string& portal_service_name,
                                   CreateCallback create_callback,
                                   const dbus::ObjectPath& session_path) {
  if (!session) {
    CloseSession(std::move(bus), portal_service_name, session_path);
    return;
  }

  session->create_request_.reset();
  if (session_path.IsValid()) {
    session->session_path_ = session_path;
    session->Initialize(portal_service_name);
  }

  if (create_callback) {
    // Run the callback asynchronously to prevent re-entrancy issues where
    // callers assume the Session unique pointer is already populated in their
    // own execution context, and to prevent unit test failures.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](CreateCallback callback, base::WeakPtr<Session> session) {
              if (session) {
                std::move(callback).Run(
                    session->path().IsValid() ? session.get() : nullptr);
              }
            },
            std::move(create_callback), session));
  }
}

// static
void Session::OnCreateSessionRequestResponse(
    base::WeakPtr<Session> session,
    scoped_refptr<dbus::Bus> bus,
    const std::string& portal_service_name,
    CreateCallback create_callback,
    Results results) {
  if (!results.has_value()) {
    HandleCreatedSession(std::move(session), std::move(bus),
                         portal_service_name, std::move(create_callback),
                         dbus::ObjectPath());
    return;
  }

  dbus::ObjectPath session_path;
  auto it = results->find("session_handle");
  if (it != results->end()) {
    // Some portals (e.g. GlobalShortcuts, ScreenCast, RemoteDesktop) errantly
    // specify session_handle as a string ("s") in the Response signal
    // dictionary, while other portal specs (e.g. InputCapture) specify it as
    // an ObjectPath ("o"). Support both types for compatibility.
    if (it->second.signature() == "s") {
      if (auto session_handle_str = std::move(it->second).Take<std::string>()) {
        dbus::ObjectPath path(*session_handle_str);
        if (path.IsValid()) {
          session_path = std::move(path);
        }
      }
    } else if (it->second.signature() == "o") {
      if (auto session_handle_path =
              std::move(it->second).Take<dbus::ObjectPath>()) {
        if (session_handle_path->IsValid()) {
          session_path = std::move(*session_handle_path);
        }
      }
    }
  }

  HandleCreatedSession(std::move(session), std::move(bus), portal_service_name,
                       std::move(create_callback), session_path);
}

// static
void Session::OnCreateSessionDirectResponse(
    base::WeakPtr<Session> session,
    scoped_refptr<dbus::Bus> bus,
    const std::string& portal_service_name,
    CreateCallback create_callback,
    dbus_utils::CallMethodResultSig<"o"> result) {
  dbus::ObjectPath session_path;
  if (result.has_value()) {
    session_path = std::get<0>(*result);
  }

  HandleCreatedSession(std::move(session), std::move(bus), portal_service_name,
                       std::move(create_callback), session_path);
}

void Session::Initialize(const std::string& portal_service_name) {
  CHECK(session_path_.IsValid());
  session_proxy_ = bus_->GetObjectProxy(portal_service_name, session_path_);
}

}  // namespace dbus_xdg
