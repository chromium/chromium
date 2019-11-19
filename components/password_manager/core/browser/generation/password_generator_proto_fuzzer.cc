// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/password_manager/core/browser/generation/password_generator.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace autofill {

DEFINE_PROTO_FUZZER(const PasswordRequirementsSpec& spec) {
  GeneratePassword(spec);
}

}  // namespace autofill
