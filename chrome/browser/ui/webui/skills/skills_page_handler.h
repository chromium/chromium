// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/skills/skills.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebContents;
}  // namespace content

namespace skills {

class SkillsPageHandler : public skills::mojom::PageHandler {
 public:
  SkillsPageHandler(mojo::PendingReceiver<skills::mojom::PageHandler> receiver,
                    content::WebContents* web_contents);

  SkillsPageHandler(const SkillsPageHandler&) = delete;
  SkillsPageHandler& operator=(const SkillsPageHandler&) = delete;

  ~SkillsPageHandler() override;

 private:
  mojo::Receiver<skills::mojom::PageHandler> receiver_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

}  // namespace skills

#endif  // CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_H_
