// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_post_install_dialog_view_utils.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_view.h"
#include "components/signin/public/base/signin_switches.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "ui/base/models/dialog_model.h"
#include "ui/color/color_id.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

namespace extensions {

namespace {

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
std::unique_ptr<views::View> CreateSigninPromoFootnoteView(
    content::WebContents* web_contents,
    const extensions::ExtensionId& extension_id) {
  auto promo_view = std::make_unique<BubbleSignInPromoView>(
      web_contents, signin_metrics::AccessPoint::kExtensionInstallBubble,
      syncer::LocalDataItemModel::DataId(extension_id));

  // Add color and insets to mimic the footnote view on the dialog. We
  // cannot add the footnote using ui::DialogModel because it doesn't
  // support complex footers.
  auto wrapper_view = std::make_unique<views::BoxLayoutView>();
  wrapper_view->SetBackground(
      views::CreateSolidBackground(ui::kColorBubbleFooterBackground));
  const auto& layout_provider = *views::LayoutProvider::Get();
  // Add the dialog insets to the wrapper view to mimic the footnote view.
  // The footnote view adds the dialog insets to its margin, so the promo
  // view also has to add them to have the same visual effect.
  const gfx::Insets dialog_insets =
      layout_provider.GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG);
  wrapper_view->SetInsideBorderInsets(
      gfx::Insets::VH(dialog_insets.top(), dialog_insets.left()));

  wrapper_view->AddChildView(std::move(promo_view));
  return wrapper_view;
}
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

}  // namespace

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
void MaybeAddSigninPromoFootnoteView(
    Profile* profile,
    content::WebContents* web_contents,
    const extensions::Extension& extension,
    ui::DialogModel::Builder& dialog_model_builder) {
  if (signin::ShouldShowExtensionSignInPromo(*profile, extension)) {
    // We use a custom field instead of a footnote because footnote doesn't
    // support complex views.
    dialog_model_builder.AddCustomField(
        std::make_unique<views::BubbleDialogModelHost::CustomView>(
            CreateSigninPromoFootnoteView(web_contents, extension.id()),
            views::BubbleDialogModelHost::FieldType::kMenuItem));
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

}  // namespace extensions
