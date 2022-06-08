// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_POLICY_STATUS_PROVIDER_UPDATER_STATUS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_POLICY_STATUS_PROVIDER_UPDATER_STATUS_PROVIDER_H_

#include <string>

#include "components/policy/core/browser/webui/policy_status_provider.h"

struct GoogleUpdateState;

namespace base {
class DictionaryValue;
}  // namespace base

class UpdaterStatusProvider : public policy::PolicyStatusProvider {
 public:
  UpdaterStatusProvider();
  ~UpdaterStatusProvider() override;
  void SetUpdaterStatus(std::unique_ptr<GoogleUpdateState> status);
  void GetStatus(base::DictionaryValue* dict) override;

 private:
  static std::string FetchActiveDirectoryDomain();
  void OnDomainReceived(std::string domain);

  std::unique_ptr<GoogleUpdateState> updater_status_;
  std::string domain_;
  base::WeakPtrFactory<UpdaterStatusProvider> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_POLICY_STATUS_PROVIDER_UPDATER_STATUS_PROVIDER_H_
