// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_SERVICE_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace personal_context {

// The PersonalContextService manages the personal context of a profile and acts
// as a bridge to handle requests to the Personal Context server.
class PersonalContextService : public KeyedService {
 public:
  ~PersonalContextService() override = default;
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_SERVICE_H_
