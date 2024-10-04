// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/signin_view_controller_delegate_views.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/signin/profile_customization_ui.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

const int kModalDialogWidth = 448;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
const int kManagedUserNoticeConfirmationDialogWidth = 512;
const int kManagedUserNoticeConfirmationDialogHeight = 576;
const int kManagedUserNoticeConfirmationUpdatedDialogWidth = 780;
const int kManagedUserNoticeConfirmationUpdatedDialogHeight = 560;
#endif
const int kSyncConfirmationDialogWidth = 512;
const int kSyncConfirmationDialogHeight = 487;
const int kSigninErrorDialogHeight = 164;

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
const int kReauthDialogWidth = 540;
const int kReauthDialogHeight = 520;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

int GetSyncConfirmationDialogPreferredHeight(Profile* profile) {
  // If sync is disabled, then the sync confirmation dialog looks like an error
  // dialog and thus it has the same preferred size.
  return SyncServiceFactory::IsSyncAllowed(profile)
             ? kSyncConfirmationDialogHeight
             : kSigninErrorDialogHeight;
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
void CloseModalSigninInBrowser(
    base::WeakPtr<Browser> browser,
    bool show_profile_switch_iph,
    ProfileCustomizationHandler::CustomizationResult result) {
  if (!browser)
    return;

  browser->signin_view_controller()->CloseModalSignin();

  if (show_profile_switch_iph) {
    browser->window()->MaybeShowProfileSwitchIPH();
  }
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

// This layout auto-resizes the host view widget to always adapt to changes in
// the size of the child views.
class WidgetAutoResizingLayout : public views::FillLayout {
 public:
  WidgetAutoResizingLayout() = default;

 private:
  // views::FillLayout:
  void OnLayoutChanged() override {
    FillLayout::OnLayoutChanged();
    if (views::Widget* widget = host_view()->GetWidget(); widget) {
      widget->SetSize(widget->non_client_view()->GetPreferredSize());
    }
  }
};

}  // namespace

// static
std::unique_ptr<views::WebView>
SigninViewControllerDelegateViews::CreateSyncConfirmationWebView(
    Browser* browser,
    SyncConfirmationStyle style,
    bool is_sync_promo) {
  GURL url = GURL(chrome::kChromeUISyncConfirmationURL);
  return CreateDialogWebView(
      browser, AppendSyncConfirmationQueryParams(url, style, is_sync_promo),
      GetSyncConfirmationDialogPreferredHeight(browser->profile()),
      kSyncConfirmationDialogWidth, InitializeSigninWebDialogUI(true));
}

// static
std::unique_ptr<views::WebView>
SigninViewControllerDelegateViews::CreateSigninErrorWebView(Browser* browser) {
  return CreateDialogWebView(browser, GURL(chrome::kChromeUISigninErrorURL),
                             kSigninErrorDialogHeight, std::nullopt,
                             InitializeSigninWebDialogUI(true));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
// static
std::unique_ptr<views::WebView>
SigninViewControllerDelegateViews::CreateReauthConfirmationWebView(
    Browser* browser,
    signin_metrics::ReauthAccessPoint access_point) {
  return CreateDialogWebView(browser, GetReauthConfirmationURL(access_point),
                             kReauthDialogHeight, kReauthDialogWidth,
                             InitializeSigninWebDialogUI(false));
}

// static
std::unique_ptr<views::WebView>
SigninViewControllerDelegateViews::CreateProfileCustomizationWebView(
    Browser* browser,
    bool is_local_profile_creation,
    bool show_profile_switch_iph) {
  GURL url = GURL(chrome::kChromeUIProfileCustomizationURL);
  if (is_local_profile_creation) {
    url = AppendProfileCustomizationQueryParams(
        url, ProfileCustomizationStyle::kLocalProfileCreation);
  }
  std::unique_ptr<views::WebView> web_view = CreateDialogWebView(
      browser, url, ProfileCustomizationUI::kPreferredHeight,
      ProfileCustomizationUI::kPreferredWidth,
      InitializeSigninWebDialogUI(false));

  ProfileCustomizationUI* web_ui = web_view->GetWebContents()
                                       ->GetWebUI()
                                       ->GetController()
                                       ->GetAs<ProfileCustomizationUI>();
  DCHECK(web_ui);
  web_ui->Initialize(base::BindOnce(&CloseModalSigninInBrowser,
                                    browser->AsWeakPtr(),
                                    show_profile_switch_iph));
  return web_view;
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
// static
std::unique_ptr<views::WebView>
SigninViewControllerDelegateViews::CreateManagedUserNoticeConfirmationWebView(
    Browser* browser,
    std::unique_ptr<signin::EnterpriseProfileCreationDialogParams>
        create_param) {
  bool enable_updated_dialog = base::FeatureList::IsEnabled(
      features::kEnterpriseUpdatedProfileCreationScreen);
  bool is_oidc_account = create_param->is_oidc_account;
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  enable_updated_dialog |=
      is_oidc_account &&
      base::FeatureList::IsEnabled(
          profile_management::features::kOidcAuthProfileManagement);
#endif
  auto width = enable_updated_dialog
                   ? kManagedUserNoticeConfirmationUpdatedDialogWidth
                   : kManagedUserNoticeConfirmationDialogWidth;
  auto height = enable_updated_dialog
                    ? kManagedUserNoticeConfirmationUpdatedDialogHeight
                    : kManagedUserNoticeConfirmationDialogHeight;
  std::unique_ptr<views::WebView> web_view = CreateDialogWebView(
      browser, GURL(chrome::kChromeUIManagedUserProfileNoticeUrl), height,
      width, InitializeSigninWebDialogUI(false));

  ManagedUserProfileNoticeUI* web_dialog_ui =
      web_view->GetWebContents()
          ->GetWebUI()
          ->GetController()
          ->GetAs<ManagedUserProfileNoticeUI>();
  DCHECK(web_dialog_ui);
  web_dialog_ui->Initialize(
      browser,
      is_oidc_account
          ? ManagedUserProfileNoticeUI::ScreenType::kEnterpriseOIDC
          : ManagedUserProfileNoticeUI::ScreenType::kEnterpriseAccountCreation,
      std::move(create_param));

  return web_view;
}
#endif

bool SigninViewControllerDelegateViews::ShouldShowCloseButton() const {
  return should_show_close_button_;
}

void SigninViewControllerDelegateViews::CloseModalSignin() {
  NotifyModalDialogClosed();
  // Either `this` is owned by the view hierarchy through `modal_signin_widget_`
  // or `modal_signin_widget_` is nullptr and then `this` is self-owned.
  if (modal_signin_widget_) {
    modal_signin_widget_->Close();
  } else {
    delete this;
  }
}

void SigninViewControllerDelegateViews::ResizeNativeView(int height) {
  content_view_->SetPreferredSize(
      gfx::Size(content_view_->GetPreferredSize().width(), height));

  if (!modal_signin_widget_) {
    // The modal wasn't displayed yet so just show it with the already resized
    // view.
    DisplayModal();
  }
}

content::WebContents* SigninViewControllerDelegateViews::GetWebContents() {
  return web_contents_;
}

void SigninViewControllerDelegateViews::SetWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  content_view_->SetWebContents(web_contents);
  web_contents_ = web_contents;
  web_contents_->SetDelegate(this);
}

bool SigninViewControllerDelegateViews::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Discard the context menu
  return true;
}

bool SigninViewControllerDelegateViews::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  // If this is a MODAL_TYPE_CHILD, then GetFocusManager() will return the focus
  // manager of the parent window, which has registered accelerators, and the
  // accelerators will fire. If this is a MODAL_TYPE_WINDOW, then this will have
  // no effect, since no accelerators have been registered for this standalone
  // window.
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

content::WebContents* SigninViewControllerDelegateViews::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  // Allows the Gaia reauth page to open links in a new tab.
  chrome::AddWebContents(browser_, source, std::move(new_contents), target_url,
                         disposition, window_features);
  return nullptr;
}

