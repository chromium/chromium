// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_XDG_FILE_TRANSFER_PORTAL_H_
#define COMPONENTS_DBUS_XDG_FILE_TRANSFER_PORTAL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/dbus/utils/read_message.h"
#include "components/dbus/utils/signature.h"
#include "components/dbus/utils/types.h"
#include "components/dbus/utils/write_value.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace dbus_xdg {

COMPONENT_EXPORT(COMPONENTS_DBUS)
BASE_DECLARE_FEATURE(kXdgFileTransferPortal);

// Helper class to communicate with the org.freedesktop.portal.FileTransfer
// portal, allowing sandboxed applications to transfer files.
class COMPONENT_EXPORT(COMPONENTS_DBUS) FileTransferPortal {
 public:
  // Returns true via `callback` if the FileTransfer portal is available on the
  // bus. This should only be called from the UI thread.
  static void IsAvailable(base::OnceCallback<void(bool)> callback,
                          dbus::Bus* bus = nullptr);

  // Asynchronously retrieves the list of files associated with the given
  // transfer `key`. Returns a list of absolute file paths via `callback`.
  static void RetrieveFiles(
      const std::string& key,
      base::OnceCallback<void(std::vector<std::string>)> callback,
      dbus::Bus* bus = nullptr);

  // Asynchronously registers a list of files with the portal and returns
  // a transfer key via `callback`. `files` is a list of absolute file paths.
  // Returns an empty string on failure.
  static void RegisterFiles(const std::vector<std::string>& files,
                            base::OnceCallback<void(std::string)> callback,
                            dbus::Bus* bus = nullptr);
};

}  // namespace dbus_xdg

#endif  // COMPONENTS_DBUS_XDG_FILE_TRANSFER_PORTAL_H_
