// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_BROWSER_APP_INSTANCE_UPDATE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_BROWSER_APP_INSTANCE_UPDATE_H_

#include "base/component_export.h"
#include "base/unguessable_token.h"

namespace apps {

struct COMPONENT_EXPORT(APP_TYPES) BrowserAppInstanceUpdate {
  enum class Type {
    kAppTab,
    kAppWindow,
  };

  BrowserAppInstanceUpdate();
  ~BrowserAppInstanceUpdate();
  BrowserAppInstanceUpdate(const BrowserAppInstanceUpdate&) = delete;
  BrowserAppInstanceUpdate& operator=(const BrowserAppInstanceUpdate&) = delete;
  BrowserAppInstanceUpdate(BrowserAppInstanceUpdate&&);
  BrowserAppInstanceUpdate& operator=(BrowserAppInstanceUpdate&&);

  base::UnguessableToken id;
  Type type;
  std::string app_id;
  std::string window_id;
  std::string title;
  bool is_browser_active = false;
  bool is_web_contents_active = false;
  uint32_t browser_session_id = 0;
  uint32_t restored_browser_session_id = 0;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_BROWSER_APP_INSTANCE_UPDATE_H_
