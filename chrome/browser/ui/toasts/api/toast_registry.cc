// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/api/toast_registry.h"

#include <memory>

#include "base/check.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_specification.h"

ToastRegistry::ToastRegistry() = default;
ToastRegistry::~ToastRegistry() = default;

void ToastRegistry::RegisterToast(
    ToastId id,
    std::unique_ptr<ToastSpecification> specification) {
  CHECK(specification);
  CHECK(!toast_specifications_.contains(id));
  toast_specifications_[id] = std::move(specification);
}

const ToastSpecification* ToastRegistry::GetToastSpecification(
    ToastId id) const {
  auto iter = toast_specifications_.find(id);
  CHECK(iter != toast_specifications_.end())
      << "Unable to find id " << static_cast<int>(id)
      << " in list of toasts size " << toast_specifications_.size();
  return iter->second.get();
}

bool ToastRegistry::IsEmpty() const {
  return toast_specifications_.empty();
}
