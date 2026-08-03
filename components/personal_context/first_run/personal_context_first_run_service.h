// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_H_
#define COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"

namespace personal_context {

// Service that manages the First Feature Run for the Personal Context.
// It provides methods to trigger the first run flow.
// Clients can observe changes to the enablement state.
class PersonalContextFirstRunService : public KeyedService {
 public:
  ~PersonalContextFirstRunService() override = default;

  // Called when the user has acknowledged the Personal Context notice in
  // Autofill.
  // TODO(b:517579158): Wire the notice UIs into this function.
  virtual void MarkPersonalContextAmbientAutofillNoticeAsAcknowledged() = 0;

  // Returns true if the Personal Context notice should be shown in Autofill.
  // TODO(b:517579158): Wire the notice UIs into this function.
  virtual bool ShouldShowPersonalContextAmbientAutofillNotice() const = 0;

  // Records an impression for the Ambient Autofill notice.
  virtual void RecordAmbientAutofillNoticeImpression(uint32_t session_id) = 0;

  // Called when the user has acknowledged the Personal Context notice in
  // At Memory.
  virtual void MarkPersonalContextInAtMemoryNoticeAsAcknowledged() = 0;

  // Returns true if the Personal Context notice should be shown in At Memory.
  virtual bool ShouldShowPersonalContextAtMemoryNotice() const = 0;

  // Records an impression for the AtMemory notice.
  virtual void RecordAtMemoryNoticeImpression(uint32_t session_id) = 0;
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_H_
