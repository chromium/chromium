// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEARCH_AI_MODE_SIGNIN_PROMO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEARCH_AI_MODE_SIGNIN_PROMO_VIEW_H_

#include "base/timer/timer.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/signin/public/base/signin_metrics.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"

class AIModeSignInPromoControllerBase;
class SearchAIModeSignInPromoController;
class ComposeboxDriveSignInPromoController;

// Base view for the location bar sign-in promo bubble.
class AIModeSignInPromoViewBase : public LocationBarBubbleDelegateView {
  METADATA_HEADER(AIModeSignInPromoViewBase, LocationBarBubbleDelegateView)
 public:
  AIModeSignInPromoViewBase(
      views::BubbleAnchor anchor,
      content::WebContents* web_contents,
      base::WeakPtr<AIModeSignInPromoControllerBase> controller,
      signin_metrics::AccessPoint access_point);
  AIModeSignInPromoViewBase(const AIModeSignInPromoViewBase&) = delete;
  AIModeSignInPromoViewBase& operator=(const AIModeSignInPromoViewBase&) =
      delete;

  ~AIModeSignInPromoViewBase() override;

 protected:
  // views::WidgetDelegate:
  void WindowClosing() override;

  void Close();

 private:
  base::WeakPtr<AIModeSignInPromoControllerBase> controller_;
};

DECLARE_ELEMENT_IDENTIFIER_VALUE(kSearchAIModeSignInPromoFrameViewId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSearchAIModeSignInPromoViewId);

// View for the Search AI Mode sign-in promo bubble.
class SearchAIModeSignInPromoView : public AIModeSignInPromoViewBase {
  METADATA_HEADER(SearchAIModeSignInPromoView, AIModeSignInPromoViewBase)
 public:
  SearchAIModeSignInPromoView(
      views::BubbleAnchor anchor,
      content::WebContents* web_contents,
      base::WeakPtr<SearchAIModeSignInPromoController> controller);
  ~SearchAIModeSignInPromoView() override;

  void FireTimerForTesting();
  bool IsTimerRunningForTesting() const;

 protected:
  // LocationBarBubbleDelegateView:
  void AddedToWidget() override;

 private:
  base::OneShotTimer self_dismissal_timer_;
};

DECLARE_ELEMENT_IDENTIFIER_VALUE(kComposeboxDriveSignInPromoFrameViewId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kComposeboxDriveSignInPromoViewId);

// View for the Composebox Drive context menu option sign-in promo bubble.
class ComposeboxDriveSignInPromoView : public AIModeSignInPromoViewBase {
  METADATA_HEADER(ComposeboxDriveSignInPromoView, AIModeSignInPromoViewBase)
 public:
  ComposeboxDriveSignInPromoView(
      views::BubbleAnchor anchor,
      content::WebContents* web_contents,
      base::WeakPtr<ComposeboxDriveSignInPromoController> controller);
  ~ComposeboxDriveSignInPromoView() override;

 protected:
  // LocationBarBubbleDelegateView:
  void AddedToWidget() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SEARCH_AI_MODE_SIGNIN_PROMO_VIEW_H_
