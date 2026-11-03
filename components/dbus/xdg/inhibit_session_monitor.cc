// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/inhibit_session_monitor.h"

#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/unguessable_token.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/xdg/portal.h"
#include "components/dbus/xdg/portal_constants.h"
#include "components/dbus/xdg/request.h"
#include "components/dbus/xdg/session.h"
#include "dbus/bus.h"
#include "dbus/object_proxy.h"

namespace dbus_xdg {

namespace {

constexpr char kInhibitInterfaceName[] = "org.freedesktop.portal.Inhibit";
constexpr char kMethodCreateMonitor[] = "CreateMonitor";
constexpr char kMethodQueryEndResponse[] = "QueryEndResponse";
constexpr char kSignalStateChanged[] = "StateChanged";
constexpr char kSessionStateKey[] = "session-state";

void OnQueryEndResponse(dbus_utils::CallMethodResultSig<""> result) {
  if (!result.has_value()) {
    VLOG(1) << "Inhibit.QueryEndResponse failed";
  }
}

}  // namespace

InhibitSessionMonitor::InhibitSessionMonitor(
    scoped_refptr<dbus::Bus> bus,
    base::OnceClosure session_ending_callback)
    : bus_(std::move(bus)),
      session_ending_callback_(std::move(session_ending_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(bus_);
  RequestXdgDesktopPortal(bus_.get(), kInhibitInterfaceName,
                          base::BindOnce(&InhibitSessionMonitor::OnPortalReady,
                                         weak_ptr_factory_.GetWeakPtr()));
}

InhibitSessionMonitor::~InhibitSessionMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool InhibitSessionMonitor::has_session_for_testing() const {
  return session_ && session_->path().IsValid();
}

void InhibitSessionMonitor::OnPortalReady(uint32_t version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (version < kMinInhibitVersion) {
    return;
  }
  portal_proxy_ = bus_->GetObjectProxy(kPortalServiceName,
                                       dbus::ObjectPath(kPortalObjectPath));
  // Connect to StateChanged before creating the monitor, since the portal
  // reports the initial state right after creating it.
  dbus_utils::ConnectToSignal<"oa{sv}">(
      portal_proxy_, kInhibitInterfaceName, kSignalStateChanged,
      base::BindRepeating(&InhibitSessionMonitor::OnStateChanged,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&InhibitSessionMonitor::OnStateChangedConnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InhibitSessionMonitor::OnStateChangedConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    VLOG(1) << "Failed to connect to " << interface_name << "." << signal_name;
    return;
  }

  std::string token = base::UnguessableToken::Create().ToString();
  session_path_ = dbus::ObjectPath(
      base::nix::XdgDesktopPortalSessionPath(bus_->GetConnectionName(), token));
  Dictionary options;
  options["session_handle_token"] =
      dbus_utils::Variant::Wrap<"s">(std::move(token));
  session_ = Session::CreateWithRequestForMethod(
      bus_, portal_proxy_, kInhibitInterfaceName, kMethodCreateMonitor,
      /*parent_window=*/std::string(), std::move(options),
      base::BindOnce(&InhibitSessionMonitor::OnSessionCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InhibitSessionMonitor::OnSessionCreated(Session* session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!session) {
    VLOG(1) << "Failed to create an Inhibit session state monitor";
    session_.reset();
    session_path_ = dbus::ObjectPath();
    return;
  }
  session_path_ = session->path();
}

void InhibitSessionMonitor::OnStateChanged(
    dbus_utils::ConnectToSignalResultSig<"oa{sv}"> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value() || !session_path_.IsValid()) {
    return;
  }
  auto& [session_handle, state] = result.value();
  if (session_handle != session_path_) {
    return;
  }
  auto it = state.find(kSessionStateKey);
  if (it == state.end()) {
    return;
  }
  std::optional<uint32_t> session_state =
      std::move(it->second).Take<uint32_t>();
  if (!session_state) {
    return;
  }

  // The state comes from the portal over D-Bus, so it may have a value not in
  // SessionState (e.g. one added by a later version of the spec, or from a
  // misbehaving portal). Such values are ignored instead of hitting a
  // NOTREACHED().
  switch (static_cast<SessionState>(*session_state)) {
    case SessionState::kRunning:
      break;
    case SessionState::kQueryEnd:
      // The session may still return to "Running" if logout is cancelled, so
      // only acknowledge the query here.
      dbus_utils::CallMethod<"o", "">(
          portal_proxy_, kInhibitInterfaceName, kMethodQueryEndResponse,
          base::BindOnce(&OnQueryEndResponse), session_path_);
      break;
    case SessionState::kEnding:
      if (session_ending_callback_) {
        std::move(session_ending_callback_).Run();
      }
      break;
  }
}

}  // namespace dbus_xdg
