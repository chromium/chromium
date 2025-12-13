// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_BROWSER_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_BROWSER_DELEGATE_H_

#include "base/component_export.h"
#include "base/one_shot_event.h"
#include "base/types/expected.h"
#include "url/gurl.h"

namespace ash::assistant {

// Main interface implemented in browser to provide dependencies to
// |ash::assistant::Service|.
class COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) AssistantBrowserDelegate {
 public:
  enum class Error {
    kProfileNotReady,
    kWebAppProviderNotReadyToRead,
    kNewEntryPointNotEnabled,
    kNewEntryPointNotFound,
    kNonGoogleChromeBuild,
  };

  AssistantBrowserDelegate();
  AssistantBrowserDelegate(const AssistantBrowserDelegate&) = delete;
  AssistantBrowserDelegate& operator=(const AssistantBrowserDelegate&) = delete;
  virtual ~AssistantBrowserDelegate();

  static AssistantBrowserDelegate* Get();

  // Opens the specified `url` in a new browser tab. Special handling is applied
  // to OS Settings url which may cause deviation from this behavior.
  virtual void OpenUrl(GURL url) = 0;

  // Returns true if a primary profile is eligible for Assistant new entry
  // point. Note that an error might be returned if you read this value before
  // `is_new_entry_point_eligible_for_primary_profile_ready` event is signaled.
  virtual base::expected<bool, Error>
  IsNewEntryPointEligibleForPrimaryProfile() = 0;

  // An event signaled if the new entry point eligibility value is ready to
  // read. There is initialization happening as an async operation. Early read
  // of `IsNewEntryPointEligibleForPrimaryProfile` can return an error value.
  // TODO(crbug.com/386257055): update API as this supports eligibility change
  // events, e.g., uninstall.
  const base::OneShotEvent&
  is_new_entry_point_eligible_for_primary_profile_ready() {
    return on_is_new_entry_point_eligible_ready_;
  }

  // Opens the new entry point.
  virtual void OpenNewEntryPoint() = 0;

  // Returns name of the new entry point. `std::nullopt` is returned for any
  // error cases, e.g., the new entry point is not installed.
  virtual std::optional<std::string> GetNewEntryPointName() = 0;

 protected:
  base::OneShotEvent on_is_new_entry_point_eligible_ready_;
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_BROWSER_DELEGATE_H_
