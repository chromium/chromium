// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/protobuf_init.h"

#include <google/protobuf/generated_message_util.h>
#include "base/check.h"
#include "base/threading/scoped_thread_priority.h"

namespace chrome {
namespace {

using ScopedBoostThreadPriority =
    base::internal::ScopedMayLoadLibraryAtBackgroundPriority;
ScopedBoostThreadPriority* g_boost_thread_priority;

void EnterInitSCC() {
  DCHECK(!g_boost_thread_priority);
  g_boost_thread_priority = new ScopedBoostThreadPriority(FROM_HERE, nullptr);
}

void LeaveInitSCC() {
  DCHECK(g_boost_thread_priority);
  delete g_boost_thread_priority;
  g_boost_thread_priority = nullptr;
}

}  // namespace

void InitializeProtobuf() {
  google::protobuf::internal::RegisterInitSCCHooks(EnterInitSCC, LeaveInitSCC);
}

}  // namespace chrome
