// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/public/renderer_controller_proxy.h"

namespace cast_streaming {

// static
RendererControllerProxy* RendererControllerProxy::singleton_instance_ = nullptr;

// static
RendererControllerProxy* RendererControllerProxy::GetInstance() {
  return singleton_instance_;
}

RendererControllerProxy::RendererControllerProxy() {
  DCHECK(!singleton_instance_);
  singleton_instance_ = this;
}

RendererControllerProxy::~RendererControllerProxy() {
  DCHECK(singleton_instance_);
  singleton_instance_ = nullptr;
}

}  // namespace cast_streaming
