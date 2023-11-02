// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_INSPECT_H_
#define COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_INSPECT_H_

namespace sys {
class ComponentInspector;
}  // namespace sys

namespace fuchsia_component_support {

// Publish the Chromium version via the Inspect API. The lifetime of
// |inspector| has to be the same as the component it belongs to.
void PublishVersionInfoToInspect(sys::ComponentInspector* inspector);

}  // namespace fuchsia_component_support

#endif  // COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_INSPECT_H_