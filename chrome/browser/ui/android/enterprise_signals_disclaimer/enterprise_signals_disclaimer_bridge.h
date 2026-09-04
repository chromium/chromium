// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_ENTERPRISE_SIGNALS_DISCLAIMER_ENTERPRISE_SIGNALS_DISCLAIMER_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_ENTERPRISE_SIGNALS_DISCLAIMER_ENTERPRISE_SIGNALS_DISCLAIMER_BRIDGE_H_

namespace enterprise_signals {

class EnterpriseSignalsDisclaimerBridge {
 public:
  EnterpriseSignalsDisclaimerBridge() = delete;
  EnterpriseSignalsDisclaimerBridge(const EnterpriseSignalsDisclaimerBridge&) =
      delete;
  EnterpriseSignalsDisclaimerBridge& operator=(
      const EnterpriseSignalsDisclaimerBridge&) = delete;
};

}  // namespace enterprise_signals

#endif  // CHROME_BROWSER_UI_ANDROID_ENTERPRISE_SIGNALS_DISCLAIMER_ENTERPRISE_SIGNALS_DISCLAIMER_BRIDGE_H_
