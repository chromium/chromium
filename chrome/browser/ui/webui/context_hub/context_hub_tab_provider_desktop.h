// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_TAB_PROVIDER_DESKTOP_H_
#define CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_TAB_PROVIDER_DESKTOP_H_

#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "chrome/browser/ui/webui/context_hub/context_hub_page_handler.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace context_hub {

struct TabGroupEntry;

class ContextHubTabProviderDesktop : public ContextHubPageHandler::TabProvider {
 public:
  explicit ContextHubTabProviderDesktop(Profile* profile);

  ContextHubTabProviderDesktop(const ContextHubTabProviderDesktop&) = delete;
  ContextHubTabProviderDesktop& operator=(const ContextHubTabProviderDesktop&) =
      delete;

  ~ContextHubTabProviderDesktop() override;

  // ContextHubPageHandler::TabProvider:
  std::vector<content::WebContents*> GetTabs() override;
  std::vector<content::WebContents*> GetUngroupedTabs() override;
  void SwitchToTab(int64_t tab_id) override;
  void CloseTab(int64_t tab_id) override;
  bool ConfirmTabGroups(
      base::span<const context_hub::TabGroupEntry> groups) override;
  void RemoveGroupFromTabstripIfOpen(const base::Uuid& saved_guid) override;
  void UngroupGroupFromTabstripIfOpen(const base::Uuid& saved_guid) override;

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_TAB_PROVIDER_DESKTOP_H_
