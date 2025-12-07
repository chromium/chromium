// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/tab_restore_service_client.h"

namespace sessions {

TabRestoreServiceClient::~TabRestoreServiceClient() = default;

void TabRestoreServiceClient::OnTabRestored(const GURL& url) {}

}  // namespace sessions
