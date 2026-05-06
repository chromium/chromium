// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_flow_dialog_delegate.h"

#include <algorithm>
#include <compare>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/site_icon_text_and_origin_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/web_apps/progress_delay.h"
#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_flow_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_intro_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_options_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_progress_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/ui/web_applications/web_app_info_image_source.h"
#include "chrome/browser/web_applications/model/dialog_image_info.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_screenshot_fetcher.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/common/constants.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "skia/ext/image_operations.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebAppInstallFlowDialogDelegate,
                                      kInstallDialogFlowViewId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebAppInstallFlowDialogDelegate,
                                      kLearnMoreButtonId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebAppInstallFlowDialogDelegate,
                                      kCancelButtonId);

std::ostream& operator<<(std::ostream& os, InstallOsType type) {
  switch (type) {
    case InstallOsType::kWin:
      return os << "kWin";
    case InstallOsType::kMac:
      return os << "kMac";
    case InstallOsType::kCros:
      return os << "kCros";
    case InstallOsType::kOther:
      return os << "kOther";
  }
  return os << "Unknown";
}

namespace {

// The defaulted <=> compares fields in declaration order: not needing
// upscaling beats needing it, and within each group the smaller distance to
// the target wins.
struct BitmapSizeRank {
  bool needs_upscaling;
  SquareSizePx distance_to_target;

  auto operator<=>(const BitmapSizeRank&) const = default;
};

// Selects a bitmap for each supported scale factor, preferring one that
// doesn't require upscaling, and resizes it to the target pixel size.
gfx::ImageSkia CreateResizedIconImage(const UnorderedSizeToBitmap& bitmaps,
                                      int target_size_dp) {
  gfx::ImageSkia resized_image;
  if (bitmaps.empty()) {
    return resized_image;
  }

  for (const auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
    const float scale = ui::GetScaleForResourceScaleFactor(scale_factor);
    const int target_size = base::saturated_cast<int>(target_size_dp * scale);

    const auto it = std::ranges::min_element(
        bitmaps, {}, [target_size](const auto& bitmap) -> BitmapSizeRank {
          const SquareSizePx size = bitmap.first;
          return {.needs_upscaling = size < target_size,
                  .distance_to_target = std::abs(size - target_size)};
        });
    SkBitmap bitmap = it->second;
    if (bitmap.width() != target_size) {
      bitmap = skia::ImageOperations::Resize(
          bitmap, skia::ImageOperations::RESIZE_BEST, target_size, target_size);
    }
    resized_image.AddRepresentation(gfx::ImageSkiaRep(bitmap, scale));
  }
  return resized_image;
}

}  // namespace

WebAppInstallFlowDialogDelegate::WebAppInstallFlowDialogDelegate(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> install_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    WebAppInstallationAcceptanceCallback callback,
    PwaInProductHelpState iph_state,
    PrefService* prefs,
    feature_engagement::Tracker* tracker,
    InstallDialogType dialog_type,
    InstallOsType os_type,
    std::unique_ptr<ProgressDelay> progress_delay)
    : WebAppInstallDialogDelegate(
          web_contents,
          std::move(install_info),
          std::move(install_tracker),
          base::BindOnce(&WebAppInstallFlowDialogDelegate::OnAcceptCallback,
                         base::Unretained(this)),
          iph_state,
          prefs,
          tracker,
          dialog_type),
      os_type_(os_type),
      callback_(std::move(callback)),
      progress_delay_(std::move(progress_delay)) {
  CHECK(progress_delay_);
}

WebAppInstallFlowDialogDelegate::~WebAppInstallFlowDialogDelegate() = default;

