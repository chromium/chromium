// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_SCOPED_THREAD_POOL_H_
#define CHROME_INSTALLER_SETUP_SCOPED_THREAD_POOL_H_

// Initializes a thread pool upon creation, and shuts it down upon destruction.
class ScopedThreadPool {
 public:
  ScopedThreadPool();

  ScopedThreadPool(const ScopedThreadPool&) = delete;
  ScopedThreadPool& operator=(const ScopedThreadPool&) = delete;

  ~ScopedThreadPool();
};

#endif  // CHROME_INSTALLER_SETUP_SCOPED_THREAD_POOL_H_
