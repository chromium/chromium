// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_UI_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/skills/skills.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "components/skills/public/skill.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
namespace skills {

class SkillsPageHandler;
class SkillsDialogHandler;
class SkillsDialogDelegate;

// MojoWebUIController for the chrome://skills page.
class SkillsUI : public ui::MojoWebUIController,
                 public skills::mojom::PageHandlerFactory {
 public:
  explicit SkillsUI(content::WebUI* web_ui);
  SkillsUI(const SkillsUI&) = delete;
  SkillsUI& operator=(const SkillsUI&) = delete;

  ~SkillsUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<skills::mojom::PageHandlerFactory> receiver);

  // Initializes the SkillsDialogDelegate and initial skill for the dialog.
  void InitializeDialog(base::WeakPtr<SkillsDialogDelegate> delegate,
                        Skill skill);

  base::WeakPtr<SkillsDialogDelegate> GetDelegateForTesting() {
    return delegate_;
  }

  const Skill& GetInitialSkillForTesting() const { return initial_skill_; }

 private:
  // The PendingRemote must be valid and bind to a receiver in order to start
  // sending messages to the receiver.
  void CreatePageHandler(
      mojo::PendingRemote<skills::mojom::SkillsPage> page,
      mojo::PendingReceiver<skills::mojom::PageHandler> receiver) override;

  void CreateDialogHandler(
      mojo::PendingReceiver<skills::mojom::DialogHandler> receiver) override;

  // The initial state for the skills dialog. This is passed to the
  // DialogHandler upon construction to populate the UI fields.
  skills::Skill initial_skill_;
  std::unique_ptr<SkillsPageHandler> page_handler_;
  std::unique_ptr<SkillsDialogHandler> dialog_handler_;
  base::WeakPtr<SkillsDialogDelegate> delegate_;

  mojo::Receiver<skills::mojom::PageHandlerFactory> page_factory_receiver_{
      this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

class SkillsUIConfig : public content::DefaultWebUIConfig<SkillsUI> {
 public:
  SkillsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISkillsHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace skills

#endif  // CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_UI_H_
