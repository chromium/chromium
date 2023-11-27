// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/signin_view_controller_delegate_views.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/search_engine_choice/search_engine_choice_tab_helper.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/signin/profile_customization_ui.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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
const int kEnterpriseConfirmationDialogWidth = 512;
const int kEnterpriseConfirmationDialogHeight = 576;
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
#if BUILDFLAG(ENABLE_SEARCH_ENGINE_CHOICE)
  SearchEngineChoiceService* search_engine_choice_service =
      SearchEngineChoiceServiceFactory::GetForProfile(browser->profile());
  if (search_engine_choice_service &&
      search_engine_choice_service->CanShowDialog(CHECK_DEREF(browser.get()))) {
    ShowSearchEngineChoiceDialog(*browser);
  }
#endif
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
    bool is_signin_intercept) {
  GURL url = GURL(chrome::kChromeUISyncConfirmationURL);
  if (is_signin_intercept) {
    url = AppendSyncConfirmationQueryParams(
        url, SyncConfirmationStyle::kSigninInterceptModal);
  }
  return CreateDialogWebView(
      browser, url,
      GetSyncConfirmationDialogPreferredHeight(browser->profile()),
      kSyncConfirmationDialogWidth, InitializeSigninWebDialogUI(true));
}

// static
std::unique_ptr<views::WebView>
SigninViewControllerDelegateViews::CreateSigninErrorWebView(Browser* browser) {
  return CreateDialogWebView(browser, GURL(chrome::kChromeUISigninErrorURL),
                             kSigninErrorDialogHeight, absl::nullopt,
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
SigninViewControllerDelegateViews::CreateEnterpriseConfirmationWebView(
    Browser* browser,
    const AccountInfo& account_info,
    bool profile_creation_required_by_policy,
    bool show_link_data_option,
    signin::SigninChoiceCallback callback) {
  std::unique_ptr<views::WebView> web_view = CreateDialogWebView(
      browser, GURL(chrome::kChromeUIEnterpriseProfileWelcomeURL),
      kEnterpriseConfirmationDialogHeight, kEnterpriseConfirmationDialogWidth,
      InitializeSigninWebDialogUI(false));

  ManagedUserProfileNoticeUI* web_dialog_ui =
      web_view->GetWebContents()
          ->GetWebUI()
          ->GetController()
          ->GetAs<ManagedUserProfileNoticeUI>();
  DCHECK(web_dialog_ui);
  web_dialog_ui->Initialize(
      browser,
      ManagedUserProfileNoticeUI::ScreenType::kEnterpriseAccountCreation,
      account_info, profile_creation_required_by_policy, show_link_data_option,
      std::move(callback));

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
    const content::NativeWebKeyboardEvent& event) {
  // If this is a MODAL_TYPE_CHILD, then GetFocusManager() will return the focus
  // manager of the parent window, which has registered accelerators, and the
  // accelerators will fire. If this is a MODAL_TYPE_WINDOW, then this will have
  // no effect, since no accelerators have been registered for this standalone
  // window.
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

void SigninViewControllerDelegateViews::AddNewContents(
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
}

web_modal::WebContentsModalDialogHost*
SigninViewControllerDelegateViews::GetWebContentsModalDialogHost() {
  return browser_->window()->GetWebContentsModalDialogHost();
}

SigninViewControllerDelegateViews::SigninViewControllerDelegateViews(
    std::unique_ptr<views::WebView> content_view,
    Browser* browser,
    ui::ModalType dialog_modal_type,
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

  SetButtons(ui::DIALOG_BUTTON_NONE);

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

  DCHECK(dialog_modal_type == ui::MODAL_TYPE_CHILD ||
         dialog_modal_type == ui::MODAL_TYPE_WINDOW)
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
    absl::optional<int> opt_width,
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
    case ui::MODAL_TYPE_WINDOW:
      modal_signin_widget_ =
          constrained_window::CreateBrowserModalDialogViews(this, window);
      modal_signin_widget_->Show();
      break;
    case ui::MODAL_TYPE_CHILD:
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
      NOTREACHED_NORETURN()
          << "Unsupported dialog modal type " << GetModalType();
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

BEGIN_METADATA(SigninViewControllerDelegateViews, views::DialogDelegateView)
END_METADATA

// --------------------------------------------------------------------
// SigninViewControllerDelegate static methods
// --------------------------------------------------------------------

// static
SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateSyncConfirmationDelegate(
    Browser* browser,
    bool is_signin_intercept) {
  return new SigninViewControllerDelegateViews(
      SigninViewControllerDelegateViews::CreateSyncConfirmationWebView(
          browser, is_signin_intercept),
      browser, ui::MODAL_TYPE_WINDOW, true, false);
}

// static
SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateSigninErrorDelegate(Browser* browser) {
  return new SigninViewControllerDelegateViews(
      SigninViewControllerDelegateViews::CreateSigninErrorWebView(browser),
      browser, ui::MODAL_TYPE_WINDOW, true, false);
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
      browser, ui::MODAL_TYPE_CHILD, false, true);
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
      browser, ui::MODAL_TYPE_WINDOW, false, false, is_local_profile_creation);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
// static
SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateEnterpriseConfirmationDelegate(
    Browser* browser,
    const AccountInfo& account_info,
    bool profile_creation_required_by_policy,
    bool show_link_data_option,
    signin::SigninChoiceCallback callback) {
  return new SigninViewControllerDelegateViews(
      SigninViewControllerDelegateViews::CreateEnterpriseConfirmationWebView(
          browser, account_info, profile_creation_required_by_policy,
          show_link_data_option, std::move(callback)),
      browser, ui::MODAL_TYPE_WINDOW, true, false);
}
#endif
