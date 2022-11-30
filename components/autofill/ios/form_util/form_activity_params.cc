// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/form_util/form_activity_params.h"

namespace autofill {

BaseFormActivityParams::BaseFormActivityParams() = default;
BaseFormActivityParams::BaseFormActivityParams(
    const BaseFormActivityParams& other) = default;
BaseFormActivityParams::~BaseFormActivityParams() = default;

FormActivityParams::FormActivityParams() = default;
FormActivityParams::FormActivityParams(const FormActivityParams& other) =
    default;
FormActivityParams::~FormActivityParams() = default;

FormRemovalParams::FormRemovalParams() = default;
FormRemovalParams::FormRemovalParams(const FormRemovalParams& other) = default;
FormRemovalParams::~FormRemovalParams() = default;

}  // namespace autofill
