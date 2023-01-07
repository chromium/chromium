// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_SANDBOX_LINUX_NACL_SANDBOX_LINUX_H_
#define COMPONENTS_NACL_LOADER_SANDBOX_LINUX_NACL_SANDBOX_LINUX_H_

#include <memory>

#include "base/files/scoped_file.h"

namespace sandbox {
class SetuidSandboxClient;
}

namespace nacl {

// NaClSandbox supports two independent layers of sandboxing.
// layer-1 uses a chroot. It requires both InitializeLayerOneSandbox() and
// SealLayerOneSandbox() to have been called to be enforcing.
// layer-2 uses seccomp-bpf. It requires the layer-1 sandbox to not yet be
// sealed when being engaged.
// For the layer-1 sandbox to work, the current process must be a child of
// the setuid sandbox. InitializeLayerOneSandbox() can only be called once
// per instance of the setuid sandbox.
//
// A typical use case of this class would be:
// 1. Load libraries and do some pre-initialization
// 2. InitializeLayerOneSandbox();
// 3. Do some more initializations (it is ok to fork() here).
// 4. CHECK(!HasOpenDirectory));
//    (This check is not strictly necessary, as the only possibility for a
//    new directory descriptor to exist after (2) has been called is via IPC)).
// 5. InitializeLayerTwoSandbox();
// 6. SealLayerOneSandbox();
// 7. CheckSandboxingStateWithPolicy();
class NaClSandbox {
 public:
  NaClSandbox();

  NaClSandbox(const NaClSandbox&) = delete;
  NaClSandbox& operator=(const NaClSandbox&) = delete;

  ~NaClSandbox();

  // This API will only work if the layer-1 sandbox is not sealed and the
  // layer-2 sandbox is not engaged.
  bool IsSingleThreaded();
  // Check whether the current process owns any directory file descriptors. This
  // will ignore any directory file descriptor owned by this object (i.e. those
  // that will be closed after SealLayerOneSandbox()) is called.
  // This API will only work if the layer-1 sandbox is not sealed and the
  // layer-2 sandbox is not engaged.
  bool HasOpenDirectory();
  // Will attempt to initialize the layer-1 sandbox, depending on flags and the
  // environment. It can only succeed if the current process is a child of the
  // setuid sandbox or was started by the namespace sandbox.
  void InitializeLayerOneSandbox();
  // Will attempt to initialize the layer-2 sandbox, depending on flags and the
  // environment.
  // This layer will also add a limit to how much of the address space can be
  // used.
  void InitializeLayerTwoSandbox();
  // Seal the layer-1 sandbox, making it enforcing.
  void SealLayerOneSandbox();
  // Check that the current sandboxing state matches the level of sandboxing
  // expected for NaCl in the current configuration. Crash if it does not.
  void CheckSandboxingStateWithPolicy();

  bool layer_one_enabled() { return layer_one_enabled_; }
  bool layer_two_enabled() { return layer_two_enabled_; }

 private:
  void CheckForExpectedNumberOfOpenFds();

  bool layer_one_enabled_;
  bool layer_one_sealed_;
  bool layer_two_enabled_;
  // |proc_fd_| must be released before the layer-1 sandbox is considered
  // enforcing.
  base::ScopedFD proc_fd_;
  std::unique_ptr<sandbox::SetuidSandboxClient> setuid_sandbox_client_;
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_LOADER_SANDBOX_LINUX_NACL_SANDBOX_LINUX_H_
