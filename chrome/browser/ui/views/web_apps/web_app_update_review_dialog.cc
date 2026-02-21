// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_update_review_dialog.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_observer.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/picture_in_picture/scoped_picture_in_picture_occlusion_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_update_identity_view.h"
#include "chrome/browser/web_applications/ui_manager/update_dialog_types.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/class_property.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget_observer.h"

namespace web_app {

namespace {

const int kArrowIconSizeDp = 32;

// The width of the columns left and right of the arrow (containing the name
// of the app (before and after).
const int kIdentityColumnWidth = 170;

int GetDialogTitleMessageId(const WebAppIdentityUpdate& update) {
  switch (update.GetCombinationChangeIndex()) {
    case 0:
      NOTREACHED();
    case WebAppIdentityUpdate::kNameChange:
      return IDS_WEBAPP_UPDATE_DIALOG_TITLE_NAME;
    case WebAppIdentityUpdate::kIconChange:
      return IDS_WEBAPP_UPDATE_DIALOG_TITLE_LOGO;
    case WebAppIdentityUpdate::kNameChange | WebAppIdentityUpdate::kIconChange:
      return IDS_WEBAPP_UPDATE_DIALOG_TITLE_NAME_AND_LOGO;
    case WebAppIdentityUpdate::kUrlChange:
      return IDS_WEBAPP_UPDATE_DIALOG_TITLE_URL;
    case WebAppIdentityUpdate::kNameChange | WebAppIdentityUpdate::kUrlChange:
      return IDS_WEBAPP_UPDATE_DIALOG_TITLE_NAME_AND_URL;
    case WebAppIdentityUpdate::kIconChange | WebAppIdentityUpdate::kUrlChange:
      return IDS_WEBAPP_UPDATE_DIALOG_TITLE_LOGO_AND_URL;
    case WebAppIdentityUpdate::kNameChange | WebAppIdentityUpdate::kIconChange |
        WebAppIdentityUpdate::kUrlChange:
      return IDS_WEBAPP_UPDATE_DIALOG_TITLE_NAME_AND_LOGO_AND_URL;
  }
  NOTREACHED();
}

int GetDialogDescriptionMessageId(const WebAppIdentityUpdate& update) {
  switch (update.GetCombinationChangeIndex()) {
    case 0:
      NOTREACHED();
    case WebAppIdentityUpdate::kNameChange:
    case WebAppIdentityUpdate::kIconChange:
    case WebAppIdentityUpdate::kNameChange | WebAppIdentityUpdate::kIconChange:
    case WebAppIdentityUpdate::kNameChange | WebAppIdentityUpdate::kUrlChange:
    case WebAppIdentityUpdate::kIconChange | WebAppIdentityUpdate::kUrlChange:
    case WebAppIdentityUpdate::kNameChange | WebAppIdentityUpdate::kIconChange |
        WebAppIdentityUpdate::kUrlChange:
      return IDS_WEBAPP_UPDATE_NEW_EXPLANATION;
    case WebAppIdentityUpdate::kUrlChange:
      return IDS_WEBAPP_UPDATE_URL_ONLY_NEW_EXPLANATION;
  }
  NOTREACHED();
}

int GetDialogAcceptMessageId(const WebAppIdentityUpdate& update) {
  switch (update.GetCombinationChangeIndex()) {
    case 0:
      NOTREACHED();
    case WebAppIdentityUpdate::kNameChange:
    case WebAppIdentityUpdate::kIconChange:
    case WebAppIdentityUpdate::kNameChange | WebAppIdentityUpdate::kIconChange:
      return IDS_WEBAPP_UPDATE_REVIEW_ACCEPT_BUTTON;
    case WebAppIdentityUpdate::kNameChange | WebAppIdentityUpdate::kUrlChange:
    case WebAppIdentityUpdate::kIconChange | WebAppIdentityUpdate::kUrlChange:
    case WebAppIdentityUpdate::kNameChange | WebAppIdentityUpdate::kIconChange |
        WebAppIdentityUpdate::kUrlChange:
    case WebAppIdentityUpdate::kUrlChange:
      return IDS_WEBAPP_UPDATE_REVIEW_ACCEPT_BUTTON_MIGRATION;
  }
  NOTREACHED();
}

class UpdateDialogDelegate : public ui::DialogModelDelegate,
                             public PictureInPictureOcclusionObserver,
                             public views::WidgetObserver,
                             public WebAppInstallManagerObserver {
 public:
  UpdateDialogDelegate(const webapps::AppId& app_id,
                       UpdateReviewDialogCallback callback,
                       Browser& browser)
      : app_id_(app_id), callback_(std::move(callback)), browser_(browser) {
    install_manager_observation_.Observe(
        &WebAppProvider::GetForWebApps(browser_->profile())->install_manager());
    browser_->GetBrowserView().SetProperty(kIsPwaUpdateDialogShowingKey, true);
  }
  ~UpdateDialogDelegate() override {
    if (browser_->window()) {
      browser_->GetBrowserView().SetProperty(kIsPwaUpdateDialogShowingKey,
                                             false);
    }
  }

  // Schedule the pending manifest update application, and terminate the dialog.
  void OnAcceptButtonClicked() {
    CHECK(callback_);
    std::move(callback_).Run(WebAppIdentityUpdateResult::kAccept);
  }

  // Close the dialog if the "Ignore" button is clicked after storing that state
  // for the web app.
  void OnIgnoreButtonClicked(const ui::Event& event) {
    CHECK(callback_);
    std::move(callback_).Run(WebAppIdentityUpdateResult::kIgnore);
    CHECK(dialog_model() && dialog_model()->host());
    dialog_model()->host()->Close();
  }

  void OnUninstallButtonClicked() {
    std::move(callback_).Run(WebAppIdentityUpdateResult::kUninstallApp);
  }

  void OnClose(bool is_forced_migration) {
    if (!callback_) {
      return;
    }
    std::move(callback_).Run(
        is_forced_migration ? WebAppIdentityUpdateResult::kCloseApp
                            : WebAppIdentityUpdateResult::kUnexpectedError);
  }
  // This is called when the dialog has been either accepted, cancelled, closed
  // or destroyed without an user-action.
  void OnDestroyed() {
    if (!callback_) {
      return;
    }
    std::move(callback_).Run(WebAppIdentityUpdateResult::kUnexpectedError);
  }

  void OnWidgetShownStartTracking(views::Widget* dialog_widget) {
    occlusion_observation_.Observe(dialog_widget);
    widget_observation_.Observe(dialog_widget);
    extensions::SecurityDialogTracker::GetInstance()->AddSecurityDialog(
        dialog_widget);
  }

  base::WeakPtr<UpdateDialogDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // PictureInPictureOcclusionObserver overrides:
  void OnOcclusionStateChanged(bool occluded) override {
    // If a picture-in-picture window is occluding the dialog, force it to close
    // to prevent spoofing.
    if (occluded) {
      PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
    }
  }

  // views::WidgetObserver overrides:
  void OnWidgetDestroyed(views::Widget* widget) override {
    widget_observation_.Reset();
  }

  // WebAppInstallManagerObserver overrides:
  void OnWebAppWillBeUninstalled(const webapps::AppId& app_id) override {
    if (!dialog_model() || !dialog_model()->host() || !callback_) {
      return;
    }
    if (app_id != app_id_) {
      return;
    }
    // Calling Close() synchronously deletes `this`, so save `callback_` on the
    // stack first.
    auto callback = std::move(callback_);
    dialog_model()->host()->Close();
    std::move(callback).Run(
        WebAppIdentityUpdateResult::kAppUninstalledDuringDialog);
  }

  void OnWebAppInstallManagerDestroyed() override {
    install_manager_observation_.Reset();
  }

 private:
  const webapps::AppId app_id_;
  UpdateReviewDialogCallback callback_;
  raw_ref<Browser> browser_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};
  ScopedPictureInPictureOcclusionObservation occlusion_observation_{this};
  base::WeakPtrFactory<UpdateDialogDelegate> weak_ptr_factory_{this};
};

}  // namespace

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsPwaUpdateDialogShowingKey, false)

