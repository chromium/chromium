// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/supports_handles.h"

#include "base/types/pass_key.h"

namespace tabs::internal {

SupportsHandlesBase::SupportsHandlesBase(HandleFactory& handle_factory)
    : handle_factory_(handle_factory),
      handle_value_(
          handle_factory.AssignHandleValue(base::PassKey<SupportsHandlesBase>(),
                                           this)) {}
SupportsHandlesBase::~SupportsHandlesBase() {
  handle_factory_->FreeHandleValue(base::PassKey<SupportsHandlesBase>(),
                                   handle_value_);
}

SupportsHandlesBase* SupportsHandlesBase::LookupHandle(HandleFactory& factory,
                                                       int32_t handle_value) {
  return factory.LookupObject(base::PassKey<SupportsHandlesBase>(),
                              handle_value);
}

HandleFactory::HandleFactory() = default;
HandleFactory::~HandleFactory() = default;

int32_t HandleFactory::AssignHandleValue(base::PassKey<SupportsHandlesBase>,
                                         StoredPointerType object) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  CHECK(object);

  // Use the next available handle value; it is an error if the value rolls
  // back over to zero.
  ++last_handle_value_;
  CHECK(last_handle_value_)
      << "Fatal handle reuse! Please curtail object creation.";

  lookup_table_.emplace(last_handle_value_, object);
  return last_handle_value_;
}

void HandleFactory::FreeHandleValue(base::PassKey<SupportsHandlesBase>,
                                    int32_t handle_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  CHECK(lookup_table_.erase(handle_value));

  OnHandleFreed(handle_value);
}

HandleFactory::StoredPointerType HandleFactory::LookupObject(
    base::PassKey<SupportsHandlesBase>,
    int32_t handle_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  const auto it = lookup_table_.find(handle_value);
  return it != lookup_table_.end() ? it->second : nullptr;
}

}  // namespace tabs::internal
