// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent.h"

namespace apps {

IntentFile::IntentFile(const GURL& url) : url(url) {}

IntentFile::~IntentFile() = default;

Intent::Intent(const std::string& action) : action(action) {}

Intent::~Intent() = default;

}  // namespace apps
