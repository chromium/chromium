// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/custom_handlers/test_protocol_handler_registry_delegate.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"

namespace custom_handlers {

TestProtocolHandlerRegistryDelegate::TestProtocolHandlerRegistryDelegate() =
    default;

TestProtocolHandlerRegistryDelegate::~TestProtocolHandlerRegistryDelegate() =
    default;

// ProtocolHandlerRegistry::Delegate:
void TestProtocolHandlerRegistryDelegate::RegisterExternalHandler(
    const std::string& protocol) {
  bool inserted = registered_protocols_.insert(protocol).second;
  DCHECK(inserted);
}

void TestProtocolHandlerRegistryDelegate::DeregisterExternalHandler(
    const std::string& protocol) {
  size_t removed = registered_protocols_.erase(protocol);
  DCHECK_EQ(removed, 1u);
}

bool TestProtocolHandlerRegistryDelegate::IsExternalHandlerRegistered(
    const std::string& protocol) {
  return registered_protocols_.find(protocol) != registered_protocols_.end();
}

void TestProtocolHandlerRegistryDelegate::RegisterWithOSAsDefaultClient(
    const std::string& protocol,
    DefaultClientCallback callback) {
  // Do as-if the registration has to run on another sequence and post back
  // the result with a task to the current thread.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), !force_os_failure_));

  if (!force_os_failure_)
    os_registered_protocols_.insert(protocol);
}

void TestProtocolHandlerRegistryDelegate::CheckDefaultClientWithOS(
    const std::string& protocol,
    DefaultClientCallback callback) {
  // Respond asynchronously to mimic the real behavior.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

bool TestProtocolHandlerRegistryDelegate::ShouldRemoveHandlersNotInOS() {
  return true;
}

bool TestProtocolHandlerRegistryDelegate::IsFakeRegisteredWithOS(
    const std::string& protocol) {
  return os_registered_protocols_.find(protocol) !=
         os_registered_protocols_.end();
}

void TestProtocolHandlerRegistryDelegate::Reset() {
  registered_protocols_.clear();
  os_registered_protocols_.clear();
  force_os_failure_ = false;
}

}  // namespace custom_handlers
