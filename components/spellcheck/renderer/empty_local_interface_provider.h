// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_EMPTY_LOCAL_INTERFACE_PROVIDER_H_
#define COMPONENTS_SPELLCHECK_RENDERER_EMPTY_LOCAL_INTERFACE_PROVIDER_H_

#include "services/service_manager/public/cpp/local_interface_provider.h"

namespace spellcheck {

// A dummy LocalInterfaceProvider that doesn't bind any remote application.
// May require a base::test::TaskEnvironment if GetInterface() is expected
// to be called.
class EmptyLocalInterfaceProvider
    : public service_manager::LocalInterfaceProvider {
 public:
  void GetInterface(const std::string& name,
                    mojo::ScopedMessagePipeHandle request_handle) override;
};

}  // namespace spellcheck

#endif  // COMPONENTS_SPELLCHECK_RENDERER_EMPTY_LOCAL_INTERFACE_PROVIDER_H_
