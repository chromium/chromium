// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/simple_web_view_dialog.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_password_reuse_detection_manager_client.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/ash/login/captive_portal_window_proxy.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "ipc/ipc_message.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

using ::content::WebContents;

// Margin between screen edge and SimpleWebViewDialog border.
const int kExternalMargin = 60;

// Margin between WebView and SimpleWebViewDialog border.
const int kInnerMargin = 2;

class ToolbarRowView : public views::View {
  METADATA_HEADER(ToolbarRowView, views::View)

 public:
  ToolbarRowView() {
    SetBackground(
        views::CreateThemedSolidBackground(ui::kColorDialogBackground));
  }

  ToolbarRowView(const ToolbarRowView&) = delete;
  ToolbarRowView& operator=(const ToolbarRowView&) = delete;
  ~ToolbarRowView() override = default;

  void Init(std::unique_ptr<views::View> back,
            std::unique_ptr<views::View> forward,
            std::unique_ptr<views::View> reload,
            std::unique_ptr<views::View> location_bar) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout->set_between_child_spacing(
        views::LayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_HORIZONTAL));

    AddChildView(std::move(back));
    AddChildView(std::move(forward));
    AddChildView(std::move(reload));
    layout->SetFlexForView(AddChildView(std::move(location_bar)), 1);
  }
};

BEGIN_METADATA(ToolbarRowView)
END_METADATA

}  // namespace

// Stub implementation of ContentSettingBubbleModelDelegate.
class StubBubbleModelDelegate : public ContentSettingBubbleModelDelegate {
 public:
  StubBubbleModelDelegate() = default;
  StubBubbleModelDelegate(const StubBubbleModelDelegate&) = delete;
  StubBubbleModelDelegate& operator=(const StubBubbleModelDelegate&) = delete;
  ~StubBubbleModelDelegate() override = default;

 private:
  // ContentSettingBubbleModelDelegate implementation:
  void ShowCollectedCookiesDialog(content::WebContents* web_contents) override {
  }
  void ShowContentSettingsPage(ContentSettingsType type) override {}
  void ShowMediaSettingsPage() override {}
  void ShowLearnMorePage(ContentSettingsType type) override {}
};

// SimpleWebViewDialog class ---------------------------------------------------

SimpleWebViewDialog::SimpleWebViewDialog(Profile* profile)
    : profile_(profile), bubble_model_delegate_(new StubBubbleModelDelegate) {
  command_updater_ = std::make_unique<CommandUpdaterImpl>(this);
  command_updater_->UpdateCommandEnabled(IDC_BACK, true);
  command_updater_->UpdateCommandEnabled(IDC_FORWARD, true);
  command_updater_->UpdateCommandEnabled(IDC_STOP, true);
  command_updater_->UpdateCommandEnabled(IDC_RELOAD, true);
  command_updater_->UpdateCommandEnabled(IDC_RELOAD_BYPASSING_CACHE, true);
  command_updater_->UpdateCommandEnabled(IDC_RELOAD_CLEARING_CACHE, true);
}

SimpleWebViewDialog::~SimpleWebViewDialog() {
  for (auto& observer : observers_) {
    observer.OnHostDestroying();
  }

  if (web_view_ && web_view_->web_contents()) {
    web_view_->web_contents()->SetDelegate(nullptr);
    web_modal::WebContentsModalDialogManager::FromWebContents(
        web_view_->web_contents())
        ->SetDelegate(nullptr);
  }
}

void SimpleWebViewDialog::StartLoad(const GURL& url) {
  if (!web_view_container_) {
    web_view_container_ = std::make_unique<views::WebView>(profile_);
  }
  web_view_ = web_view_container_.get();
  web_view_->GetWebContents()->SetDelegate(this);
  web_view_->LoadInitialURL(url,
                            views::WebView::HttpsUpgradePolicy::kNoUpgrade);

  WebContents* web_contents = web_view_->GetWebContents();
  DCHECK(web_contents);

  // Create the password manager that is needed for the proxy.
  autofill::ChromeAutofillClient::CreateForWebContents(web_contents);
  ChromePasswordManagerClient::CreateForWebContents(web_contents);

  // Create the password reuse detection manager for simple web view dialog.
  ChromePasswordReuseDetectionManagerClient::CreateForWebContents(web_contents);

  // Set this as the web modal delegate so that web dialog can appear.
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents);
  web_modal::WebContentsModalDialogManager::FromWebContents(web_contents)
      ->SetDelegate(this);
}