bool WebAppInstallFlowDialogDelegate::AdvanceToNextStepOrClose() {
  CHECK(dialog_model());

  // Update install dialog step.
  switch (current_step_) {
    case InstallDialogStep::kInstallDialog:
      if (os_type_ == InstallOsType::kOther) {
        current_step_ = InstallDialogStep::kProgress;
      } else {
        current_step_ = InstallDialogStep::kInstallerOptions;
      }
      break;

    case InstallDialogStep::kInstallerOptions:
      current_step_ = InstallDialogStep::kProgress;
      break;

    case InstallDialogStep::kProgress:
      current_step_ = InstallDialogStep::kSuccessful;
      break;

    case InstallDialogStep::kSuccessful:
      if (reparent_closure_) {
        std::move(reparent_closure_).Run();
      }
      return true;
  }

  // Actions based on the new current_step_
  switch (current_step_) {
    case InstallDialogStep::kInstallDialog:
      break;

    case InstallDialogStep::kInstallerOptions: {
      ui::DialogModel::Button* ok_button =
          dialog_model()->GetButtonByUniqueId(kPwaInstallDialogInstallButton);
      if (ok_button) {
        dialog_model()->SetButtonLabel(ok_button,
                                       l10n_util::GetStringUTF16(IDS_INSTALL));
      }
      break;
    }

    case InstallDialogStep::kProgress:
      // Trigger the installation.
      // TODO(crbug.com/508383640): Clean up metrics usage for new install flow.
      OnAccept();

      progress_delay_->Start(
          base::BindRepeating(&WebAppInstallFlowDialogDelegate::OnProgress,
                              weak_ptr_factory_.GetWeakPtr()));

      // Hide buttons on progress step.
      dialog_model()->SetVisible(kLearnMoreButtonId, false);
      dialog_model()->SetVisible(kPwaInstallDialogInstallButton, false);
      dialog_model()->SetVisible(kCancelButtonId, false);
      break;

    case InstallDialogStep::kSuccessful: {
      ui::DialogModel::Button* ok_button =
          dialog_model()->GetButtonByUniqueId(kPwaInstallDialogInstallButton);
      if (ok_button) {
        dialog_model()->SetButtonLabel(
            ok_button,
            l10n_util::GetStringUTF16(IDS_WEB_APP_INSTALL_SUCCESS_OPEN_APP));
      }
      ui::DialogModel::Button* cancel_button =
          dialog_model()->GetButtonByUniqueId(kCancelButtonId);
      if (cancel_button) {
        dialog_model()->SetButtonLabel(cancel_button,
                                       l10n_util::GetStringUTF16(IDS_CLOSE));
      }
      dialog_model()->SetVisible(kPwaInstallDialogInstallButton, true);
      dialog_model()->SetVisible(kCancelButtonId, true);
      break;
    }
  }

  if (flow_view_) {
    flow_view_->UpdateStepVisibility(current_step_);
  }

  UpdateDialogTitleAndHeader(current_step_);

  return false;
}

void WebAppInstallFlowDialogDelegate::OnLearnMoreButtonClicked() {
  web_contents()->OpenURL(
      content::OpenURLParams(
          GURL(chrome::kInstallDialogFlowLearnMoreURL), content::Referrer(),
          WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
          /*is_renderer_initiated=*/false),
      base::DoNothing());
}

void WebAppInstallFlowDialogDelegate::OnAccept() {
  if (options_view_) {
    // Pin to Shelf and Pin to Taskbar cannot both be true at the same time
    // because they're only set in different OS configurations, so this should
    // be fine.
    install_info_->add_to_quick_launch_bar =
        options_view_->IsPinToShelfChecked() ||
        options_view_->IsPinToTaskBarChecked();
    install_info_->add_to_desktop =
        options_view_->IsAddDesktopShortcutChecked();
  }
  WebAppInstallDialogDelegate::OnAccept();
}

// Updates dialog title based on current step.
void WebAppInstallFlowDialogDelegate::UpdateDialogTitleAndHeader(
    InstallDialogStep step) {
  std::u16string title;
  switch (step) {
    case InstallDialogStep::kInstallDialog:
      NOTREACHED();
    case InstallDialogStep::kInstallerOptions:
      title = l10n_util::GetStringUTF16(dialog_type() == InstallDialogType::kDiy
                                            ? IDS_DIY_APP_INSTALL_DIALOG_TITLE
                                            : IDS_INSTALL_PWA_DIALOG_TITLE);
      break;
    case InstallDialogStep::kProgress:
      title = l10n_util::GetStringUTF16(IDS_INSTALL_PWA_DIALOG_INSTALLING);
      break;
    case InstallDialogStep::kSuccessful:
      title = l10n_util::GetStringUTF16(IDS_WEB_APP_INSTALL_SUCCESS_TITLE);
      break;
  }

  CHECK(dialog_model() && dialog_model()->host());
  auto* host =
      static_cast<views::BubbleDialogModelHost*>(dialog_model()->host());
  host->SetTitle(title);
  host->SetAccessibleTitle(title);
  // Clear the subtitle for all subsequent steps.
  host->SetSubtitle(std::u16string());

  // Clear the header for all subsequent steps.
  if (host->GetBubbleFrameView()) {
    host->GetBubbleFrameView()->SetHeaderView(nullptr);
  }
}

