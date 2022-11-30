// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/external_constants_default.h"

namespace updater {

scoped_refptr<ExternalConstants> CreateExternalConstants() {
  return CreateDefaultExternalConstants();
}

}  // namespace updater
