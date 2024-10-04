// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_UI_SELECTOR_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_UI_SELECTOR_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_uma_util.h"

namespace permissions {

// The interface for implementations that decide if the quiet prompt UI should
// be used to display a permission |request|, whether a warning should be
// printed to the Dev Tools console, and the reasons for both.
//
// Implementations of interface are expected to have long-lived instances that
// can support multiple requests, but only one at a time.
class PermissionUiSelector {
 public:
  enum class QuietUiReason {
    kEnabledInPrefs,
    kTriggeredByCrowdDeny,
    kTriggeredDueToAbusiveRequests,
    kTriggeredDueToAbusiveContent,
    kServicePredictedVeryUnlikelyGrant,
    kOnDevicePredictedVeryUnlikelyGrant,
    kTriggeredDueToDisruptiveBehavior,
  };

  enum class WarningReason {
    kAbusiveRequests,
    kAbusiveContent,
    kDisruptiveBehavior,
  };

  struct Decision {
    Decision(std::optional<QuietUiReason> quiet_ui_reason,
             std::optional<WarningReason> warning_reason);
    ~Decision();

    Decision(const Decision&);
    Decision& operator=(const Decision&);

    static constexpr std::optional<QuietUiReason> UseNormalUi() {
      return std::nullopt;
    }

    static constexpr std::optional<WarningReason> ShowNoWarning() {
      return std::nullopt;
    }

    static Decision UseNormalUiAndShowNoWarning();

    // The reason for showing the quiet UI, or `std::nullopt` if the normal UI
    // should be used.
    std::optional<QuietUiReason> quiet_ui_reason;

    // The reason for printing a warning to the console, or `std::nullopt` if
    // no warning should be printed.
    std::optional<WarningReason> warning_reason;
  };

  using DecisionMadeCallback = base::OnceCallback<void(const Decision&)>;

  virtual ~PermissionUiSelector() = default;

  // Determines whether animations should be suppressed because we're very
  // confident the user does not want notifications (e.g. they're abusive).
  static bool ShouldSuppressAnimation(std::optional<QuietUiReason> reason);

  // Determines the UI to use for the given |request|, and invokes |callback|
  // when done, either synchronously or asynchronously. The |callback| is
  // guaranteed never to be invoked after |this| goes out of scope. Only one
  // request is supported at a time.
  virtual void SelectUiToUse(PermissionRequest* request,
                             DecisionMadeCallback callback) = 0;

  // Cancel the pending request, if any. After this, the |callback| is
  // guaranteed not to be invoked anymore, and another call to SelectUiToUse()
  // can be issued. Can be called when there is no pending request which will
  // simply be a no-op.
  virtual void Cancel() {}

  virtual bool IsPermissionRequestSupported(RequestType request_type) = 0;

  // Will return the selector's discretized prediction value, if any is
  // applicable to be recorded in UKMs. This is specific only to a selector that
  // makes use of the Web Permission Predictions Service to make decisions.
  virtual std::optional<PermissionUmaUtil::PredictionGrantLikelihood>
  PredictedGrantLikelihoodForUKM();

  // Will return if the selector's decision was heldback. Currently only the
  // Web Prediction Service selector supports holdbacks.
  virtual std::optional<bool> WasSelectorDecisionHeldback();
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_UI_SELECTOR_H_
