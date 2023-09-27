// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_IMPL_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_IMPL_H_

#include "base/memory/raw_ref.h"
#include "components/compose/core/browser/compose_client.h"
#include "components/compose/core/browser/compose_manager.h"

namespace compose {

class ComposeManagerImpl : public ComposeManager {
 public:
  explicit ComposeManagerImpl(ComposeClient* client);
  ComposeManagerImpl(const ComposeManagerImpl&) = delete;
  ComposeManagerImpl& operator=(const ComposeManagerImpl&) = delete;
  ~ComposeManagerImpl() override;

  bool IsEnabled() const override;
  void OfferCompose(ComposeCallback callback) override;

 private:
  void ComposeTextForQuery(const ComposeClient::QueryParams& params);

  // A raw reference to the client, which owns `this` and therefore outlives it.
  const raw_ref<ComposeClient> client_;

  // A callback to Autofill that triggers filling the field.
  // TODO(b/301368162): Potentially make into a
  // `flat_map<FieldGlobalId, ComposeCallback>` to accommodate keeping
  // state for multiple input fields.
  ComposeCallback callback_;
};

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_IMPL_H_
