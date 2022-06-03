// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/website_login_manager.h"

namespace autofill_assistant {

WebsiteLoginManager::Login::Login(const GURL& _origin,
                                  const std::string& _username)
    : origin(_origin), username(_username) {}

WebsiteLoginManager::Login::Login(const Login& other) = default;
WebsiteLoginManager::Login::~Login() = default;

}  // namespace autofill_assistant
