// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CUSTOM_HANDLERS_TEST_PROTOCOL_HANDLER_REGISTRY_DELEGATE_H_
#define COMPONENTS_CUSTOM_HANDLERS_TEST_PROTOCOL_HANDLER_REGISTRY_DELEGATE_H_

#include <set>
#include <string>

#include "components/custom_handlers/protocol_handler_registry.h"

namespace custom_handlers {

// A test ProtocolHandlerRegistry::Delegate implementation that keeps track of
// registered protocols and doesn't change any OS settings.
class TestProtocolHandlerRegistryDelegate
    : public custom_handlers::ProtocolHandlerRegistry::Delegate {
 public:
  TestProtocolHandlerRegistryDelegate();
  ~TestProtocolHandlerRegistryDelegate() override;

  TestProtocolHandlerRegistryDelegate(
      const TestProtocolHandlerRegistryDelegate& other) = delete;
  TestProtocolHandlerRegistryDelegate& operator=(
      const TestProtocolHandlerRegistryDelegate& other) = delete;

  // ProtocolHandlerRegistry::Delegate:
  void RegisterExternalHandler(const std::string& protocol) override;
  void DeregisterExternalHandler(const std::string& protocol) override;
  bool IsExternalHandlerRegistered(const std::string& protocol) override;
  void RegisterWithOSAsDefaultClient(const std::string& protocol,
                                     DefaultClientCallback callback) override;
  void CheckDefaultClientWithOS(const std::string& protocol,
                                DefaultClientCallback callback) override;
  bool ShouldRemoveHandlersNotInOS() override;

  bool IsFakeRegistered(const std::string& protocol);
  bool IsFakeRegisteredWithOS(const std::string& protocol);

  void set_force_os_failure(bool force) { force_os_failure_ = force; }
  bool force_os_failure() { return force_os_failure_; }

  void Reset();

 private:
  // Holds registered protocols.
  std::set<std::string> registered_protocols_;
  std::set<std::string> os_registered_protocols_;
  bool force_os_failure_{false};
};

}  // namespace custom_handlers

#endif  // COMPONENTS_CUSTOM_HANDLERS_TEST_PROTOCOL_HANDLER_REGISTRY_DELEGATE_H_
