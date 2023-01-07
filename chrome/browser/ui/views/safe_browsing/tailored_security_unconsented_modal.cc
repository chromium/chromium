// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/tailored_security_unconsented_modal.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_outcome.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/layout_provider.h"

namespace safe_browsing {

namespace {

constexpr int kAvatarSize = 40;
constexpr int kAvatarOffset = 45;
constexpr int kImageOffset = 5;

void RecordModalOutcomeAndRunCallback(TailoredSecurityOutcome outcome,
                                      base::OnceClosure callback) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.TailoredSecurityUnconsentedModalOutcome", outcome);
  std::move(callback).Run();
}

void EnableEsbAndShowSettings(content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  SetSafeBrowsingState(profile->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION,
                       /*is_esb_enabled_in_sync=*/false);
  if (!chrome::FindBrowserWithWebContents(web_contents))
    return;
  chrome::ShowSafeBrowsingEnhancedProtection(
      chrome::FindBrowserWithWebContents(web_contents));
}

class SuperimposedOffsetImageSource : public gfx::CanvasImageSource {
 public:
  SuperimposedOffsetImageSource(const gfx::ImageSkia& first,
                                const gfx::ImageSkia& second)
      : gfx::CanvasImageSource(gfx::Size(first.size().width(),
                                         first.size().height() + kImageOffset)),
        first_(first),
        second_(second) {}

  SuperimposedOffsetImageSource(const SuperimposedOffsetImageSource&) = delete;
  SuperimposedOffsetImageSource& operator=(
      const SuperimposedOffsetImageSource&) = delete;

  ~SuperimposedOffsetImageSource() override = default;

  // gfx::CanvasImageSource override.
  void Draw(gfx::Canvas* canvas) override {
    canvas->DrawImageInt(first_, 0, kImageOffset);
    canvas->DrawImageInt(second_, kAvatarOffset, kAvatarOffset + kImageOffset);
  }

 private:
  const gfx::ImageSkia first_;
  const gfx::ImageSkia second_;
};

}  // namespace

/*static*/
void TailoredSecurityUnconsentedModal::ShowForWebContents(
    content::WebContents* web_contents) {
  constrained_window::ShowWebModalDialogViews(
      new TailoredSecurityUnconsentedModal(web_contents), web_contents);
}

TailoredSecurityUnconsentedModal::TailoredSecurityUnconsentedModal(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  SetModalType(ui::MODAL_TYPE_CHILD);

  SetTitle(IDS_TAILORED_SECURITY_UNCONSENTED_MODAL_TITLE);
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(
                     IDS_TAILORED_SECURITY_UNCONSENTED_ACCEPT_BUTTON));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(
                     IDS_TAILORED_SECURITY_UNCONSENTED_CANCEL_BUTTON));

  RecordModalOutcomeAndRunCallback(TailoredSecurityOutcome::kShown,
                                   base::DoNothing());

  SetAcceptCallback(base::BindOnce(
      RecordModalOutcomeAndRunCallback, TailoredSecurityOutcome::kAccepted,
      base::BindOnce(&EnableEsbAndShowSettings, web_contents_)));
  SetCancelCallback(base::BindOnce(RecordModalOutcomeAndRunCallback,
                                   TailoredSecurityOutcome::kDismissed,
                                   base::DoNothing()));
  SetCloseCallback(base::BindOnce(RecordModalOutcomeAndRunCallback,
                                  TailoredSecurityOutcome::kDismissed,
                                  base::DoNothing()));
}

TailoredSecurityUnconsentedModal::~TailoredSecurityUnconsentedModal() = default;

bool TailoredSecurityUnconsentedModal::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return (button == ui::DIALOG_BUTTON_OK || button == ui::DIALOG_BUTTON_CANCEL);
}

bool TailoredSecurityUnconsentedModal::ShouldShowCloseButton() const {
  return false;
}

void TailoredSecurityUnconsentedModal::AddedToWidget() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin))
    return;

  gfx::ImageSkia avatar_image = identity_manager
                                    ->FindExtendedAccountInfoByAccountId(
                                        identity_manager->GetPrimaryAccountId(
                                            signin::ConsentLevel::kSignin))
                                    .account_image.AsImageSkia();

  gfx::ImageSkia sized_avatar_image =
      gfx::ImageSkiaOperations::CreateResizedImage(
          avatar_image, skia::ImageOperations::RESIZE_BEST,
          gfx::Size(kAvatarSize, kAvatarSize));
  // The color used in `circle_mask` is irrelevant as long as it's opaque; only
  // the alpha channel matters.
  gfx::ImageSkia circle_mask =
      gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
          kAvatarSize / 2, SK_ColorWHITE, gfx::ImageSkia());
  gfx::ImageSkia cropped_avatar_image =
      gfx::ImageSkiaOperations::CreateMaskedImage(sized_avatar_image,
                                                  circle_mask);
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia header_image =
      *bundle.GetImageSkiaNamed(IDR_TAILORED_SECURITY_UNCONSENTED);
  gfx::ImageSkia header_and_avatar(
      std::make_unique<SuperimposedOffsetImageSource>(header_image,
                                                      cropped_avatar_image),
      gfx::Size(header_image.size().width(),
                header_image.size().height() + kImageOffset));

  auto image_view = std::make_unique<views::ImageView>(
      ui::ImageModel::FromImageSkia(header_and_avatar));
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  GetBubbleFrameView()->SetHeaderView(std::move(image_view));
}

BEGIN_METADATA(TailoredSecurityUnconsentedModal, views::DialogDelegateView)
END_METADATA

}  // namespace safe_browsing
