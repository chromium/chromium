// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_INSPECT_H_
#define COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_INSPECT_H_

namespace inspect {
class Node;
}  // namespace inspect

namespace fuchsia_component_support {

// Publish the Chromium version via the Inspect API. The lifetime of
// |parent| has to be the same as the component it belongs to.
void PublishVersionInfoToInspect(inspect::Node* parent);

}  // namespace fuchsia_component_support

#endif  // COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_INSPECT_H_
