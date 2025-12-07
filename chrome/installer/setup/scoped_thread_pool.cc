// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/scoped_thread_pool.h"

#include "base/task/thread_pool/thread_pool_instance.h"

ScopedThreadPool::ScopedThreadPool() {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("chrome_setup");
}

ScopedThreadPool::~ScopedThreadPool() {
  base::ThreadPoolInstance::Get()->Shutdown();
}
