// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/empty_local_interface_provider.h"

namespace spellcheck {

void EmptyLocalInterfaceProvider::GetInterface(
    const std::string& name,
    mojo::ScopedMessagePipeHandle request_handle) {}

}  // namespace spellcheck
