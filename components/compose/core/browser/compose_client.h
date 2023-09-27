// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_CLIENT_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_CLIENT_H_

#include <string>

#include "base/functional/callback.h"

namespace compose {

class ComposeManager;

// An interface for embedder actions, e.g. Chrome on Desktop.
class ComposeClient {
 public:
  // TODO(b/301609035): Add more parameters and potentially move to delegate or
  // service class.
  struct QueryParams {
    std::u16string query;
  };

  virtual ~ComposeClient() = default;

  // Returns the `ComposeManager` associated with this client.
  virtual ComposeManager& manager() = 0;

  using ComposeDialogCallback = base::OnceCallback<void(const QueryParams&)>;
  virtual void ShowComposeDialog(ComposeDialogCallback callback) = 0;
};

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_CLIENT_H_
