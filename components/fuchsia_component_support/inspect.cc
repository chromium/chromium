// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_component_support/inspect.h"

#include <lib/inspect/cpp/inspect.h>

#include "components/version_info/version_info.h"

namespace fuchsia_component_support {

namespace {
const char kVersion[] = "version";
const char kLastChange[] = "last_change_revision";
}  // namespace

void PublishVersionInfoToInspect(inspect::Node* parent) {
  // These values are managed by the inspector, since they won't be updated over
  // the lifetime of the component.
  // TODO(crbug.com/42050100): Add release channel.
  parent->RecordString(kVersion, std::string(version_info::GetVersionNumber()));
  parent->RecordString(kLastChange, std::string(version_info::GetLastChange()));
}

}  // namespace fuchsia_component_support
