// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_page_handler.h"

#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace skills {

SkillsPageHandler::SkillsPageHandler(
    mojo::PendingReceiver<skills::mojom::PageHandler> receiver,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)), web_contents_(web_contents) {}

SkillsPageHandler::~SkillsPageHandler() = default;

}  // namespace skills
