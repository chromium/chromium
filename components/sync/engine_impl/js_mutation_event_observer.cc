// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/js_mutation_event_observer.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/sync/js/js_event_details.h"
#include "components/sync/js/js_event_handler.h"

namespace syncer {

JsMutationEventObserver::JsMutationEventObserver() {}

JsMutationEventObserver::~JsMutationEventObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::WeakPtr<JsMutationEventObserver> JsMutationEventObserver::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void JsMutationEventObserver::InvalidateWeakPtrs() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void JsMutationEventObserver::SetJsEventHandler(
    const WeakHandle<JsEventHandler>& event_handler) {
  event_handler_ = event_handler;
}

namespace {

// Max number of changes we attempt to convert to values (to avoid
// running out of memory).
const size_t kChangeLimit = 100;

}  // namespace

void JsMutationEventObserver::OnChangesApplied(
    ModelType model_type,
    int64_t write_transaction_id,
    const ImmutableChangeRecordList& changes) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  base::DictionaryValue details;
  details.SetString("modelType", ModelTypeToString(model_type));
  details.SetString("writeTransactionId",
                    base::NumberToString(write_transaction_id));
  std::unique_ptr<base::Value> changes_value;
  const size_t changes_size = changes.Get().size();
  if (changes_size <= kChangeLimit) {
    auto changes_list = std::make_unique<base::ListValue>();
    for (auto it = changes.Get().begin(); it != changes.Get().end(); ++it) {
      changes_list->Append(it->ToValue());
    }
    changes_value = std::move(changes_list);
  } else {
    changes_value = std::make_unique<base::Value>(
        base::NumberToString(changes_size) + " changes");
  }
  details.Set("changes", std::move(changes_value));
  HandleJsEvent(FROM_HERE, "onChangesApplied", JsEventDetails(&details));
}

void JsMutationEventObserver::OnChangesComplete(ModelType model_type) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  base::DictionaryValue details;
  details.SetString("modelType", ModelTypeToString(model_type));
  HandleJsEvent(FROM_HERE, "onChangesComplete", JsEventDetails(&details));
}

void JsMutationEventObserver::OnTransactionWrite(
    const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
    ModelTypeSet models_with_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!event_handler_.IsInitialized()) {
    return;
  }
  base::DictionaryValue details;
  details.Set("writeTransactionInfo",
              write_transaction_info.Get().ToValue(kChangeLimit));
  details.Set("modelsWithChanges", ModelTypeSetToValue(models_with_changes));
  HandleJsEvent(FROM_HERE, "onTransactionWrite", JsEventDetails(&details));
}

void JsMutationEventObserver::HandleJsEvent(const base::Location& from_here,
                                            const std::string& name,
                                            const JsEventDetails& details) {
  if (!event_handler_.IsInitialized()) {
    NOTREACHED();
    return;
  }
  event_handler_.Call(from_here, &JsEventHandler::HandleJsEvent, name, details);
}

}  // namespace syncer
