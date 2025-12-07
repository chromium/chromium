// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_RULES_SERVICE_BASE_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_RULES_SERVICE_BASE_H_

#include "components/enterprise/data_controls/core/browser/rule.h"
#include "components/enterprise/data_controls/core/browser/verdict.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace data_controls {

// Platform-agnostic keyed service that provides an interface to check what
// restrictions should be applied from the DataControlsRules policy, as well as
// internal logic to track updates made to that policy.
class RulesServiceBase : public KeyedService {
 public:
  explicit RulesServiceBase(PrefService* pref_service);
  ~RulesServiceBase() override;

  // Returns a clipboard verdict only based the source of the copy, without
  // making any special destination assumptions. This is meant to trigger rules
  // that only have "sources" conditions, and blocking/warning verdicts returned
  // by this function should trigger a dialog.
  virtual Verdict GetCopyRestrictedBySourceVerdict(const GURL& source) const;

  // Returns a clipboard verdict with the provided source attributes, and with
  // the "os_clipboard" destination. This is meant to trigger rules that make
  // use of the "os_clipboard" destination attribute. Blocking verdicts returned
  // by this function should replace the data put in the clipboard, and warning
  // verdicts should trigger a dialog.
  virtual Verdict GetCopyToOSClipboardVerdict(const GURL& source) const;

  // Returns a verdict to be applied to a specific file download.
  virtual Verdict GetDownloadVerdict(const GURL& download_url) const;

 protected:
  // Returns a `Verdict` corresponding to all triggered Data Control rules given
  // the provided context.
  Verdict GetVerdict(Rule::Restriction restriction,
                     const ActionContext& context) const;

 private:
  // Returns whether the profile associated with the service is an incognito one
  // or not.
  virtual bool incognito_profile() const = 0;

  // Returns if the "DataControlsRules" policy for the current service is set at
  // the machine scope or not.
  bool MachineScopePolicy() const;

  // Parse the "DataControlsRules" policy if the corresponding experiment is
  // enabled, and populate `rules_`.
  void OnDataControlsRulesUpdate();

  // Watches changes to the "DataControlsRules" policy.
  PrefChangeRegistrar pref_registrar_;

  // List of rules created from the "DataControlsRules" policy.
  std::vector<Rule> rules_;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_RULES_SERVICE_BASE_H_
