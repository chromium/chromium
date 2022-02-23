// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/public/autofill_assistant.h"

#include "base/containers/flat_map.h"

namespace autofill_assistant {

AutofillAssistant::CapabilitiesInfo::CapabilitiesInfo() = default;
AutofillAssistant::CapabilitiesInfo::CapabilitiesInfo(
    const std::string& url,
    const base::flat_map<std::string, std::string>& script_parameters)
    : url(url), script_parameters(script_parameters) {}
AutofillAssistant::CapabilitiesInfo::~CapabilitiesInfo() = default;
AutofillAssistant::CapabilitiesInfo::CapabilitiesInfo(
    const CapabilitiesInfo& other) = default;
AutofillAssistant::CapabilitiesInfo&
AutofillAssistant::CapabilitiesInfo::operator=(const CapabilitiesInfo& other) =
    default;

}  // namespace autofill_assistant
