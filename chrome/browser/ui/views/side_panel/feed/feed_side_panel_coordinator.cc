// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/feed/feed_side_panel_coordinator.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/feed/feed_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/views/vector_icons.h"

namespace feed {

FeedSidePanelCoordinator::FeedSidePanelCoordinator(Browser* browser)
    : BrowserUserData<FeedSidePanelCoordinator>(*browser) {}

FeedSidePanelCoordinator::~FeedSidePanelCoordinator() = default;

void FeedSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kFeed, l10n_util::GetStringUTF16(IDS_FEED_TITLE),
      ui::ImageModel::FromVectorIcon(vector_icons::kFeedIcon, ui::kColorIcon),
      base::BindRepeating(&FeedSidePanelCoordinator::CreateFeedWebUIView,
                          base::Unretained(this))));
}

std::unique_ptr<views::View> FeedSidePanelCoordinator::CreateFeedWebUIView() {
  auto view = std::make_unique<SidePanelWebUIViewT<FeedUI>>(
      base::RepeatingClosure(), base::RepeatingClosure(),
      std::make_unique<BubbleContentsWrapperT<FeedUI>>(
          GURL(chrome::kChromeUIUntrustedFeedURL), GetBrowser().profile(),
          IDS_FEED_TITLE,
          /*webui_resizes_host=*/false,
          /*esc_closes_ui=*/false));
  // Show the feed page immediately. The loading spinner will show up in the
  // page if we need to wait for the feed content coming from the server.
  view->SetVisible(true);
  SidePanelUtil::GetSidePanelContentProxy(view.get())->SetAvailable(true);
  return view;
}

BROWSER_USER_DATA_KEY_IMPL(FeedSidePanelCoordinator);

}  // namespace feed