void SimpleWebViewDialog::Init() {
  // Create the security state model that the location bar model needs.
  if (web_view_->GetWebContents()) {
    ChromeSecurityStateTabHelper::CreateForWebContents(
        web_view_->GetWebContents());
  }
  location_bar_model_ = std::make_unique<LocationBarModelImpl>(
      this, content::kMaxURLDisplayChars);

  SetBackground(views::CreateThemedSolidBackground(ui::kColorDialogBackground));

  // Back/Forward buttons.
  auto back = std::make_unique<views::ImageButton>(base::BindRepeating(
      [](CommandUpdater* updater) { updater->ExecuteCommand(IDC_BACK); },
      command_updater_.get()));
  back->SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                                 ui::EF_MIDDLE_MOUSE_BUTTON);
  back->SetImageHorizontalAlignment(views::ImageButton::ALIGN_RIGHT);
  back->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_BACK));
  back->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  back->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back->SetID(VIEW_ID_BACK_BUTTON);
  back_ = back.get();

  auto forward = std::make_unique<views::ImageButton>(base::BindRepeating(
      [](CommandUpdater* updater) { updater->ExecuteCommand(IDC_FORWARD); },
      command_updater_.get()));
  forward->SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                                    ui::EF_MIDDLE_MOUSE_BUTTON);
  forward->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_FORWARD));
  forward->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  forward->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_FORWARD));
  forward->SetID(VIEW_ID_FORWARD_BUTTON);
  forward_ = forward.get();

  // Location bar.
  auto location_bar = std::make_unique<LocationBarView>(
      nullptr, profile_, command_updater_.get(), this, true);
  location_bar_ = location_bar.get();

  // Reload button.
  auto reload = std::make_unique<ReloadButton>(command_updater_.get());
  reload->SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                                   ui::EF_MIDDLE_MOUSE_BUTTON);
  reload->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_RELOAD));
  reload->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_RELOAD));
  reload->SetID(VIEW_ID_RELOAD_BUTTON);
  reload_ = reload.get();

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->SetOrientation(views::BoxLayout::Orientation::kVertical);

  // Use separate view to setup custom background.
  auto toolbar_row = std::make_unique<ToolbarRowView>();
  toolbar_row->Init(std::move(back), std::move(forward), std::move(reload),
                    std::move(location_bar));
  AddChildView(std::move(toolbar_row));

  web_view_container_->SetProperty(views::kMarginsKey,
                                   gfx::Insets(kInnerMargin));
  layout->SetFlexForView(AddChildView(std::move(web_view_container_)), 1);

  LoadImages();

  location_bar_->Init();
  UpdateReload(web_view_->web_contents()->IsLoading(), true);

  gfx::Rect screen_bounds = CalculateScreenBounds(gfx::Size());
  screen_bounds.Inset(kExternalMargin);
  SetPreferredSize(screen_bounds.size());
}

content::WebContents* SimpleWebViewDialog::OpenURL(
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  // As there are no Browsers right now, this could not actually ever work.
  NOTIMPLEMENTED();
  return nullptr;
}

void SimpleWebViewDialog::NavigationStateChanged(
    WebContents* source,
    content::InvalidateTypes changed_flags) {
  if (location_bar_) {
    location_bar_->Update(nullptr);
    UpdateButtons();
  }
}

void SimpleWebViewDialog::LoadingStateChanged(WebContents* source,
                                              bool should_show_loading_ui) {
  bool is_loading = source->IsLoading();
  UpdateReload(is_loading && should_show_loading_ui, false);
  command_updater_->UpdateCommandEnabled(IDC_STOP, is_loading);
}

WebContents* SimpleWebViewDialog::GetWebContents() {
  return nullptr;
}

LocationBarModel* SimpleWebViewDialog::GetLocationBarModel() {
  return location_bar_model_.get();
}

const LocationBarModel* SimpleWebViewDialog::GetLocationBarModel() const {
  return location_bar_model_.get();
}

