// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

App::App(AppType app_type, const std::string& app_id)
    : app_type(app_type), app_id(app_id) {}

App::~App() = default;

}  // namespace apps
