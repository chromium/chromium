// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEEP_ALIVE_REGISTRY_SCOPED_KEEP_ALIVE_H_
#define COMPONENTS_KEEP_ALIVE_REGISTRY_SCOPED_KEEP_ALIVE_H_

enum class KeepAliveOrigin;
enum class KeepAliveRestartOption;

// Prevents the BrowserProcess from shutting down. Registers with
// KeepAliveRegistry on creation, and unregisters them on destruction. Use these
// objects with a unique_ptr for easy management.
//
// If you need to access a particular Profile (or its KeyedServices) during the
// same period, you should use a ScopedProfileKeepAlive as well.
//
// Note: The objects should only be created and destroyed on the main thread.
//
// Note: The registration will hit a CHECK if it happens while we are
// shutting down. Caller code should make sure that this can't happen.
class ScopedKeepAlive {
 public:
  ScopedKeepAlive(KeepAliveOrigin origin, KeepAliveRestartOption restart);

  ScopedKeepAlive(const ScopedKeepAlive&) = delete;
  ScopedKeepAlive& operator=(const ScopedKeepAlive&) = delete;

  ~ScopedKeepAlive();

 private:
  const KeepAliveOrigin origin_;
  const KeepAliveRestartOption restart_;
};

#endif  // COMPONENTS_KEEP_ALIVE_REGISTRY_SCOPED_KEEP_ALIVE_H_
