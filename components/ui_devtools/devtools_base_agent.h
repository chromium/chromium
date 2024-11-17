// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_DEVTOOLS_BASE_AGENT_H_
#define COMPONENTS_UI_DEVTOOLS_DEVTOOLS_BASE_AGENT_H_

#include "components/ui_devtools/protocol.h"

namespace ui_devtools {

class UiDevToolsAgent {
 public:
  UiDevToolsAgent() = default;
  virtual ~UiDevToolsAgent() = default;

  virtual void Init(protocol::UberDispatcher*) = 0;
  virtual void Disable() = 0;
};

// A base agent so that any Backend implementation has access to the
// corresponding frontend instance. This also wires the backend with
// the dispatcher. When initializing an instance of this class,
// the template argument is simply the generated Metainfo class of
// any domain type such as DOM or CSS.
template <typename DomainMetainfo>
class UiDevToolsBaseAgent : public UiDevToolsAgent,
                            public DomainMetainfo::BackendClass {
 public:
  UiDevToolsBaseAgent(const UiDevToolsBaseAgent&) = delete;
  UiDevToolsBaseAgent& operator=(const UiDevToolsBaseAgent&) = delete;

  // UiDevToolsAgent:
  void Init(protocol::UberDispatcher* dispatcher) override {
    frontend_.reset(
        new typename DomainMetainfo::FrontendClass(dispatcher->channel()));
    DomainMetainfo::DispatcherClass::wire(dispatcher, this);
  }

  void Disable() override { disable(); }

  // Common methods between all generated Backends, subclasses may
  // choose to override them (but not necessary).
  protocol::Response enable() override { return protocol::Response::Success(); }

  protocol::Response disable() override {
    return protocol::Response::Success();
  }

 protected:
  UiDevToolsBaseAgent() = default;
  typename DomainMetainfo::FrontendClass* frontend() const {
    return frontend_.get();
  }

 private:
  std::unique_ptr<typename DomainMetainfo::FrontendClass> frontend_;
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_DEVTOOLS_BASE_AGENT_H_
