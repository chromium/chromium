// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/feature_promo_controller.h"

#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"

namespace user_education {

namespace {

// Implementation for test callback list.
using TestResultCallbackList =
    base::RepeatingCallbackList<void(const base::Feature&, FeaturePromoResult)>;
TestResultCallbackList& GetTestResultCallbackList() {
  static base::NoDestructor<TestResultCallbackList> instance;
  return *instance.get();
}

}  // namespace

FeaturePromoController::FeaturePromoController() = default;
FeaturePromoController::~FeaturePromoController() = default;

void FeaturePromoController::PostShowPromoResult(
    const base::Feature& feature,
    ShowPromoResultCallback callback,
    FeaturePromoResult result) {
  GetTestResultCallbackList().Notify(feature, result);
  if (callback) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](FeaturePromoController::ShowPromoResultCallback callback,
               FeaturePromoResult result) { std::move(callback).Run(result); },
            std::move(callback), result));
  }
}

// static
base::CallbackListSubscription
FeaturePromoController::AddResultCallbackForTesting(  // IN-TEST
    TestResultCallback callback) {
  return GetTestResultCallbackList().Add(std::move(callback));
}

FeaturePromoParams::FeaturePromoParams(const base::Feature& iph_feature,
                                       const std::string& promo_key)
    : feature(iph_feature), key(promo_key) {}
FeaturePromoParams::FeaturePromoParams(FeaturePromoParams&& other) noexcept =
    default;
FeaturePromoParams& FeaturePromoParams::operator=(
    FeaturePromoParams&& other) noexcept = default;
FeaturePromoParams::~FeaturePromoParams() = default;

std::ostream& operator<<(std::ostream& os, FeaturePromoStatus status) {
  switch (status) {
    case FeaturePromoStatus::kBubbleShowing:
      os << "kBubbleShowing";
      break;
    case FeaturePromoStatus::kContinued:
      os << "kContinued";
      break;
    case FeaturePromoStatus::kNotRunning:
      os << "kNotRunning";
      break;
    case FeaturePromoStatus::kQueued:
      os << "kQueued";
      break;
  }
  return os;
}

}  // namespace user_education
