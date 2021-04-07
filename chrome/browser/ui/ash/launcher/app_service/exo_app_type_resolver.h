// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_EXO_APP_TYPE_RESOLVER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_EXO_APP_TYPE_RESOLVER_H_

#include "components/exo/wm_helper.h"

// This class populates the window property to identify the type of application
// for exo's toplevel window based on |app_id| and |startup_id|.
class ExoAppTypeResolver : public exo::WMHelper::AppPropertyResolver {
 public:
  ExoAppTypeResolver() = default;
  ExoAppTypeResolver(const ExoAppTypeResolver&) = delete;
  ExoAppTypeResolver& operator=(const ExoAppTypeResolver&) = delete;
  ~ExoAppTypeResolver() override = default;

  // exo::WMHelper::AppPropertyResolver:
  void PopulateProperties(
      const Params& params,
      ui::PropertyHandler& out_properties_container) override;
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_EXO_APP_TYPE_RESOLVER_H_
