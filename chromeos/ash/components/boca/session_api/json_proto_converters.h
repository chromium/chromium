// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_JSON_PROTO_CONVERTERS_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_JSON_PROTO_CONVERTERS_H_

#include "base/values.h"

namespace boca {
class UserIdentity;
}  // namespace boca

namespace ash::boca {

::boca::UserIdentity ConvertUserIdentityJsonToProto(
    const base::Value::Dict* dict);

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_JSON_PROTO_CONVERTERS_H_