ContentSettingBubbleModelDelegate*
SimpleWebViewDialog::GetContentSettingBubbleModelDelegate() {
  return bubble_model_delegate_.get();
}

content::WebContents* SimpleWebViewDialog::GetActiveWebContents() const {
  return web_view_->web_contents();
}

void SimpleWebViewDialog::ExecuteCommandWithDisposition(int id,
                                                        WindowOpenDisposition) {
  WebContents* web_contents = web_view_->web_contents();
  switch (id) {
    case IDC_BACK:
      if (web_contents->GetController().CanGoBack()) {
        location_bar_->Revert();
        web_contents->GetController().GoBack();
      }
      break;
    case IDC_FORWARD:
      if (web_contents->GetController().CanGoForward()) {
        location_bar_->Revert();
        web_contents->GetController().GoForward();
      }
      break;
    case IDC_STOP:
      web_contents->Stop();
      break;
    case IDC_RELOAD:
    // Always reload bypassing cache.
    case IDC_RELOAD_BYPASSING_CACHE:
    case IDC_RELOAD_CLEARING_CACHE:
      location_bar_->Revert();
      web_contents->GetController().Reload(content::ReloadType::BYPASSING_CACHE,
                                           true);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

std::unique_ptr<views::WidgetDelegate>
SimpleWebViewDialog::MakeWidgetDelegate() {
  auto delegate = std::make_unique<views::WidgetDelegate>();
  delegate->SetInitiallyFocusedView(web_view_);
  delegate->SetOwnedByWidget(true);
  return delegate;
}

void SimpleWebViewDialog::LoadImages() {
  const ui::ThemeProvider* tp = GetThemeProvider();

  auto set_image_model = [=](views::ImageButton* button,
                             views::Button::ButtonState state, int idr) {
    gfx::ImageSkia* image = tp->GetImageSkiaNamed(idr);
    button->SetImageModel(state, image ? ui::ImageModel::FromImageSkia(*image)
                                       : ui::ImageModel());
  };

  set_image_model(back_, views::Button::STATE_NORMAL, IDR_BACK);
  set_image_model(back_, views::Button::STATE_HOVERED, IDR_BACK_H);
  set_image_model(back_, views::Button::STATE_PRESSED, IDR_BACK_P);
  set_image_model(back_, views::Button::STATE_DISABLED, IDR_BACK_D);

  set_image_model(forward_, views::Button::STATE_NORMAL, IDR_FORWARD);
  set_image_model(forward_, views::Button::STATE_HOVERED, IDR_FORWARD_H);
  set_image_model(forward_, views::Button::STATE_PRESSED, IDR_FORWARD_P);
  set_image_model(forward_, views::Button::STATE_DISABLED, IDR_FORWARD_D);
}

void SimpleWebViewDialog::UpdateButtons() {
  content::NavigationController& navigation_controller =
      web_view_->web_contents()->GetController();
  back_->SetEnabled(navigation_controller.CanGoBack());
  forward_->SetEnabled(navigation_controller.CanGoForward());
}

void SimpleWebViewDialog::UpdateReload(bool is_loading, bool force) {
  if (reload_) {
    reload_->ChangeMode(
        is_loading ? ReloadButton::Mode::kStop : ReloadButton::Mode::kReload,
        force);
  }
}

web_modal::WebContentsModalDialogHost*
SimpleWebViewDialog::GetWebContentsModalDialogHost() {
  return this;
}

gfx::NativeView SimpleWebViewDialog::GetHostView() const {
  return GetWidget()->GetNativeWindow();
}

gfx::Point SimpleWebViewDialog::GetDialogPosition(const gfx::Size& size) {
  const gfx::Size& host_size = this->size();
  return gfx::Point(host_size.width() / 2 - size.width() / 2,
                    host_size.height() / 2 - size.height() / 2);
}

gfx::Size SimpleWebViewDialog::GetMaximumDialogSize() {
  return size();
}

void SimpleWebViewDialog::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observers_.AddObserver(observer);
}

void SimpleWebViewDialog::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observers_.RemoveObserver(observer);
}

BEGIN_METADATA(SimpleWebViewDialog)
END_METADATA

}  // namespace ash
