// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CONTENT_BROWSER_RULES_SERVICE_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CONTENT_BROWSER_RULES_SERVICE_H_

#include "components/enterprise/data_controls/core/browser/rule.h"
#include "components/enterprise/data_controls/core/browser/verdict.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/clipboard_types.h"

namespace data_controls {

// Abstract keyed service that provides an interface to check what restrictions
// should be applied from the DataControlsRules policy.
class RulesService : public KeyedService {
 public:
  ~RulesService() override;

  virtual Verdict GetPrintVerdict(const GURL& printed_page_url) const = 0;

  // Returns a clipboard verdict to be applied to a paste action. A null browser
  // context on `source` represents data coming from the OS clipboard.
  // `destination` is always expected to have a valid browser context.
  virtual Verdict GetPasteVerdict(
      const content::ClipboardEndpoint& source,
      const content::ClipboardEndpoint& destination,
      const content::ClipboardMetadata& metadata) const = 0;

  // Returns a clipboard verdict only based the source of the copy, without
  // making any special destination assumptions. This is meant to trigger rules
  // that only have "sources" conditions, and blocking/warning verdicts returned
  // by this function should trigger a dialog.
  virtual Verdict GetCopyRestrictedBySourceVerdict(
      const GURL& source) const = 0;

  // Returns a clipboard verdict with the provided source attributes, and with
  // the "os_clipboard" destination. This is meant to trigger rules that make
  // use of the "os_clipboard" destination attribute. Blocking verdicts returned
  // by this function should replace the data put in the clipboard, and warning
  // verdicts should trigger a dialog.
  virtual Verdict GetCopyToOSClipboardVerdict(const GURL& source) const = 0;

  // Returns true if rules indicate screenshots should be blocked. Only the
  // "block" level is supported, a "warn" screenshot rule will not make this
  // functions return true.
  virtual bool BlockScreenshots(const GURL& url) const = 0;

 protected:
  explicit RulesService(PrefService* pref_service);

  // Returns a `Verdict` corresponding to all triggered Data Control rules given
  // the provided context.
  Verdict GetVerdict(Rule::Restriction restriction,
                     const ActionContext& context) const;

 private:
  // Parse the "DataControlsRules" policy if the corresponding experiment is
  // enabled, and populate `rules_`.
  void OnDataControlsRulesUpdate();

  // Watches changes to the "DataControlsRules" policy. Does nothing if the
  // "EnableDesktopDataControls" experiment is disabled.
  PrefChangeRegistrar pref_registrar_;

  // List of rules created from the "DataControlsRules" policy. Empty if the
  // "EnableDesktopDataControls" experiment is disabled.
  std::vector<Rule> rules_;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CONTENT_BROWSER_RULES_SERVICE_H_
