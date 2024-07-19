// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_ACTION_PERFORMER_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_ACTION_PERFORMER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/values.h"

namespace growth {

// The different actions that the Growth framework can run.
// These values are deserialized from Growth Campaign, so entries should not
// be renumbered and numeric values should never be reused
enum class ActionType {
  // This is a special action that handled by surfaces like Nudge which has
  // different implementation of dismissal (instead of action performers that
  // are used by different surfaces).
  kDismiss = 0,

  kInstallWebApp = 1,
  kPinWebApp = 2,
  kOpenUrl = 3,
  kShowNudge = 4,
  kShowNotification = 5,
  kUpdateUserPref = 6,

  kMaxValue = kUpdateUserPref
};

enum class ActionResult {
  kSuccess = 0,
  kFailure = 1,
};

enum class ActionResultReason {
  kUnknown = 0,
  kParsingActionFailed = 1,

  // For kInstallWebApp action
  kWebAppProviderNotAvailable = 2,
  kWebAppInstallFailedOther = 3,

  // For kUpdateUserPref action
  kUpdateUserPrefFailed = 4,
};

// Abstract interface for the different actions that Growth framework
// can make.
class ActionPerformer {
 public:
  using Callback =
      base::OnceCallback<void(ActionResult, std::optional<ActionResultReason>)>;
  ActionPerformer() = default;
  ActionPerformer(const ActionPerformer&) = delete;
  ActionPerformer& operator=(const ActionPerformer&) = delete;
  virtual ~ActionPerformer() = default;

  virtual void Run(int campaign_id,
                   std::optional<int> group_id,
                   const base::Value::Dict* action_params,
                   Callback callback) = 0;

  // Returns what type of action the subclass can run.
  virtual ActionType ActionType() const = 0;
};

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_ACTION_PERFORMER_H_
