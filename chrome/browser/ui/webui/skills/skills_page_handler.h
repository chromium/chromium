// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_H_

#include "chrome/browser/ui/webui/skills/skills.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace skills {

class SkillsPageHandler : public skills::mojom::PageHandler {
 public:
  explicit SkillsPageHandler(
      mojo::PendingReceiver<skills::mojom::PageHandler> receiver);

  SkillsPageHandler(const SkillsPageHandler&) = delete;
  SkillsPageHandler& operator=(const SkillsPageHandler&) = delete;

  ~SkillsPageHandler() override;

 private:
  mojo::Receiver<skills::mojom::PageHandler> receiver_;
};

}  // namespace skills

#endif  // CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_H_
