// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/protobuf_init.h"

#include <google/protobuf/generated_message_util.h>
#include "base/check.h"
#include "base/no_destructor.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/threading/thread_local.h"

namespace chrome {
namespace {

using ScopedBoostThreadPriority =
    base::internal::ScopedMayLoadLibraryAtBackgroundPriority;
using ScopedBoostThreadPriorityTLS =
    base::ThreadLocalPointer<ScopedBoostThreadPriority>;

ScopedBoostThreadPriorityTLS& GetScopedBoostThreadPriorityTLS() {
  static base::NoDestructor<ScopedBoostThreadPriorityTLS> tls_slot;
  return *tls_slot;
}

void EnterInitSCC() {
  DCHECK(!GetScopedBoostThreadPriorityTLS().Get());
  GetScopedBoostThreadPriorityTLS().Set(
      new ScopedBoostThreadPriority(FROM_HERE, nullptr));
}

void LeaveInitSCC() {
  DCHECK(GetScopedBoostThreadPriorityTLS().Get());
  ScopedBoostThreadPriority* boost_thread_priority =
      GetScopedBoostThreadPriorityTLS().Get();
  delete boost_thread_priority;
  GetScopedBoostThreadPriorityTLS().Set(nullptr);
}

}  // namespace

void InitializeProtobuf() {
  google::protobuf::internal::RegisterInitSCCHooks(EnterInitSCC, LeaveInitSCC);
}

}  // namespace chrome
