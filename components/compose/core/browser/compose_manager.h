// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_H_

#include <string>

#include "base/functional/callback.h"

namespace compose {

// The interface for embedder-independent, tab-specific compose logic.
class ComposeManager {
 public:
  using ComposeCallback = base::OnceCallback<void(const std::u16string&)>;

  virtual ~ComposeManager() = default;

  virtual bool IsEnabled() const = 0;
  virtual void OfferCompose(ComposeCallback callback) = 0;
};

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_H_
