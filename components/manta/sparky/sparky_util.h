// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_SPARKY_SPARKY_UTIL_H_
#define COMPONENTS_MANTA_SPARKY_SPARKY_UTIL_H_

#include "base/component_export.h"
#include "components/manta/proto/sparky.pb.h"
#include "components/manta/sparky/sparky_delegate.h"
#include "components/manta/sparky/system_info_delegate.h"

namespace manta {

void COMPONENT_EXPORT(MANTA)
    AddSettingsProto(const SparkyDelegate::SettingsDataList& settings_list,
                     ::manta::proto::SettingsData* settings_data);

std::vector<Diagnostics> COMPONENT_EXPORT(MANTA)
    ObtainDiagnosticsVectorFromProto(
        const ::manta::proto::DiagnosticsRequest& diagnostics_request);

void COMPONENT_EXPORT(MANTA)
    AddDiagnosticsProto(std::unique_ptr<DiagnosticsData> diagnostics_data,
                        proto::DiagnosticsData* diagnostics_proto);

}  // namespace manta

#endif  // COMPONENTS_MANTA_SPARKY_SPARKY_UTIL_H_
