// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/comments/comments_side_panel_ui.h"

#include "base/feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/side_panel/comments/comments.mojom.h"
#include "chrome/browser/ui/webui/side_panel/comments/comments_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/side_panel_comments_resources.h"
#include "chrome/grit/side_panel_comments_resources_map.h"
#include "components/collaboration/public/features.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

CommentsSidePanelUIConfig::CommentsSidePanelUIConfig()
    : DefaultTopChromeWebUIConfig<CommentsSidePanelUI>(
          content::kChromeUIScheme,
          chrome::kChromeUICommentsSidePanelHost) {}

bool CommentsSidePanelUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      collaboration::features::kCollaborationComments);
}

std::optional<int> CommentsSidePanelUIConfig::GetCommandIdForTesting() {
  return IDC_SHOW_COMMENTS_SIDE_PANEL;
}

CommentsSidePanelUI::CommentsSidePanelUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui, true) {
  Profile* const profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUICommentsSidePanelHost);
  webui::SetupWebUIDataSource(source, kSidePanelCommentsResources,
                              IDR_SIDE_PANEL_COMMENTS_COMMENTS_HTML);
}

CommentsSidePanelUI::~CommentsSidePanelUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(CommentsSidePanelUI)

void CommentsSidePanelUI::BindInterface(
    mojo::PendingReceiver<comments::mojom::PageHandlerFactory> receiver) {
  comments_page_factory_receiver_.reset();
  comments_page_factory_receiver_.Bind(std::move(receiver));
}

void CommentsSidePanelUI::CreatePageHandler(
    mojo::PendingRemote<comments::mojom::Page> page,
    mojo::PendingReceiver<comments::mojom::PageHandler> receiver) {
  comments_page_handler_ = std::make_unique<CommentsPageHandler>(
      std::move(receiver), std::move(page), *this, *web_ui());
}