web_modal::WebContentsModalDialogHost*
SigninViewControllerDelegateViews::GetWebContentsModalDialogHost() {
  return browser_->window()->GetWebContentsModalDialogHost();
}

SigninViewControllerDelegateViews::SigninViewControllerDelegateViews(
    std::unique_ptr<views::WebView> content_view,
    Browser* browser,
    ui::mojom::ModalType dialog_modal_type,
    bool wait_for_size,
    bool should_show_close_button,
    bool delete_profile_on_cancel)
    : content_view_(content_view.get()),
      web_contents_(content_view->GetWebContents()),
      browser_(browser),
      should_show_close_button_(should_show_close_button) {
  DCHECK(web_contents_);
  DCHECK(browser_);
  DCHECK(browser_->tab_strip_model()->GetActiveWebContents())
      << "A tab must be active to present the sign-in modal dialog.";
  DCHECK(content_view_);

  // Use the layout manager of `this` to automatically translate its preferred
  // size to the owning Widget.
  SetLayoutManager(std::make_unique<WidgetAutoResizingLayout>());
  // `AnimatingLayoutManager` resizes `animated_view` to match `content_view`'s
  // preferred size with animation.
  views::View* animated_view = AddChildView(std::make_unique<views::View>());
  views::AnimatingLayoutManager* animating_layout =
      animated_view->SetLayoutManager(
          std::make_unique<views::AnimatingLayoutManager>());
  animating_layout
      ->SetBoundsAnimationMode(
          views::AnimatingLayoutManager::BoundsAnimationMode::kAnimateMainAxis)
      .SetOrientation(views::LayoutOrientation::kVertical);
  // Using `FlexLayout` because `AnimatingLayoutManager` doesn't work properly
  // with `FillLayout`.
  auto* flex_layout = animating_layout->SetTargetLayoutManager(
      std::make_unique<views::FlexLayout>());
  flex_layout->SetOrientation(views::LayoutOrientation::kVertical);
  animated_view->AddChildView(std::move(content_view));

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
  // On the local profile creation dialog, cancelling the dialog (for instance
  // through the VKEY_ESCAPE accelerator) should delete the profile.
  if (delete_profile_on_cancel) {
    SetCancelCallback(base::BindOnce(
        &SigninViewControllerDelegateViews::DeleteProfileOnCancel,
        base::Unretained(this)));
  }
#endif

  web_contents_->SetDelegate(this);

  DCHECK(dialog_modal_type == ui::mojom::ModalType::kChild ||
         dialog_modal_type == ui::mojom::ModalType::kWindow)
      << "Unsupported dialog modal type " << dialog_modal_type;
  SetModalType(dialog_modal_type);

  RegisterDeleteDelegateCallback(base::BindOnce(
      &SigninViewControllerDelegateViews::NotifyModalDialogClosed,
      base::Unretained(this)));

  if (!wait_for_size)
    DisplayModal();
}

