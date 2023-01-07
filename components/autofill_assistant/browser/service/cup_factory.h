// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_CUP_FACTORY_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_CUP_FACTORY_H_

#include "components/autofill_assistant/browser/service/cup.h"

namespace autofill_assistant {

namespace cup {

// Base interface for creators of CUP (Client Update Protocol) instances.
class CUPFactory {
 public:
  virtual ~CUPFactory() = default;

  // Creates an instance of CUP for a call of given |rpc_type|.
  virtual std::unique_ptr<CUP> CreateInstance(RpcType rpc_type) const = 0;

 protected:
  CUPFactory() = default;
};

// Implementation of |CUPFactory| for |CUPImpl| instances.
class CUPImplFactory : public CUPFactory {
 public:
  CUPImplFactory() = default;
  ~CUPImplFactory() override = default;
  CUPImplFactory(const CUPImplFactory&) = delete;
  CUPImplFactory& operator=(const CUPImplFactory&) = delete;

  std::unique_ptr<CUP> CreateInstance(RpcType rpc_type) const override;
};

}  // namespace cup

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_CUP_FACTORY_H_
