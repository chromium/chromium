// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_KEYED_SERVICE_H_
#define COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_KEYED_SERVICE_H_

#include <string>

#include "components/keyed_service/core/keyed_service.h"

namespace breadcrumbs {

// Logs breadcrumbs associated with a browser (BrowserState on iOS,
// BrowserContext on Desktop) - either incognito or normal.
class BreadcrumbManagerKeyedService : public KeyedService {
 public:
  explicit BreadcrumbManagerKeyedService(bool is_off_the_record);
  BreadcrumbManagerKeyedService(const BreadcrumbManagerKeyedService&) = delete;
  BreadcrumbManagerKeyedService& operator=(
      const BreadcrumbManagerKeyedService&) = delete;
  ~BreadcrumbManagerKeyedService() override;

  // Logs a breadcrumb |event| associated with the browser. Prepends the
  // |browsing_mode_| identifier to the event before passing it to the
  // |breadcrumb_manager_|.
  void AddEvent(const std::string& event);

 private:
  // A short string identifying the browser used to initialize the receiver. For
  // example, "I" for "I"ncognito browsing mode. This value is prepended to
  // events sent to |AddEvent| in order to differentiate the browser associated
  // with each event.
  // Note: Normal browsing mode uses an empty string in order to prevent
  // prepending most events with the same static value.
  std::string browsing_mode_;
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_KEYED_SERVICE_H_
