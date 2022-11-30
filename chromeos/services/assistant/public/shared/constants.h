// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_SHARED_CONSTANTS_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_SHARED_CONSTANTS_H_

#include "base/component_export.h"

namespace chromeos {
namespace assistant {

// HTTP request related constants.
COMPONENT_EXPORT(ASSISTANT_SERVICE_SHARED)
extern const char kPayloadParamName[];
COMPONENT_EXPORT(ASSISTANT_SERVICE_SHARED)
extern const char kKnowledgeApiEndpoint[];
COMPONENT_EXPORT(ASSISTANT_SERVICE_SHARED)
extern const char kSampleKnowledgeApiRequest[];
COMPONENT_EXPORT(ASSISTANT_SERVICE_SHARED)
extern const char kServiceIdEndpoint[];
COMPONENT_EXPORT(ASSISTANT_SERVICE_SHARED)
extern const char kSampleServiceIdRequest[];
COMPONENT_EXPORT(ASSISTANT_SERVICE_SHARED)
extern const char kServiceIdRequestPayload[];

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_SHARED_CONSTANTS_H_
