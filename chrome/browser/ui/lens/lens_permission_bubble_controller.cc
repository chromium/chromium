// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_permission_bubble_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/lens/lens_overlay_theme_utils.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_metrics.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/lens/lens_permission_user_action.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/image_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/compositor/layer.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace lens {

LensPermissionBubbleController::LensPermissionBubbleController(
    tabs::TabInterface& tab_interface,
    PrefService* pref_service,
    LensOverlayInvocationSource invocation_source)
    : invocation_source_(invocation_source),
      tab_interface_(tab_interface),
      pref_service_(pref_service) {
  tab_will_detach_subscription_ = tab_interface_->RegisterWillDetach(
      base::BindRepeating(&LensPermissionBubbleController::TabWillDetach,
                          base::Unretained(this)));
}

LensPermissionBubbleController::~LensPermissionBubbleController() {
  if (HasOpenDialogWidget()) {
    CloseDialogWidget(views::Widget::ClosedReason::kUnspecified);
  }
}

void LensPermissionBubbleController::RequestPermission(
    content::WebContents* web_contents,
    RequestPermissionCallback callback) {
  // Return early if there is already an open dialog widget. Show the dialog if
  // it is not currently visible for some reason.
  if (HasOpenDialogWidget()) {
    if (!dialog_widget_->IsVisible()) {
      dialog_widget_->Show();
    }
    RecordPermissionRequestedToBeShown(false, invocation_source_);
    return;
  }
  RecordPermissionRequestedToBeShown(true, invocation_source_);

  // Observe pref changes. Reset the pref observer in case this method called
  // several times in succession.
  pref_observer_.Reset();
  pref_observer_.Init(pref_service_);
  if (lens::features::IsLensOverlayContextualSearchboxEnabled()) {
    pref_observer_.Add(
        prefs::kLensSharingPageContentEnabled,
        base::BindRepeating(
            &LensPermissionBubbleController::OnPermissionPreferenceUpdated,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    pref_observer_.Add(
        prefs::kLensSharingPageScreenshotEnabled,
        base::BindRepeating(
            &LensPermissionBubbleController::OnPermissionPreferenceUpdated,
            weak_ptr_factory_.GetWeakPtr()));
  }

  dialog_widget_ = ShowDialogWidget(std::move(callback), web_contents);

  // Clip layers to root layer bounds so that they don't render outside of the
  // dialog boundary when the dialog is small.
  // TODO(crbug.com/358379367): this should live in the framework and should
  // clip to the window opaque area. Currently child layers will bleed into the
  // window shadow area.
  dialog_widget_->GetLayer()->SetMasksToBounds(true);
}

std::unique_ptr<views::Widget> LensPermissionBubbleController::ShowDialogWidget(
    RequestPermissionCallback callback,
    content::WebContents* web_contents) {
  // The widget will own `model_host` through DialogDelegate.
  views::BubbleDialogModelHost* model_host =
      views::BubbleDialogModelHost::CreateModal(
          CreateLensPermissionDialogModel(std::move(callback)),
          ui::mojom::ModalType::kChild)
          .release();
  model_host->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  std::unique_ptr<views::Widget> widget =
      tab_interface_->GetTabFeatures()
          ->tab_dialog_manager()
          ->CreateShowDialogAndBlockTabInteraction(model_host);
  widget->MakeCloseSynchronous(
      base::BindOnce(&LensPermissionBubbleController::CloseDialogWidget,
                     base::Unretained(this)));

  views::View* focused_view = model_host->GetInitiallyFocusedView();
  CHECK(focused_view);
  focused_view->RequestFocus();

  return widget;
}

void LensPermissionBubbleController::CloseDialogWidget(
    views::Widget::ClosedReason reason) {
  switch (reason) {
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      RecordPermissionUserAction(LensPermissionUserAction::kAcceptButtonPressed,
                                 invocation_source_);
      break;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      RecordPermissionUserAction(LensPermissionUserAction::kCancelButtonPressed,
                                 invocation_source_);
      break;
    case views::Widget::ClosedReason::kEscKeyPressed:
      RecordPermissionUserAction(LensPermissionUserAction::kEscKeyPressed,
                                 invocation_source_);
      break;
    case views::Widget::ClosedReason::kUnspecified:
    case views::Widget::ClosedReason::kCloseButtonClicked:
    case views::Widget::ClosedReason::kLostFocus:
      break;
  }
  dialog_widget_.reset();
}

std::unique_ptr<ui::DialogModel>
LensPermissionBubbleController::CreateLensPermissionDialogModel(
    RequestPermissionCallback callback) {
  ui::DialogModelLabel::TextReplacement link = ui::DialogModelLabel::CreateLink(
      IDS_LENS_PERMISSION_BUBBLE_DIALOG_LEARN_MORE_LINK,
      base::BindRepeating(
          &LensPermissionBubbleController::OnHelpCenterLinkClicked,
          weak_ptr_factory_.GetWeakPtr()));

  auto description_text =
      lens::features::IsLensOverlayContextualSearchboxEnabled()
          ? ui::DialogModelLabel::CreateWithReplacement(
                IDS_LENS_PERMISSION_BUBBLE_DIALOG_CSB_DESCRIPTION, link)
          : ui::DialogModelLabel::CreateWithReplacement(
                IDS_LENS_PERMISSION_BUBBLE_DIALOG_DESCRIPTION, link);

  return ui::DialogModel::Builder()
      .SetInternalName(kLensPermissionDialogName)
      .SetTitle(
          l10n_util::GetStringUTF16(IDS_LENS_PERMISSION_BUBBLE_DIALOG_TITLE))
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      .SetIcon(ui::ImageModel::FromVectorIcon(vector_icons::kGoogleColorIcon,
                                              ui::kColorIcon, 20))
      .SetBannerImage(ui::ImageModel::FromImageSkia(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LENS_PERMISSION_MODAL_IMAGE)))
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
      .AddParagraph(description_text)
      .AddOkButton(
          base::BindOnce(
              &LensPermissionBubbleController::OnPermissionDialogAccept,
              weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetStringUTF16(
                  IDS_LENS_PERMISSION_BUBBLE_DIALOG_CONTINUE_BUTTON))
              .SetId(kLensPermissionDialogOkButtonElementId)
              .SetStyle(ui::ButtonStyle::kProminent))
      .AddCancelButton(base::DoNothing(),
                       ui::DialogModel::Button::Params()
                           .SetLabel(l10n_util::GetStringUTF16(
                               IDS_LENS_PERMISSION_BUBBLE_DIALOG_CANCEL_BUTTON))
                           .SetId(kLensPermissionDialogCancelButtonElementId)
                           .SetStyle(ui::ButtonStyle::kTonal))
      .Build();
}

bool LensPermissionBubbleController::HasOpenDialogWidget() {
  return dialog_widget_ && !dialog_widget_->IsClosed();
}

void LensPermissionBubbleController::OnHelpCenterLinkClicked(
    const ui::Event& event) {
  RecordPermissionUserAction(LensPermissionUserAction::kLinkOpened,
                             invocation_source_);
  tab_interface_->GetBrowserWindowInterface()->OpenGURL(
      GURL(lens::features::GetLensOverlayHelpCenterURL()),
      ui::DispositionFromEventFlags(event.flags(),
                                    WindowOpenDisposition::NEW_BACKGROUND_TAB));
}

void LensPermissionBubbleController::OnPermissionDialogAccept(
    RequestPermissionCallback callback) {
  // Pref observer is used to close background dialogs on other tabs. Observing
  // the prefs is no longer necessary when the dialog is being closed because
  // the user accepted the dialog.
  pref_observer_.Reset();
  if (lens::features::IsLensOverlayContextualSearchboxEnabled()) {
    pref_service_->SetBoolean(prefs::kLensSharingPageContentEnabled, true);
  }
  pref_service_->SetBoolean(prefs::kLensSharingPageScreenshotEnabled, true);
  // Must close dialog widget before running callback. This ensures that the
  // overlay can show (it can't if there is another modal is showing).
  CloseDialogWidget(views::Widget::ClosedReason::kAcceptButtonClicked);
  callback.Run();
}

void LensPermissionBubbleController::OnPermissionPreferenceUpdated() {
  if (HasOpenDialogWidget()) {
    CloseDialogWidget(views::Widget::ClosedReason::kAcceptButtonClicked);
  }
  pref_observer_.Reset();
}

void LensPermissionBubbleController::TabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  if (reason == tabs::TabInterface::DetachReason::kDelete &&
      HasOpenDialogWidget()) {
    CloseDialogWidget(views::Widget::ClosedReason::kUnspecified);
  }
}

}  // namespace lens
