// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MANAGEMENT_DISCLOSURE_MANAGEMENT_DISCLOSURE_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_MANAGEMENT_DISCLOSURE_MANAGEMENT_DISCLOSURE_CLIENT_IMPL_H_

#include <unordered_map>

#include "ash/public/cpp/management_disclosure_client.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"

// Handles showing the management disclosure calls from ash to chrome.
class ManagementDisclosureClientImpl : public ash::ManagementDisclosureClient {
 public:
  ManagementDisclosureClientImpl(policy::BrowserPolicyConnectorAsh* connector,
                                 Profile* profile);

  ManagementDisclosureClientImpl(const ManagementDisclosureClientImpl&) =
      delete;
  ManagementDisclosureClientImpl& operator=(
      const ManagementDisclosureClientImpl&) = delete;

  ~ManagementDisclosureClientImpl() override;

  std::vector<std::u16string> GetDisclosures() override;

 private:
  raw_ptr<policy::BrowserPolicyConnectorAsh> connector_;
  raw_ptr<Profile> profile_;
  std::unordered_map<std::string, int> policy_map_;

  base::WeakPtrFactory<ManagementDisclosureClientImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_MANAGEMENT_DISCLOSURE_MANAGEMENT_DISCLOSURE_CLIENT_IMPL_H_
