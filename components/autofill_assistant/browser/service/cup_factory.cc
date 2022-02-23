// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/cup_factory.h"
#include "components/autofill_assistant/browser/service/cup_impl.h"

namespace autofill_assistant {

namespace cup {

std::unique_ptr<CUP> CUPImplFactory::CreateInstance(RpcType rpc_type) const {
  return std::make_unique<CUPImpl>(CUPImpl::CreateQuerySigner(), rpc_type);
}

}  // namespace cup

}  // namespace autofill_assistant
