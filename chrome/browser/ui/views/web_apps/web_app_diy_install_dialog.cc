// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_coordinator.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_views_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/metrics/structured/event_logging_features.h"
// TODO(crbug/1125897): Enable gn check once it learns about conditional
// includes.
#include "components/metrics/structured/structured_events.h"  // nogncheck
#include "components/metrics/structured/structured_metrics_client.h"  // nogncheck
#endif

namespace {

std::u16string GetTrimmedAppTitle(std::u16string app_title) {
  base::TrimWhitespace(app_title, base::TRIM_ALL, &app_title);
  return app_title;
}

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOkButtonId);

class DiyAppDialogIconNameAndOriginView : public views::View,
                                          views::TextfieldController {
  METADATA_HEADER(DiyAppDialogIconNameAndOriginView, views::View)
 public:
  static std::unique_ptr<DiyAppDialogIconNameAndOriginView> Create(
      const gfx::ImageSkia& icon_image,
      std::u16string app_title,
      const GURL& start_url,
      ui::DialogModel* dialog_model,
      content::WebContents* web_contents,
      web_app::DiyAppTitleFieldTextTracker text_tracker) {
    return base::WrapUnique(new DiyAppDialogIconNameAndOriginView(
        icon_image, app_title, start_url, dialog_model, web_contents,
        text_tracker));
  }

  ~DiyAppDialogIconNameAndOriginView() override = default;

 private:
  DiyAppDialogIconNameAndOriginView(
      const gfx::ImageSkia& icon_image,
      std::u16string app_title,
      const GURL& start_url,
      ui::DialogModel* dialog_model,
      content::WebContents* web_contents,
      web_app::DiyAppTitleFieldTextTracker text_tracker)
      : dialog_model_(dialog_model),
        web_contents_(web_contents),
        text_tracker_(text_tracker) {
    const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

    const int textfield_width = 320;
    auto* layout = SetLayoutManager(std::make_unique<views::TableLayout>());
    layout
        ->AddColumn(views::LayoutAlignment::kStretch,
                    views::LayoutAlignment::kCenter,
                    views::TableLayout::kFixedSize,
                    views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
        .AddPaddingColumn(views::TableLayout::kFixedSize,
                          layout_provider->GetDistanceMetric(
                              views::DISTANCE_RELATED_CONTROL_HORIZONTAL))
        .AddColumn(views::LayoutAlignment::kStretch,
                   views::LayoutAlignment::kCenter,
                   views::TableLayout::kFixedSize,
                   views::TableLayout::ColumnSize::kFixed, textfield_width, 0)
        .AddRows(1, views::TableLayout::kFixedSize)
        .AddPaddingRow(views::TableLayout::kFixedSize,
                       layout_provider->GetDistanceMetric(
                           views::DISTANCE_RELATED_CONTROL_VERTICAL))
        .AddRows(1, views::TableLayout::kFixedSize);

    auto icon_view = std::make_unique<views::ImageView>();
    icon_view->SetImage(ui::ImageModel::FromImageSkia(icon_image));
    AddChildView(icon_view.release());

    text_tracker_->data = web_app::NormalizeSuggestedAppTitle(app_title);

    AddChildView(views::Builder<views::Textfield>()
                     .CopyAddressTo(&title_field_)
                     .SetText(text_tracker_->data)
                     .SetAccessibleName(l10n_util::GetStringUTF16(
                         IDS_DIY_APP_AX_BUBBLE_NAME_LABEL))
                     .SetController(this)
                     .Build());

    // Skip the first column in the 2nd row, that is the area below the icon and
    // should stay empty.
    AddChildView(views::Builder<views::View>().Build());

    AddChildView(
        web_app::CreateOriginLabelFromStartUrl(start_url,
                                               /*is_primary_text=*/false)
            .release());

    title_field_->SetID(web_app::kTextFieldId);
    title_field_->SelectAll(true);
  }

  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override {
    CHECK_EQ(sender, title_field_);
    std::u16string trimmed_title = GetTrimmedAppTitle(new_contents);
    auto* ok_button = dialog_model_->GetButtonByUniqueId(kOkButtonId);
    bool ok_button_currently_enabled = ok_button->is_enabled();
    bool current_string_empty = trimmed_title.empty();

    text_tracker_->data = std::move(trimmed_title);

    if (ok_button_currently_enabled && current_string_empty) {
      dialog_model_->SetButtonEnabled(ok_button, /*enabled=*/false);
    } else if (!ok_button_currently_enabled && !current_string_empty) {
      dialog_model_->SetButtonEnabled(ok_button, /*enabled=*/true);
    }

    // TODO(crbug.com/328588659): This shouldn't be needed but we need to undo
    // any position changes that are currently incorrectly caused by a
    // SizeToContents() call, leading to the dialog being anchored off screen
    // from the Chrome window.
    Browser* browser = chrome::FindBrowserWithTab(web_contents_);
    CHECK(browser);
    web_app::WebAppInstallDialogCoordinator* coordinator =
        web_app::WebAppInstallDialogCoordinator::FromBrowser(browser);
    CHECK(coordinator);

    constrained_window::UpdateWebContentsModalDialogPosition(
        coordinator->GetBubbleView()->GetWidget(),
        web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_)
            ->delegate()
            ->GetWebContentsModalDialogHost());
  }

  raw_ptr<views::Textfield> title_field_ = nullptr;
  raw_ptr<ui::DialogModel> dialog_model_ = nullptr;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  web_app::DiyAppTitleFieldTextTracker text_tracker_;
};

