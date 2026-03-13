// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_TOKEN_BASED_CONTEXT_DECORATOR_H_
#define COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_TOKEN_BASED_CONTEXT_DECORATOR_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "components/contextual_tasks/public/context_decorator.h"

namespace contextual_search {
class ContextualSearchContextController;
class ContextualSearchSessionHandle;
}  // namespace contextual_search

namespace contextual_tasks {

class TokenBasedContextDecorator : public ContextDecorator {
 public:
  // ContextDecorator:
  void DecorateContext(
      std::unique_ptr<ContextualTaskContext> context,
      ContextDecorationParams* params,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          context_callback) override;

 protected:
  virtual std::vector<base::UnguessableToken> GetTokensToDecorate(
      contextual_search::ContextualSearchSessionHandle* handle) const = 0;

  void DecorateContextWithTokens(
      std::unique_ptr<ContextualTaskContext> context,
      contextual_search::ContextualSearchContextController* controller,
      const std::vector<base::UnguessableToken>& tokens,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          context_callback);
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_TOKEN_BASED_CONTEXT_DECORATOR_H_
