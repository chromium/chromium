// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_component_support/inspect.h"

#include <lib/sys/inspect/cpp/component.h>

#include "components/version_info/version_info.h"

namespace fuchsia_component_support {

namespace {
const char kVersion[] = "version";
const char kLastChange[] = "last_change_revision";
}  // namespace

void PublishVersionInfoToInspect(sys::ComponentInspector* inspector) {
  // These values are managed by the inspector, since they won't be updated over
  // the lifetime of the component.
  // TODO(https://crbug.com/1077428): Add release channel.
  inspector->root().CreateString(
      kVersion, std::string(version_info::GetVersionNumber()), inspector);
  inspector->root().CreateString(
      kLastChange, std::string(version_info::GetLastChange()), inspector);
}

}  // namespace fuchsia_component_support
