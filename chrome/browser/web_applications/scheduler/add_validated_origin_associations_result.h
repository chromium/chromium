// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_ADD_VALIDATED_ORIGIN_ASSOCIATIONS_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_ADD_VALIDATED_ORIGIN_ASSOCIATIONS_RESULT_H_

namespace web_app {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AddValidatedOriginAssociationsResult)
enum class AddValidatedOriginAssociationsResult {
  // Origin associations were fetched succcesfully, validated and stored in
  // web_app.
  kSuccess = 0,

  // Removed, don't reuse this value.
  // kNotNeeded = 1,

  // The validation command was throttled (rate-limited). The caller needs to
  // wait for more time before revalidation can happen.
  kThrottled = 2,
  // Web App was not found with specified app id.
  kWebAppNotInstalled = 3,
  // Origin association validation has completed, but not all unvalidated
  // association origin data was validated. It is possible that data was
  // validated partially
  // and added to the current web app validated items.
  kUnvalidatedItemsRemain = 4,
  // The command was shutdown and the execution did not finish.
  kShutdown = 5,
  kMaxValue = kShutdown
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:AddValidatedOriginAssociationsResult)

}  //  namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_ADD_VALIDATED_ORIGIN_ASSOCIATIONS_RESULT_H_
