// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/firewall_manager_win.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "chrome/installer/util/advanced_firewall_manager_win.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"

namespace installer {

namespace {

const uint16_t kDefaultMdnsPort = 5353;

class FirewallManagerAdvancedImpl : public FirewallManager {
 public:
  FirewallManagerAdvancedImpl() {}

  FirewallManagerAdvancedImpl(const FirewallManagerAdvancedImpl&) = delete;
  FirewallManagerAdvancedImpl& operator=(const FirewallManagerAdvancedImpl&) =
      delete;

  ~FirewallManagerAdvancedImpl() override {}

  bool Init(const std::wstring& app_name, const base::FilePath& app_path) {
    return manager_.Init(app_name, app_path);
  }

  // FirewallManager methods.
  bool CanUseLocalPorts() override {
    return !manager_.IsFirewallEnabled() || manager_.HasAnyRule();
  }

  bool AddFirewallRules() override {
    return manager_.AddUDPRule(GetMdnsRuleName(), GetMdnsRuleDescription(),
                               kDefaultMdnsPort);
  }

  void RemoveFirewallRules() override { manager_.DeleteAllRules(); }

 private:
  static std::wstring GetMdnsRuleName() {
    return GetLocalizedString(IDS_INBOUND_MDNS_RULE_NAME_BASE);
  }

  static std::wstring GetMdnsRuleDescription() {
    return GetLocalizedString(IDS_INBOUND_MDNS_RULE_DESCRIPTION_BASE);
  }

  AdvancedFirewallManager manager_;
};

}  // namespace

FirewallManager::~FirewallManager() {}

// static
std::unique_ptr<FirewallManager> FirewallManager::Create(
    const base::FilePath& chrome_path) {
  // Try to connect to "Windows Firewall with Advanced Security" (Vista+).
  auto manager = std::make_unique<FirewallManagerAdvancedImpl>();
  if (manager->Init(InstallUtil::GetDisplayName(), chrome_path))
    return std::move(manager);

  return nullptr;
}

FirewallManager::FirewallManager() {}

}  // namespace installer
