// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/content/private_ai_oak_session_driver_content.h"

#include "content/public/browser/service_process_host.h"

namespace private_ai {

mojo::Remote<mojom::OakSession>
PrivateAiOakSessionDriverContent::BindOakSessionService() {
  return content::ServiceProcessHost::Launch<mojom::OakSession>(
      content::ServiceProcessHost::Options()
          .WithDisplayName("Oak Session Service")
          .Pass());
}

}  // namespace private_ai
