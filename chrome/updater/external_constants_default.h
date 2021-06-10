// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_EXTERNAL_CONSTANTS_DEFAULT_H_
#define CHROME_UPDATER_EXTERNAL_CONSTANTS_DEFAULT_H_

#include <memory>

namespace updater {

class ExternalConstants;

std::unique_ptr<ExternalConstants> CreateDefaultExternalConstants();

}  // namespace updater

#endif  // CHROME_UPDATER_EXTERNAL_CONSTANTS_DEFAULT_H_
