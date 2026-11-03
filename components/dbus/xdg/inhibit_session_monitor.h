// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_XDG_INHIBIT_SESSION_MONITOR_H_
#define COMPONENTS_DBUS_XDG_INHIBIT_SESSION_MONITOR_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/dbus/utils/connect_to_signal.h"
#include "dbus/object_path.h"

namespace dbus {
class Bus;
class ObjectProxy;
}  // namespace dbus

namespace dbus_xdg {

class Session;

// Watches the desktop session state through a session state monitor of
// org.freedesktop.portal.Inhibit (version 3 or later), which works inside
// Flatpak and Snap sandboxes and on desktops without other session end
// notifications (e.g. KDE Plasma).
// https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Inhibit.html
//
// When the session state becomes "Query End", the monitor acknowledges it with
// QueryEndResponse() right away, since the portal waits at most one second for
// the acknowledgement. The session may still return to "Running" (e.g. when
// the user cancels logout because another application inhibits it), so
// `session_ending_callback` runs only once the state becomes "Ending".
class COMPONENT_EXPORT(COMPONENTS_DBUS) InhibitSessionMonitor {
 public:
  // Session states reported in the "session-state" member of StateChanged.
  // The values are defined by the org.freedesktop.portal.Inhibit spec.
  enum class SessionState : uint32_t {
    kRunning = 1,
    kQueryEnd = 2,
    kEnding = 3,
  };

  // The minimum version of the Inhibit interface that reports
  // "session-state" and supports QueryEndResponse().
  static constexpr uint32_t kMinInhibitVersion = 3;

  // Starts monitoring on `bus`, which must be a non-null session bus. Does
  // nothing if the portal or a recent enough Inhibit interface is unavailable.
  // `session_ending_callback` runs at most once, never after `this` is
  // destroyed.
  InhibitSessionMonitor(scoped_refptr<dbus::Bus> bus,
                        base::OnceClosure session_ending_callback);
  InhibitSessionMonitor(const InhibitSessionMonitor&) = delete;
  InhibitSessionMonitor& operator=(const InhibitSessionMonitor&) = delete;
  ~InhibitSessionMonitor();

  // Whether the monitor session has been created, for testing.
  bool has_session_for_testing() const;

 private:
  void OnPortalReady(uint32_t version);
  void OnStateChangedConnected(const std::string& interface_name,
                               const std::string& signal_name,
                               bool success);
  void OnSessionCreated(Session* session);
  void OnStateChanged(dbus_utils::ConnectToSignalResultSig<"oa{sv}"> result);

  const scoped_refptr<dbus::Bus> bus_;
  raw_ptr<dbus::ObjectProxy> portal_proxy_ = nullptr;
  base::OnceClosure session_ending_callback_;

  // The object path of the monitor session. Known before CreateMonitor
  // returns, so StateChanged signals sent before the Request::Response signal
  // are not missed.
  dbus::ObjectPath session_path_;
  std::unique_ptr<Session> session_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<InhibitSessionMonitor> weak_ptr_factory_{this};
};

}  // namespace dbus_xdg

#endif  // COMPONENTS_DBUS_XDG_INHIBIT_SESSION_MONITOR_H_
