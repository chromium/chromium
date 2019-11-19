// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/cryptohome_key_constants.h"

namespace chromeos {

// The label used for the key derived from the user's GAIA credentials.
//
// Also, for historical reasons, this key is used for Active Directory users and
// for Public Session keys.
//
// TODO(crbug.com/826417): Introduce a separate constant for the Public Session
// key label.
const char kCryptohomeGaiaKeyLabel[] = "gaia";

}  // namespace chromeos
