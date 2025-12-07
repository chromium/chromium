// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_sign_in_promo_bubble_view.h"

#include "chrome/browser/ui/signin/promos/bubble_signin_promo_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kBookmarkSigninPromoFrameViewId);

BookmarkSigninPromoBubbleView::BookmarkSigninPromoBubbleView(
    View* anchor_view,
    content::WebContents* web_contents,
    const bookmarks::BookmarkNode* bookmark)
    : LocationBarBubbleDelegateView(anchor_view, web_contents) {
  CHECK(bookmark);
  CHECK(web_contents);

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetTitle(IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARKED);
  SetShowCloseButton(true);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  // Show the sign in promo.
  auto* sign_in_promo = AddChildView(std::make_unique<BubbleSignInPromoView>(
      web_contents, signin_metrics::AccessPoint::kBookmarkBubble,
      syncer::LocalDataItemModel::DataId(bookmark->id())));
  SetInitiallyFocusedView(sign_in_promo->GetSignInButton());
}

BookmarkSigninPromoBubbleView::~BookmarkSigninPromoBubbleView() = default;

void BookmarkSigninPromoBubbleView::AddedToWidget() {
  GetBubbleFrameView()->SetProperty(views::kElementIdentifierKey,
                                    kBookmarkSigninPromoFrameViewId);
}

BEGIN_METADATA(BookmarkSigninPromoBubbleView)
END_METADATA