SigninViewControllerDelegateViews::~SigninViewControllerDelegateViews() =
    default;

std::unique_ptr<views::WebView>
SigninViewControllerDelegateViews::CreateDialogWebView(
    Browser* browser,
    const GURL& url,
    int dialog_height,
    std::optional<int> opt_width,
    InitializeSigninWebDialogUI initialize_signin_web_dialog_ui) {
  int dialog_width = opt_width.value_or(kModalDialogWidth);
  views::WebView* web_view = new views::WebView(browser->profile());
  web_view->LoadInitialURL(url);

  if (initialize_signin_web_dialog_ui) {
    SigninWebDialogUI* web_dialog_ui = static_cast<SigninWebDialogUI*>(
        web_view->GetWebContents()->GetWebUI()->GetController());
    web_dialog_ui->InitializeMessageHandlerWithBrowser(browser);
  }

  web_view->SetPreferredSize(gfx::Size(dialog_width, dialog_height));

  return std::unique_ptr<views::WebView>(web_view);
}

void SigninViewControllerDelegateViews::DisplayModal() {
  DCHECK(!modal_signin_widget_);
  content::WebContents* host_web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();

  // Avoid displaying the sign-in modal view if there are no active web
  // contents. This happens if the user closes the browser window before this
  // dialog has a chance to be displayed.
  if (!host_web_contents)
    return;

  gfx::NativeWindow window = host_web_contents->GetTopLevelNativeWindow();
  switch (GetModalType()) {
    case ui::mojom::ModalType::kWindow:
      modal_signin_widget_ =
          constrained_window::CreateBrowserModalDialogViews(this, window);
      modal_signin_widget_->Show();
      break;
    case ui::mojom::ModalType::kChild:
      modal_signin_widget_ = constrained_window::CreateWebModalDialogViews(
          this, host_web_contents);
      if (should_show_close_button_) {
        auto border = std::make_unique<views::BubbleBorder>(
            views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW,
            kColorProfilesReauthDialogBorder);
        GetBubbleFrameView()->SetBubbleBorder(std::move(border));
      }
      constrained_window::ShowModalDialog(
          modal_signin_widget_->GetNativeWindow(), host_web_contents);
      break;
    default:
      NOTREACHED() << "Unsupported dialog modal type " << GetModalType();
  }

  DCHECK(modal_signin_widget_);
  content_view_->RequestFocus();
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
void SigninViewControllerDelegateViews::DeleteProfileOnCancel() {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(browser_->profile()->GetPath());
  DCHECK(entry);
  DCHECK(entry->IsEphemeral());
  // Open the profile picker in the profile creation step again.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kOpenNewWindowAfterProfileDeletion));
  // Since the profile is ephemeral, closing all browser windows triggers the
  // deletion.
  BrowserList::CloseAllBrowsersWithProfile(browser_->profile(),
                                           BrowserList::CloseCallback(),
                                           BrowserList::CloseCallback(),
                                           /*skip_beforeunload=*/true);
}
#endif

BEGIN_METADATA(SigninViewControllerDelegateViews)
END_METADATA

// --------------------------------------------------------------------
// SigninViewControllerDelegate static methods
// --------------------------------------------------------------------

// static
SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateSyncConfirmationDelegate(
    Browser* browser,
    SyncConfirmationStyle style,
    bool is_sync_promo) {
  return new SigninViewControllerDelegateViews(
      SigninViewControllerDelegateViews::CreateSyncConfirmationWebView(
          browser, style, is_sync_promo),
      browser, ui::mojom::ModalType::kWindow, true, false);
}

// static
SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateSigninErrorDelegate(Browser* browser) {
  return new SigninViewControllerDelegateViews(
      SigninViewControllerDelegateViews::CreateSigninErrorWebView(browser),
      browser, ui::mojom::ModalType::kWindow, true, false);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
// static
SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateReauthConfirmationDelegate(
    Browser* browser,
    const CoreAccountId& account_id,
    signin_metrics::ReauthAccessPoint access_point) {
  return new SigninViewControllerDelegateViews(
      SigninViewControllerDelegateViews::CreateReauthConfirmationWebView(
          browser, access_point),
      browser, ui::mojom::ModalType::kChild, false, true);
}

// static
SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateProfileCustomizationDelegate(
    Browser* browser,
    bool is_local_profile_creation,
    bool show_profile_switch_iph) {
  return new SigninViewControllerDelegateViews(
      SigninViewControllerDelegateViews::CreateProfileCustomizationWebView(
          browser, is_local_profile_creation, show_profile_switch_iph),
      browser, ui::mojom::ModalType::kWindow, false, false,
      is_local_profile_creation);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
// static
SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateManagedUserNoticeDelegate(
    Browser* browser,
    std::unique_ptr<signin::EnterpriseProfileCreationDialogParams>
        create_param) {
  return new SigninViewControllerDelegateViews(
      SigninViewControllerDelegateViews::
          CreateManagedUserNoticeConfirmationWebView(browser,
                                                     std::move(create_param)),
      browser, ui::mojom::ModalType::kWindow, true, false);
}
#endif
