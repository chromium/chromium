// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_EXO_APP_TYPE_RESOLVER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_EXO_APP_TYPE_RESOLVER_H_

#include "ash/components/arc/video_accelerator/protected_native_pixmap_query_client.h"
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

 private:
  arc::ProtectedNativePixmapQueryClient protected_native_pixmap_query_client_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_EXO_APP_TYPE_RESOLVER_H_