void WebAppInstallFlowDialogDelegate::UpdateProgressAndMaybeAdvance() {
  if (current_step_ != InstallDialogStep::kProgress) {
    return;
  }
  double percent =
      timer_percentage_.value_or(1.0) * 0.9 + (install_success_ ? 0.1 : 0);
  if (progress_view_) {
    progress_view_->SetProgressValue(percent);
  }

  if (!timer_percentage_.has_value() && install_success_) {
    AdvanceToNextStepOrClose();
  }
}

void WebAppInstallFlowDialogDelegate::OnInstallResult(
    bool success,
    base::OnceClosure reparent_closure) {
  if (!success) {
    CloseDialogAsIgnored();
    return;
  }
  install_success_ = true;
  reparent_closure_ = std::move(reparent_closure);
  IntentPickerTabHelper* helper =
      IntentPickerTabHelper::FromWebContents(web_contents());
  if (helper) {
    helper->MaybeShowIntentPickerIcon();
  }
  UpdateProgressAndMaybeAdvance();
}

void WebAppInstallFlowDialogDelegate::OnAcceptCallback(
    bool success,
    std::unique_ptr<WebAppInstallInfo> web_app_info) {
  if (callback_) {
    std::move(callback_).Run(
        success, std::move(web_app_info),
        base::BindOnce(&WebAppInstallFlowDialogDelegate::OnInstallResult,
                       AsWeakPtr()));
  }
}

void WebAppInstallFlowDialogDelegate::OnProgress(
    std::optional<double> percent) {
  timer_percentage_ = percent;
  UpdateProgressAndMaybeAdvance();
}