BEGIN_METADATA(DiyAppDialogIconNameAndOriginView)
END_METADATA

#if BUILDFLAG(IS_CHROMEOS)
namespace cros_events = metrics::structured::events::v2::cr_os_events;
#endif

}  // namespace

namespace web_app {

void ShowDiyAppInstallDialog(
    content::WebContents* web_contents,
    std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback,
    PwaInProductHelpState iph_state) {
  CHECK(web_app_info->is_diy_app);
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  WebAppInstallDialogCoordinator* dialog_coordinator =
      WebAppInstallDialogCoordinator::GetOrCreateForBrowser(browser);
  if (dialog_coordinator->IsShowing()) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  auto* browser_context = web_contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  PrefService* prefs = profile->GetPrefs();

#if BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(metrics::structured::kAppDiscoveryLogging)) {
    webapps::AppId app_id =
        web_app::GenerateAppIdFromManifestId(web_app_info->manifest_id);
    metrics::structured::StructuredMetricsClient::Record(
        cros_events::AppDiscovery_Browser_AppInstallDialogShown().SetAppId(
            app_id));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(browser_context);

  gfx::ImageSkia icon_image(std::make_unique<WebAppInfoImageSource>(
                                kIconSize, web_app_info->icon_bitmaps.any),
                            gfx::Size(kIconSize, kIconSize));
  GURL start_url = web_app_info->start_url;

  // Fallback to using the document title if the web_app_info->title is not
  // populated, as the document title is always guaranteed to exist.
  std::u16string app_name = web_app_info->title;
  if (app_name.empty()) {
    app_name = UrlIdentity::CreateFromUrl(profile, start_url,
                                          {UrlIdentity::Type::kDefault}, {})
                   .name;
  }

  DiyAppTitleFieldTextTracker data =
      base::MakeRefCounted<base::RefCountedData<std::u16string>>();

  auto delegate = std::make_unique<web_app::WebAppInstallDialogDelegate>(
      web_contents, std::move(web_app_info), std::move(install_tracker),
      std::move(callback), std::move(iph_state), prefs, tracker,
      InstallDialogType::kDiy, data);
  auto delegate_weak_ptr = delegate->AsWeakPtr();

  auto dialog_model =
      ui::DialogModel::Builder(std::move(delegate))
          .SetInternalName("WebAppDiyInstallDialog")
          .SetTitle(l10n_util::GetStringUTF16(IDS_DIY_APP_INSTALL_DIALOG_TITLE))
          .SetSubtitle(
              l10n_util::GetStringUTF16(IDS_DIY_APP_INSTALL_DIALOG_SUBTITLE))
          .AddOkButton(base::BindOnce(&WebAppInstallDialogDelegate::OnAccept,
                                      delegate_weak_ptr),
                       ui::DialogModel::Button::Params()
                           .SetLabel(l10n_util::GetStringUTF16(IDS_INSTALL))
                           .SetId(kOkButtonId))
          .AddCancelButton(base::BindOnce(
              &WebAppInstallDialogDelegate::OnCancel, delegate_weak_ptr))
          .SetCloseActionCallback(base::BindOnce(
              &WebAppInstallDialogDelegate::OnClose, delegate_weak_ptr))
          .SetDialogDestroyingCallback(base::BindOnce(
              &WebAppInstallDialogDelegate::OnClose, delegate_weak_ptr))
          .OverrideDefaultButton(ui::DialogButton::DIALOG_BUTTON_NONE)
          .Build();

  auto* dialog_model_ptr = dialog_model.get();
  dialog_model->AddCustomField(
      std::make_unique<views::BubbleDialogModelHost::CustomView>(
          DiyAppDialogIconNameAndOriginView::Create(icon_image, app_name,
                                                    start_url, dialog_model_ptr,
                                                    web_contents, data),
          views::BubbleDialogModelHost::FieldType::kControl));

  auto dialog = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ui::MODAL_TYPE_CHILD);

  views::BubbleDialogDelegate* dialog_delegate =
      dialog->AsBubbleDialogDelegate();
  constrained_window::ShowWebModalDialogViews(dialog.release(), web_contents);
  dialog_coordinator->StartTracking(dialog_delegate);

  base::RecordAction(base::UserMetricsAction("WebAppDiyInstallShown"));
}

}  // namespace web_app
