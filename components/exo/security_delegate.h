// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SECURITY_DELEGATE_H_
#define COMPONENTS_EXO_SECURITY_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace aura {
class Window;
}

namespace base {
class Pickle;
class RefCountedMemory;
}  // namespace base

namespace ui {
struct FileInfo;
enum class EndpointType;
}  // namespace ui

namespace exo {

// Each wayland server managed by exo, including the default server, will have a
// single delegate associated with it to control security-sensitive features of
// the server, e.g.:
//  - Availability of privileged APIs used by trusted clients only.
//  - Handling of certain mechanisms differently for different products (arc,
//    crostini, etc)
// This allows exo to make strong guarantees about the relationship between the
// wl clients and the SecurityDelegate the server owns.
//
// See go/secure-exo-ids and go/securer-exo-ids for more details.
class SecurityDelegate {
 public:
  // See |CanSetBounds()|.
  enum SetBoundsPolicy {
    // By default, clients may not set window bounds. Requests are ignored.
    IGNORE,

    // Clients may set bounds, but Exo may DCHECK on requests for windows with
    // server-side decoration.
    DCHECK_IF_DECORATED,

    // Clients may set bounds for any window. Exo will expand the requested
    // bounds to account for server-side decorations, if any.
    ADJUST_IF_DECORATED,
  };

  virtual ~SecurityDelegate() = default;

  // "Self-activation" is a security sensitive windowing operation that is a
  // common paradigm in X11. The need to self-activate is controlled
  // per-subsystem, i.e. a product like ARC++ knows that its windows should be
  // able to self activate, whereas Crostini knows they usually shouldn't.
  virtual bool CanSelfActivate(aura::Window* window) const = 0;

  // Called when a client made pointer lock request, defined in
  // pointer-constraints-unstable-v1.xml extension protocol.  True if the client
  // can lock the location of the pointer and disable movement, or return false
  // to reject the pointer lock request.
  virtual bool CanLockPointer(aura::Window* window) const = 0;

  // Whether clients may set their own windows' bounds is a security-relevant
  // policy decision.
  //
  // If server-side decoration is used, clients normally should not set their
  // own window bounds, as they may not be able to compute them correctly
  // (accounting for the size of the window decorations).
  virtual SetBoundsPolicy CanSetBounds(aura::Window* window) const = 0;

  // Read filenames from text/uri-list |data| which was provided by `source`
  // endpoint. Translates paths from source to host format.
  virtual std::vector<ui::FileInfo> GetFilenames(
      ui::EndpointType source,
      const std::vector<uint8_t>& data) const = 0;

  // Sends the given list of `files` to `target` endpoint. Translates paths from
  // host format to the target and performs any required file sharing for VMs.
  using SendDataCallback =
      base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)>;
  virtual void SendFileInfo(ui::EndpointType target,
                            const std::vector<ui::FileInfo>& files,
                            SendDataCallback callback) const = 0;

  // Takes in `pickle` constructed by the web contents view containing
  // filesystem URLs. Provides translations for the specified `target` endpoint
  // and performs any required file sharing for VMs.
  virtual void SendPickle(ui::EndpointType target,
                          const base::Pickle& pickle,
                          SendDataCallback callback) = 0;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SECURITY_DELEGATE_H_
