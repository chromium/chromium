// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/renderer/mock_renderer_agent.h"

#include "components/fingerprinting_protection_filter/renderer/renderer_agent.h"
#include "content/public/renderer/render_frame.h"

namespace fingerprinting_protection_filter {

MockRendererAgent::MockRendererAgent() : RendererAgent(nullptr) {}

MockRendererAgent::~MockRendererAgent() = default;

}  // namespace fingerprinting_protection_filter
