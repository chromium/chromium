// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_HISTORY_HISTORY_SERVICE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_HISTORY_HISTORY_SERVICE_PROVIDER_H_

#include "base/component_export.h"

class AccountId;

namespace history {
class HistoryService;
}  // namespace history

namespace ash {

// Provides the history::HistoryService associated with a user to ChromeOS
// callers without forcing them to depend on //chrome/browser/history's
// Profile-keyed factory. The concrete implementation lives in //chrome (see
// //chrome/browser/ash/browser_delegate/keyed_service_provider/
// history_service_provider_impl.h).
class COMPONENT_EXPORT(HISTORY_SERVICE_PROVIDER) HistoryServiceProvider {
 public:
  HistoryServiceProvider();
  HistoryServiceProvider(const HistoryServiceProvider&) = delete;
  HistoryServiceProvider& operator=(const HistoryServiceProvider&) = delete;
  virtual ~HistoryServiceProvider();

  // Returns the process-wide singleton.
  static HistoryServiceProvider& Get();

  // Returns the HistoryService associated with `account_id` (with
  // EXPLICIT_ACCESS), or nullptr if none is available. The returned pointer is
  // owned by the BrowserContext-keyed service infrastructure; callers must not
  // delete it.
  virtual history::HistoryService* Find(const AccountId& account_id) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_HISTORY_HISTORY_SERVICE_PROVIDER_H_
