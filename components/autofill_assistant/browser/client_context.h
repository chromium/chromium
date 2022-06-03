// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_CONTEXT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_CONTEXT_H_

#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_context.h"

namespace autofill_assistant {

// Provides a ClientContextProto and a way to update its values dynamically.
class ClientContext {
 public:
  virtual ~ClientContext() = default;
  // Updates the client context based on the current state of the client.
  virtual void Update(const TriggerContext& trigger_context) = 0;
  // Returns the proto representation of this client context.
  virtual ClientContextProto AsProto() const = 0;
};

// Represents the client context for a given |client| instance.
class ClientContextImpl : public ClientContext {
 public:
  // |client| must outlive this instance.
  ClientContextImpl(const Client* client);
  ~ClientContextImpl() override = default;
  void Update(const TriggerContext& trigger_context) override;
  ClientContextProto AsProto() const override;

 private:
  const Client* client_;
  ClientContextProto proto_;
};

// An empty client context that does not contain any data.
class EmptyClientContext : public ClientContext {
 public:
  EmptyClientContext() = default;
  ~EmptyClientContext() override = default;
  void Update(const TriggerContext& trigger_context) override {}
  ClientContextProto AsProto() const override;
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_CONTEXT_H_