DEFINE_ELEMENT_IDENTIFIER_VALUE(kWebAppUpdateReviewDialogAcceptButton);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kWebAppUpdateReviewDialogUninstallButton);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kWebAppUpdateReviewIgnoreButton);

void ShowWebAppReviewUpdateDialog(const webapps::AppId& app_id,
                                  const WebAppIdentityUpdate& update,
                                  Browser* browser,
                                  base::TimeTicks start_time,
                                  UpdateReviewDialogCallback callback) {
  CHECK(!callback.is_null());

  // Abort if a review update dialog is already being shown in this browser.
  if (browser->GetBrowserView().GetProperty(kIsPwaUpdateDialogShowingKey)) {
    std::move(callback).Run(WebAppIdentityUpdateResult::kUnexpectedError);
    return;
  }

  // Some combination of changes should be existing if the update dialog needs
  // to be triggered.
  CHECK_GT(update.GetCombinationChangeIndex(), 0);
  CHECK(AreWebAppsEnabled(browser->profile()));
  bool url_migration_only =
      (update.GetCombinationChangeIndex() == WebAppIdentityUpdate::kUrlChange);

  // Forced migrations can only happen for PWA migrations where the start url
  // changes.
  if (update.is_forced_migration) {
    CHECK(url_migration_only);
  }

  std::unique_ptr<UpdateDialogDelegate> delegate =
      std::make_unique<UpdateDialogDelegate>(app_id, std::move(callback),
                                             *browser);
  auto delegate_weak_ptr = delegate->AsWeakPtr();

  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int distance_related_horizontal = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);

  ui::DialogModel::Builder dialog_model_builder =
      ui::DialogModel::Builder(std::move(delegate));
  dialog_model_builder.SetInternalName("WebAppUpdateReviewDialog")
      .SetTitle(l10n_util::GetStringUTF16(GetDialogTitleMessageId(update)))
      .AddOkButton(base::BindOnce(&UpdateDialogDelegate::OnAcceptButtonClicked,
                                  delegate_weak_ptr),
                   ui::DialogModel::Button::Params()
                       .SetLabel(l10n_util::GetStringUTF16(
                           GetDialogAcceptMessageId(update)))
                       .SetStyle(ui::ButtonStyle::kProminent)
                       .SetId(kWebAppUpdateReviewDialogAcceptButton))
      .AddCancelButton(
          base::BindOnce(&UpdateDialogDelegate::OnUninstallButtonClicked,
                         delegate_weak_ptr),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetStringUTF16(
                  IDS_WEBAPP_UPDATE_REVIEW_UNINSTALL_BUTTON))
              .SetId(kWebAppUpdateReviewDialogUninstallButton))
      .OverrideDefaultButton(ui::mojom::DialogButton::kNone)
      .SetCloseActionCallback(base::BindOnce(&UpdateDialogDelegate::OnClose,
                                             delegate_weak_ptr,
                                             update.is_forced_migration))
      .SetDialogDestroyingCallback(
          base::BindOnce(&UpdateDialogDelegate::OnDestroyed, delegate_weak_ptr))
      .AddParagraph(ui::DialogModelLabel(
          l10n_util::GetStringUTF16(GetDialogDescriptionMessageId(update))))
      .AddCustomField(
          std::make_unique<views::BubbleDialogModelHost::CustomView>(
              views::Builder<views::TableLayoutView>()
                  // Left padding.
                  .AddPaddingColumn(/*horizontal_resize=*/1, /*width=*/0)
                  // The 'before' column
                  .AddColumn(views::LayoutAlignment::kCenter,
                             views::LayoutAlignment::kStart,
                             views::TableLayout::kFixedSize,
                             views::TableLayout::ColumnSize::kFixed,
                             /*fixed_width=*/kIdentityColumnWidth,
                             /*min_width=*/0)
                  // Padding between the 'before' and the 'arrow' column.
                  .AddPaddingColumn(views::TableLayout::kFixedSize,
                                    /*width=*/distance_related_horizontal)
                  // The 'arrow' column.
                  .AddColumn(views::LayoutAlignment::kStretch,
                             views::LayoutAlignment::kCenter,
                             views::TableLayout::kFixedSize,
                             views::TableLayout::ColumnSize::kUsePreferred,
                             /*fixed_width=*/0, /*min_width=*/0)
                  // Padding between the 'arrow' and the 'after' column.
                  .AddPaddingColumn(views::TableLayout::kFixedSize,
                                    /*width=*/distance_related_horizontal)
                  // The 'after' column.
                  .AddColumn(views::LayoutAlignment::kCenter,
                             views::LayoutAlignment::kStart,
                             views::TableLayout::kFixedSize,
                             views::TableLayout::ColumnSize::kFixed,
                             /*fixed_width=*/kIdentityColumnWidth,
                             /*min_width=*/0)
                  // Padding at the right of the dialog.
                  .AddPaddingColumn(/*horizontal_resize=*/1, /*width=*/0)
                  .AddRows(
                      /*n=*/1,
                      /*vertical_resize=*/views::TableLayout::kFixedSize,
                      /*height=*/0)
                  // Using AddChildren() here leads to the evaluation order
                  // reversed on Windows, making it harder to test
                  // consistently.
                  .AddChild(views::Builder<WebAppUpdateIdentityView>(
                      std::make_unique<WebAppUpdateIdentityView>(
                          update.MakeOldIdentity(), url_migration_only,
                          update.HasTitleChange())))
                  .AddChild(views::Builder<views::ImageView>().SetImage(
                      ui::ImageModel::FromVectorIcon(
                          vector_icons::kForwardArrowIcon, ui::kColorIcon,
                          kArrowIconSizeDp)))
                  .AddChild(views::Builder<WebAppUpdateIdentityView>(
                      std::make_unique<WebAppUpdateIdentityView>(
                          update.MakeNewIdentity(), url_migration_only,
                          update.HasTitleChange())))
                  .SetMinimumSize(gfx::Size(
                      layout_provider->GetDistanceMetric(
                          views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH),
                      /*height=*/200))
                  .Build(),
              views::BubbleDialogModelHost::FieldType::kText));

  // Add the `ignore` button only if the migration is not forced.
  if (!update.is_forced_migration) {
    dialog_model_builder
        .AddExtraButton(
            base::BindRepeating(&UpdateDialogDelegate::OnIgnoreButtonClicked,
                                delegate_weak_ptr),
            ui::DialogModel::Button::Params()
                .SetLabel(l10n_util::GetStringUTF16(
                    IDS_WEBAPP_UPDATE_REVIEW_IGNORE_BUTTON))
                .SetId(kWebAppUpdateReviewIgnoreButton))
        .SetInitiallyFocusedField(kWebAppUpdateReviewIgnoreButton);
  }

  views::Widget* widget = constrained_window::ShowBrowserModal(
      dialog_model_builder.Build(), browser->window()->GetNativeWindow());
  delegate_weak_ptr->OnWidgetShownStartTracking(widget);

  base::UmaHistogramTimes("WebApp.UpdateReviewDialog.TriggerToShowTime",
                          base::TimeTicks::Now() - start_time);
  base::RecordAction(
      base::UserMetricsAction("PredictableAppUpdateDialogShown"));
}

}  // namespace web_app
