// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_promo_bubble_view.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_delegate.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace send_tab_to_self {

namespace {

std::unique_ptr<BubbleSignInPromoDelegate> CreateSignInPromoDelegate(
    content::WebContents* web_contents) {
  CHECK(web_contents);
  auto* controller =
      SendTabToSelfBubbleController::GetOrCreateForWebContents(web_contents);
  CHECK(controller);
  return std::make_unique<DefaultBubbleSignInPromoDelegate>(
      *web_contents, signin_metrics::AccessPoint::kSendTabToSelfPromo,
      base::BindOnce(
          &SendTabToSelfBubbleController::ShowBubble, controller->AsWeakPtr(),
          controller->entry_point().value_or(ShareEntryPoint::kShareSheet)));
}

}  // namespace

SendTabToSelfSignInPromoBubbleView::SendTabToSelfSignInPromoBubbleView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents)
    : BubbleSignInPromoDialogView(
          anchor,
          web_contents,
          signin_metrics::AccessPoint::kSendTabToSelfPromo,
          CreateSignInPromoDelegate(web_contents),
          IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_TITLE,
          IDS_SEND_TAB_TO_SELF_VERIFY_ITS_YOU_PROMO_TITLE,
          IDR_INSTANT_HANDOFF_ILLUSTRATION) {}

SendTabToSelfSignInPromoBubbleView::~SendTabToSelfSignInPromoBubbleView() =
    default;

BEGIN_METADATA(SendTabToSelfSignInPromoBubbleView)
END_METADATA

}  // namespace send_tab_to_self
