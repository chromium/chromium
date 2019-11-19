// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOMAIN_RELIABILITY_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DOMAIN_RELIABILITY_INTERNALS_UI_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_controller.h"

namespace base {
class ListValue;
class Value;
}  // namespace base

// The WebUI for chrome://domain-reliability-internals
class DomainReliabilityInternalsUI : public content::WebUIController {
 public:
  explicit DomainReliabilityInternalsUI(content::WebUI* web_ui);
  ~DomainReliabilityInternalsUI() override;

 private:
  void UpdateData(const base::ListValue* args);
  void OnDataUpdated(base::Value data) const;

  base::WeakPtrFactory<DomainReliabilityInternalsUI> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DomainReliabilityInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOMAIN_RELIABILITY_INTERNALS_UI_H_
