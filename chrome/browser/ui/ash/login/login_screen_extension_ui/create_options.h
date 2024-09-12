// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_SCREEN_EXTENSION_UI_CREATE_OPTIONS_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_SCREEN_EXTENSION_UI_CREATE_OPTIONS_H_

#include <string>

#include "base/functional/callback.h"
#include "url/gurl.h"

namespace ash {
namespace login_screen_extension_ui {

struct CreateOptions {
  CreateOptions(const std::string& extension_name,
                const GURL& content_url,
                bool can_be_closed_by_user,
                base::OnceClosure close_callback);

  CreateOptions(const CreateOptions&) = delete;
  CreateOptions& operator=(const CreateOptions&) = delete;

  ~CreateOptions();

  const std::string extension_name;
  const GURL content_url;
  bool can_be_closed_by_user;
  base::OnceClosure close_callback;
};

}  // namespace login_screen_extension_ui
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_SCREEN_EXTENSION_UI_CREATE_OPTIONS_H_