// Builds and shows an install dialog flow according to the install_type.
void WebAppInstallFlowDialogDelegate::Show(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> install_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    WebAppInstallationAcceptanceCallback callback,
    PwaInProductHelpState iph_state,
    base::WeakPtr<WebAppScreenshotFetcher> screenshot_fetcher,
    bool show_initiating_origin,
    InstallDialogType install_type,
    InstallOsType os_type,
    std::unique_ptr<ProgressDelay> progress_delay) {
  auto* browser_context = web_contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  PrefService* prefs = profile->GetPrefs();
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(browser_context);

  DialogImageInfo dialog_image_info =
      install_info->GetIconBitmapsForSecureSurfaces();

  // For Install Options view, we need a larger icon image.
  gfx::ImageSkia icon_image_80 =
      CreateResizedIconImage(dialog_image_info.bitmaps, kLargeImageSize);

  gfx::ImageSkia icon_image_32(
      std::make_unique<WebAppInfoImageSource>(
          kIconSize, std::move(dialog_image_info.bitmaps)),
      gfx::Size(kIconSize, kIconSize));

  std::u16string title = install_info->title.value();
  GURL start_url = install_info->start_url();
  std::u16string description = install_info->description.value();
  auto delegate = std::make_unique<WebAppInstallFlowDialogDelegate>(
      web_contents, std::move(install_info), std::move(install_tracker),
      std::move(callback), std::move(iph_state), prefs, tracker, install_type,
      os_type, std::move(progress_delay));
  auto delegate_weak_ptr = delegate->AsWeakPtr();

  absl::flat_hash_map<InstallDialogStep, std::unique_ptr<views::View>>
      install_step_to_view;

  // kInstallDialog
  install_step_to_view[InstallDialogStep::kInstallDialog] =
      WebAppInstallIntroView::Create(
          install_type, icon_image_32, title, start_url,
          dialog_image_info.is_maskable, description, screenshot_fetcher,
          base::BindRepeating(
              &WebAppInstallDialogDelegate::OnTextFieldChangedMaybeUpdateButton,
              delegate_weak_ptr));

  // kInstallerOptions
  auto options_view = WebAppInstallOptionsView::Create(
      os_type, title, icon_image_32, icon_image_80,
      dialog_image_info.is_maskable, start_url);
  delegate->options_view_ = options_view->GetWeakPtr();
  install_step_to_view[InstallDialogStep::kInstallerOptions] =
      std::move(options_view);

  // kProgress
  auto progress_view = std::make_unique<WebAppInstallProgressView>();
  auto progress_view_weak_ptr = progress_view->GetWeakPtr();
  install_step_to_view[InstallDialogStep::kProgress] = std::move(progress_view);

  // kSuccessful
  auto successful_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .Build();
  successful_view->AddChildView(WebAppIconNameAndOriginView::Create(
      icon_image_32, title, start_url, dialog_image_info.is_maskable));

  install_step_to_view[InstallDialogStep::kSuccessful] =
      std::move(successful_view);

  auto flow_view =
      std::make_unique<WebAppInstallFlowView>(std::move(install_step_to_view));
  flow_view->SetProperty(views::kElementIdentifierKey,
                         kInstallDialogFlowViewId);
  auto flow_view_weak_ptr = flow_view->GetWeakPtr();
  delegate->SetFlowView(flow_view_weak_ptr);
  delegate->SetProgressView(progress_view_weak_ptr);

  views::View* focusable_view =
      flow_view->GetViewForStep(InstallDialogStep::kInstallDialog);

  auto dialog_model_builder = ui::DialogModel::Builder(std::move(delegate));
  dialog_model_builder.SetInternalName("WebAppInstallFlowDialog")
      .SetAccessibleTitle(
          l10n_util::GetStringUTF16(install_type == InstallDialogType::kDiy
                                        ? IDS_DIY_APP_INSTALL_DIALOG_TITLE
                                        : IDS_INSTALL_PWA_DIALOG_TITLE))
      .SetTitle(
          l10n_util::GetStringUTF16(install_type == InstallDialogType::kDiy
                                        ? IDS_DIY_APP_INSTALL_DIALOG_TITLE
                                        : IDS_INSTALL_PWA_DIALOG_TITLE))
      // TODO(b/473080055): Use a translated string. Should use the correct
      // subtitle if DIY vs simple and detailed like the title above.
      .SetSubtitle(u"Access this site on a dedicated window on your computer")
      .AddExtraButton(
          base::BindRepeating(
              [](base::WeakPtr<WebAppInstallFlowDialogDelegate> delegate,
                 const ui::Event&) {
                if (delegate) {
                  delegate->OnLearnMoreButtonClicked();
                }
              },
              delegate_weak_ptr),
          ui::DialogModel::Button::Params()
              .SetLabel(
                  l10n_util::GetStringUTF16(IDS_LEARN_MORE_MAYBE_TITLE_CASE))
              .SetId(kLearnMoreButtonId))
      .AddOkButton(
          base::BindRepeating(
              [](base::WeakPtr<WebAppInstallFlowDialogDelegate> delegate) {
                return delegate ? delegate->AdvanceToNextStepOrClose() : true;
              },
              delegate_weak_ptr),
          ui::DialogModel::Button::Params()
              // TODO: Change hardcoded string "next" to a localized string.
              .SetLabel(os_type == InstallOsType::kOther
                            ? l10n_util::GetStringUTF16(IDS_INSTALL)
                            : u"Next")
              .SetId(
                  WebAppInstallDialogDelegate::kPwaInstallDialogInstallButton))
      .AddCancelButton(base::BindOnce(&WebAppInstallDialogDelegate::OnCancel,
                                      delegate_weak_ptr),
                       ui::DialogModel::Button::Params().SetId(kCancelButtonId))
      .SetCloseActionCallback(base::BindOnce(
          &WebAppInstallDialogDelegate::OnClose, delegate_weak_ptr))
      .SetDialogDestroyingCallback(base::BindOnce(
          &WebAppInstallDialogDelegate::OnDestroyed, delegate_weak_ptr))
      .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
      .AddCustomField(
          std::make_unique<views::BubbleDialogModelHost::CustomView>(
              std::move(flow_view),
              views::BubbleDialogModelHost::FieldType::kControl,
              focusable_view),
          WebAppInstallFlowDialogDelegate::kInstallDialogFlowViewId);

  if (install_type == InstallDialogType::kDiy) {
    dialog_model_builder.SetInitiallyFocusedField(kInstallDialogFlowViewId);
  }
  if (install_type != InstallDialogType::kDetailed) {
    dialog_model_builder.SetBannerImage(ui::ImageModel::FromImageSkia(
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_WEB_APP_INSTALL_ILLUSTRATION)));
  }

  if (show_initiating_origin) {
    url::Origin initiating_origin =
        web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
    std::u16string origin_url = url_formatter::FormatOriginForSecurityDisplay(
        initiating_origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    dialog_model_builder.SetSubtitle(l10n_util::GetStringFUTF16(
        IDS_INSTALL_PWA_DIALOG_ORIGIN_LABEL, origin_url));
  }

  auto dialog = views::BubbleDialogModelHost::CreateModal(
      dialog_model_builder.Build(), ui::mojom::ModalType::kChild);

  views::Widget* widget = constrained_window::ShowWebModalDialogViews(
      dialog.release(), web_contents);

  if (IsWidgetCurrentSizeSmallerThanPreferredSize(widget)) {
    delegate_weak_ptr->CloseDialogAsIgnored();
    return;
  }
  delegate_weak_ptr->OnWidgetShownStartTracking(widget);
}

}  // namespace web_app
