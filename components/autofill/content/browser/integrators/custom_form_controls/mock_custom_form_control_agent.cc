// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/custom_form_controls/mock_custom_form_control_agent.h"

#include <utility>

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace autofill {

MockCustomFormControlAgent::MockCustomFormControlAgent() = default;

MockCustomFormControlAgent::~MockCustomFormControlAgent() = default;

void MockCustomFormControlAgent::BindPendingReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receivers_.Add(this,
                 mojo::PendingAssociatedReceiver<mojom::CustomFormControlAgent>(
                     std::move(handle)));
}

}  // namespace autofill
