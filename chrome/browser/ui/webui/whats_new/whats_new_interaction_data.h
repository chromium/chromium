// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_INTERACTION_DATA_H_
#define CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_INTERACTION_DATA_H_

#include "chrome/browser/ui/webui/whats_new/whats_new.mojom-data-view.h"
#include "content/public/browser/web_contents_user_data.h"

class WhatsNewInteractionData
    : public content::WebContentsUserData<WhatsNewInteractionData> {
 public:
  ~WhatsNewInteractionData() override;

  struct ModuleShown {
    const std::string name;
    whats_new::mojom::ModulePosition position =
        whats_new::mojom::ModulePosition::kSpotlight1;
  };

  struct InteractionMetrics {
    InteractionMetrics();
    ~InteractionMetrics();
    whats_new::mojom::ScrollDepth scroll_depth =
        whats_new::mojom::ScrollDepth::k0;

    std::vector<ModuleShown> modules_shown;
  };

  whats_new::mojom::ScrollDepth scroll_depth() const {
    return interaction_metrics_.scroll_depth;
  }
  void set_scroll_depth(whats_new::mojom::ScrollDepth depth) {
    interaction_metrics_.scroll_depth = depth;
  }

  void add_module_shown(const std::string& name,
                        whats_new::mojom::ModulePosition position) {
    interaction_metrics_.modules_shown.push_back({name, position});
  }

  const std::vector<ModuleShown>& modules_shown() const {
    return interaction_metrics_.modules_shown;
  }

 private:
  explicit WhatsNewInteractionData(content::WebContents* web_contents);
  friend class content::WebContentsUserData<WhatsNewInteractionData>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  InteractionMetrics interaction_metrics_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_INTERACTION_DATA_H_
