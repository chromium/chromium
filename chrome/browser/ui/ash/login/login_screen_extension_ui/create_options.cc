// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/login_screen_extension_ui/create_options.h"

#include <memory>

namespace ash {
namespace login_screen_extension_ui {

CreateOptions::CreateOptions(const std::string& extension_name,
                             const GURL& content_url,
                             bool can_be_closed_by_user,
                             base::OnceClosure close_callback)
    : extension_name(extension_name),
      content_url(content_url),
      can_be_closed_by_user(can_be_closed_by_user),
      close_callback(std::move(close_callback)) {}

CreateOptions::~CreateOptions() = default;

}  // namespace login_screen_extension_ui
}  // namespace ash
