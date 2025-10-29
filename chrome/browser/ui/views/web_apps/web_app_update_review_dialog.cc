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
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_update_identity_view.h"
#include "chrome/browser/web_applications/commands/apply_pending_manifest_update_command.h"
#include "chrome/browser/web_applications/ui_manager/update_dialog_types.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/keep_alive_registry/keep_alive_types.h"
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
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget_observer.h"

namespace web_app {

namespace {

const int kArrowIconSizeDp = 32;

// The width of the columns left and right of the arrow (containing the name
// of the app (before and after).
const int kIdentityColumnWidth = 170;

int GetDialogTitleMessageId(const WebAppIdentityUpdate& update) {
  bool title_change = update.new_title.has_value();
  bool icon_change = update.new_icon.has_value();
  bool url_migration = update.new_start_url.has_value();

  const int kNameChange = 0b001;
  const int kIconChange = 0b010;
  const int kUrlChange = 0b100;
  int combination_index = (title_change ? kNameChange : 0) |
                          (icon_change ? kIconChange : 0) |
                          (url_migration ? kUrlChange : 0);
  switch (combination_index) {
    case 0:
      NOTREACHED();
    case kNameChange:
      return IDS_WEBAPP_UPDATE_DIALOG_TITLE_NAME;
    case kIconChange:
      return IDS_WEBAPP_UPDATE_DIALOG_TITLE_LOGO;
    case kNameChange | kIconChange:
      return IDS_WEBAPP_UPDATE_DIALOG_TITLE_NAME_AND_LOGO;
    case kUrlChange:
      return IDS_WEBAPP_UPDATE_DIALOG_TITLE_URL;
    case kNameChange | kUrlChange:
      return IDS_WEBAPP_UPDATE_DIALOG_TITLE_NAME_AND_URL;
    case kIconChange | kUrlChange:
      return IDS_WEBAPP_UPDATE_DIALOG_TITLE_LOGO_AND_URL;
    case kNameChange | kIconChange | kUrlChange:
      return IDS_WEBAPP_UPDATE_DIALOG_TITLE_NAME_AND_LOGO_AND_URL;
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
    web_app_provider_ = WebAppProvider::GetForWebApps(browser_->profile());
    install_manager_observation_.Observe(&web_app_provider_->install_manager());
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
    CHECK(web_app_provider_);
    CHECK(callback_);
    CHECK(!browser_->profile()->IsOffTheRecord());
    auto profile_keep_alive = ScopedProfileKeepAlive::TryAcquire(
        browser_->profile(), ProfileKeepAliveOrigin::kWebAppUpdate);
    if (!profile_keep_alive) {
      // Profile is scheduled for destruction, abort.
      std::move(callback_).Run(WebAppIdentityUpdateResult::kUnexpectedError);
      return;
    }
    auto keep_alive = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::APP_MANIFEST_UPDATE, KeepAliveRestartOption::DISABLED);
    web_app_provider_->scheduler().ScheduleApplyPendingManifestUpdate(
        app_id_, std::move(keep_alive), std::move(profile_keep_alive),
        base::DoNothing());
    std::move(callback_).Run(WebAppIdentityUpdateResult::kAccept);
  }

  // Close the dialog if the "Ignore" button is clicked after storing that state
  // for the web app.
  void OnIgnoreButtonClicked(const ui::Event& event) {
    CHECK(web_app_provider_);
    CHECK(callback_);
    web_app_provider_->scheduler().MarkAppPendingUpdateAsIgnored(
        app_id_, base::BindOnce(std::move(callback_),
                                WebAppIdentityUpdateResult::kIgnore));
    CHECK(dialog_model() && dialog_model()->host());
    dialog_model()->host()->Close();
  }

  void OnUninstallButtonClicked() {
    CHECK(web_app_provider_);
    web_app_provider_->ui_manager().PresentUserUninstallDialog(
        app_id_, webapps::WebappUninstallSource::kAppMenu, browser_->window(),
        base::DoNothing());
    std::move(callback_).Run(WebAppIdentityUpdateResult::kUninstallApp);
  }

