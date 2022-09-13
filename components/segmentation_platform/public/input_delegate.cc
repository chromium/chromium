// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/input_delegate.h"

namespace segmentation_platform::processing {

InputDelegate::InputDelegate() = default;
InputDelegate::~InputDelegate() = default;

InputDelegateHolder::InputDelegateHolder() = default;
InputDelegateHolder::~InputDelegateHolder() = default;

InputDelegate* InputDelegateHolder::GetDelegate(
    proto::CustomInput::FillPolicy policy) {
  auto it = input_delegates_.find(policy);
  if (it != input_delegates_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void InputDelegateHolder::SetDelegate(proto::CustomInput::FillPolicy policy,
                                      std::unique_ptr<InputDelegate> delegate) {
  DCHECK(!input_delegates_.count(policy));
  input_delegates_[policy] = std::move(delegate);
}

}  // namespace segmentation_platform::processing
