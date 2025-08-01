// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/ai_mode_page_action_icon_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/grit/branded_strings.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "net/base/url_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace {

constexpr char kAiModeBaseUrl[] =
    "https://www.google.com/search?sourceid=chrome&udm=50&aep=48";

}

AiModePageActionIconView::AiModePageActionIconView(
    IconLabelBubbleView::Delegate* parent_delegate,
    Delegate* delegate,
    BrowserWindowInterface* browser)
    : PageActionIconView(nullptr,
                         0,
                         parent_delegate,
                         delegate,
                         "AiMode",
                         kActionAiMode),
      browser_(browser) {
  CHECK(browser_);
  image_container_view()->SetFlipCanvasOnPaintForRTLUI(false);

  SetProperty(views::kElementIdentifierKey, kAiModePageActionIconElementId);

  SetLabel(l10n_util::GetStringUTF16(IDS_AI_MODE_ENTRYPOINT_LABEL));
  SetUseTonalColorsWhenExpanded(true);
  SetBackgroundVisibility(BackgroundVisibility::kWithLabel);
}

AiModePageActionIconView::~AiModePageActionIconView() = default;

void AiModePageActionIconView::UpdateImpl() {
  SetVisible(ShouldShow());
  ResetSlideAnimation(true);
}

bool AiModePageActionIconView::ShouldShow() {
  auto* web_contents = GetWebContents();
  if (!web_contents) {
    return false;
  }
  OmniboxView* omnibox_view = search::GetOmniboxView(web_contents);
  if (!omnibox_view) {
    return false;
  }
  // Show the AIM chip ONLY IF the Omnibox is visibly focused.
  return omnibox_view->model()->is_caret_visible();
}

void AiModePageActionIconView::OnExecuting(
    PageActionIconView::ExecuteSource source) {
  GURL ai_mode_url = GURL(kAiModeBaseUrl);

  auto* web_contents = GetWebContents();
  CHECK(web_contents);
  OmniboxView* omnibox_view = search::GetOmniboxView(web_contents);
  CHECK(omnibox_view);

  const auto match = omnibox_view->model()->CurrentMatch(nullptr);
  if (AutocompleteMatch::IsSearchType(match.type) &&
      !omnibox_view->model()->is_keyword_selected()) {
    auto query_text = match.contents;
    if (!query_text.empty()) {
      ai_mode_url = net::AppendQueryParameter(ai_mode_url, "q",
                                              base::UTF16ToUTF8(query_text));
    }
  }

  // TODO(crbug.com/432744091): Replace direct URL navigation with invocation
  // of OmniboxEditModel::OpenSelection().
  // A transition type of `PAGE_TRANSITION_AUTO_BOOKMARK` is used here in order
  // to signal that this URL is loaded as a result of the user clicking on a UI
  // control (AIM page action button) which is located in the location bar.
  web_contents->OpenURL(
      content::OpenURLParams(
          ai_mode_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
          ui::PAGE_TRANSITION_AUTO_BOOKMARK, /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
}

views::BubbleDialogDelegate* AiModePageActionIconView::GetBubble() const {
  return nullptr;
}

const gfx::VectorIcon& AiModePageActionIconView::GetVectorIcon() const {
  return omnibox::kSearchSparkIcon;
}

void AiModePageActionIconView::ExecuteWithKeyboardSourceForTesting() {
  CHECK(GetVisible());
  OnExecuting(EXECUTE_SOURCE_KEYBOARD);
}

BEGIN_METADATA(AiModePageActionIconView)
END_METADATA
