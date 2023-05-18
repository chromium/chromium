// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_AGGREGATOR_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_AGGREGATOR_H_

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"

namespace device_signals {

struct SignalsAggregationRequest;
struct SignalsAggregationResponse;
struct UserContext;

class SignalsAggregator : public KeyedService {
 public:
  using GetSignalsCallback =
      base::OnceCallback<void(SignalsAggregationResponse)>;

  ~SignalsAggregator() override = default;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Will asynchronously collect signals whose names are specified in the
  // `request` object, and will also use a `user_context` to validate that the
  // user has permissions to the device's signals. Invokes `callback` with the
  // collected signals. Currently only supports the collection of one signal
  // (only one entry in `request.signal_names`) for simplicity as no current use
  // case requires supporting the collection of multiple signals in one request.
  virtual void GetSignalsForUser(const UserContext& user_context,
                                 const SignalsAggregationRequest& request,
                                 GetSignalsCallback callback) = 0;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  // Will asynchronously collect signals whose names are specified in the
  // `request` object. Uses the current context (browser management and current
  // user) to evaluation signal collection permissions. Invokes `callback` with
  // the collected signals. Currently only supports the collection of one signal
  // (only one entry in `request.signal_names`) for simplicity as no current use
  // case requires supporting the collection of multiple signals in one request.
  virtual void GetSignals(const SignalsAggregationRequest& request,
                          GetSignalsCallback callback) = 0;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_AGGREGATOR_H_
