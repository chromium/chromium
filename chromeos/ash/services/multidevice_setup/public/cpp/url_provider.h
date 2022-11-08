// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_URL_PROVIDER_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_URL_PROVIDER_H_

#include "url/gurl.h"

namespace ash {

namespace multidevice_setup {

GURL GetBoardSpecificBetterTogetherSuiteLearnMoreUrl();
GURL GetBoardSpecificMessagesLearnMoreUrl();

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_URL_PROVIDER_H_
