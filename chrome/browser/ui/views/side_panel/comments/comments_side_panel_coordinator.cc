// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/comments/comments_side_panel_coordinator.h"

#include "base/functional/callback.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/comments/comments_side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/collaboration/public/features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/metadata/metadata_impl_macros.h"

using SidePanelWebUIViewT_CommentsSidePanelUI =
    SidePanelWebUIViewT<CommentsSidePanelUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_CommentsSidePanelUI,
                        SidePanelWebUIViewT)
END_METADATA

// static
bool CommentsSidePanelCoordinator::IsSupported() {
  return base::FeatureList::IsEnabled(
      collaboration::features::kCollaborationComments);
}

void CommentsSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Key(SidePanelEntry::Id::kComments),
      base::BindRepeating(&CommentsSidePanelCoordinator::CreateCommentsWebView,
                          base::Unretained(this)),
      SidePanelEntry::kSidePanelDefaultContentWidth));
}

std::unique_ptr<views::View>
CommentsSidePanelCoordinator::CreateCommentsWebView(
    SidePanelEntryScope& scope) {
  return std::make_unique<SidePanelWebUIViewT<CommentsSidePanelUI>>(
      scope, base::RepeatingClosure(), base::RepeatingClosure(),
      std::make_unique<WebUIContentsWrapperT<CommentsSidePanelUI>>(
          GURL(chrome::kChromeUICommentsSidePanelURL),
          scope.GetBrowserWindowInterface().GetProfile(),
          IDS_COLLABORATION_SHARED_TAB_GROUPS_COMMENTS_TITLE,
          /*esc_closes_ui=*/false));
}
