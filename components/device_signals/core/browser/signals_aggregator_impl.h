// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_AGGREGATOR_IMPL_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_AGGREGATOR_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/device_signals/core/browser/signals_aggregator.h"

namespace device_signals {

class SignalsCollector;
class UserPermissionService;
enum class UserPermission;

class SignalsAggregatorImpl : public SignalsAggregator {
 public:
  explicit SignalsAggregatorImpl(
      UserPermissionService* permission_service,
      std::vector<std::unique_ptr<SignalsCollector>> collectors);

  SignalsAggregatorImpl(const SignalsAggregatorImpl&) = delete;
  SignalsAggregatorImpl& operator=(const SignalsAggregatorImpl&) = delete;

  ~SignalsAggregatorImpl() override;

  // SignalsAggregator:
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  void GetSignalsForUser(const UserContext& user_context,
                         const SignalsAggregationRequest& request,
                         GetSignalsCallback callback) override;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  void GetSignals(const SignalsAggregationRequest& request,
                  GetSignalsCallback callback) override;

 private:
  void GetSignalsWithPermission(const UserPermission user_permission,
                                const SignalsAggregationRequest& request,
                                GetSignalsCallback callback);

  raw_ptr<UserPermissionService> permission_service_;
  std::vector<std::unique_ptr<SignalsCollector>> collectors_;

  base::WeakPtrFactory<SignalsAggregatorImpl> weak_factory_{this};
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_AGGREGATOR_IMPL_H_
