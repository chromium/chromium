// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_page_handler.h"

#include "mojo/public/cpp/bindings/receiver.h"

namespace skills {

SkillsPageHandler::SkillsPageHandler(
    mojo::PendingReceiver<skills::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

SkillsPageHandler::~SkillsPageHandler() = default;

}  // namespace skills
