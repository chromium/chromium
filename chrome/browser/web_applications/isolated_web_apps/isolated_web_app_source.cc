// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "base/files/file_path.h"
#include "url/origin.h"

namespace web_app {

bool IwaSourceBundle::operator==(const IwaSourceBundle& other) const = default;

bool IwaSourceProxy::operator==(const IwaSourceProxy& other) const = default;

}  // namespace web_app