  void OnClose() {
    // This should not be called, but due to lack of clarity with UI framework
    // assumptions, we should still handle this even if we asked for the close
    // button to be hidden.
    if (!callback_) {
      return;
    }
    std::move(callback_).Run(WebAppIdentityUpdateResult::kUnexpectedError);
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
    if (!dialog_model() || !dialog_model()->host()) {
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
  raw_ptr<WebAppProvider> web_app_provider_ = nullptr;
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

  bool title_change = update.new_title.has_value();
  bool icon_change = update.new_icon.has_value();
  bool migration = update.new_start_url.has_value();
  CHECK(title_change || icon_change || migration);
  int title = GetDialogTitleMessageId(update);

  CHECK(AreWebAppsEnabled(browser->profile()));

  std::unique_ptr<UpdateDialogDelegate> delegate =
      std::make_unique<UpdateDialogDelegate>(app_id, std::move(callback),
                                             *browser);
  auto delegate_weak_ptr = delegate->AsWeakPtr();

  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int distance_related_horizontal = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);

  std::unique_ptr<ui::DialogModel> dialog_model =
      ui::DialogModel::Builder(std::move(delegate))
          .SetInternalName("WebAppUpdateReviewDialog")
          .SetTitle(l10n_util::GetStringUTF16(title))
          .AddOkButton(
              base::BindOnce(&UpdateDialogDelegate::OnAcceptButtonClicked,
                             delegate_weak_ptr),
              ui::DialogModel::Button::Params()
                  .SetLabel(l10n_util::GetStringUTF16(
                      IDS_WEBAPP_UPDATE_REVIEW_ACCEPT_BUTTON))
                  .SetStyle(ui::ButtonStyle::kProminent)
                  .SetId(kWebAppUpdateReviewDialogAcceptButton))
          .AddCancelButton(
              base::BindOnce(&UpdateDialogDelegate::OnUninstallButtonClicked,
                             delegate_weak_ptr),
              ui::DialogModel::Button::Params()
                  .SetLabel(l10n_util::GetStringUTF16(
                      IDS_WEBAPP_UPDATE_REVIEW_UNINSTALL_BUTTON))
                  .SetId(kWebAppUpdateReviewDialogUninstallButton))
          .AddExtraButton(
              base::BindRepeating(&UpdateDialogDelegate::OnIgnoreButtonClicked,
                                  delegate_weak_ptr),
              ui::DialogModel::Button::Params()
                  .SetLabel(l10n_util::GetStringUTF16(
                      IDS_WEBAPP_UPDATE_REVIEW_IGNORE_BUTTON))
                  .SetId(kWebAppUpdateReviewIgnoreButton))
          .OverrideDefaultButton(ui::mojom::DialogButton::kNone)
          .SetInitiallyFocusedField(kWebAppUpdateReviewIgnoreButton)
          .SetCloseActionCallback(
              base::BindOnce(&UpdateDialogDelegate::OnClose, delegate_weak_ptr))
          .SetDialogDestroyingCallback(base::BindOnce(
              &UpdateDialogDelegate::OnDestroyed, delegate_weak_ptr))
          .AddParagraph(ui::DialogModelLabel(
              l10n_util::GetStringUTF16(IDS_WEBAPP_UPDATE_NEW_EXPLANATION)))

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
                              update.MakeOldIdentity())))
                      .AddChild(views::Builder<views::ImageView>().SetImage(
                          ui::ImageModel::FromVectorIcon(
                              vector_icons::kForwardArrowIcon, ui::kColorIcon,
                              kArrowIconSizeDp)))
                      .AddChild(views::Builder<WebAppUpdateIdentityView>(
                          std::make_unique<WebAppUpdateIdentityView>(
                              update.MakeNewIdentity())))
                      .SetMinimumSize(gfx::Size(
                          layout_provider->GetDistanceMetric(
                              views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH),
                          /*height=*/200))
                      .Build(),
                  views::BubbleDialogModelHost::FieldType::kText))
          .Build();

  views::Widget* widget = constrained_window::ShowBrowserModal(
      std::move(dialog_model), browser->window()->GetNativeWindow());
  delegate_weak_ptr->OnWidgetShownStartTracking(widget);

  base::UmaHistogramTimes("WebApp.UpdateReviewDialog.TriggerToShowTime",
                          base::TimeTicks::Now() - start_time);
  base::RecordAction(
      base::UserMetricsAction("PredictableAppUpdateDialogShown"));
}

}  // namespace web_app
