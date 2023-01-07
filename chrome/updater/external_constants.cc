// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/external_constants.h"

#include "base/memory/scoped_refptr.h"

namespace updater {

ExternalConstants::ExternalConstants(
    scoped_refptr<ExternalConstants> next_provider)
    : next_provider_(next_provider) {}

ExternalConstants::~ExternalConstants() = default;

}  // namespace updater
