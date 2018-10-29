// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/profiles/badged_profile_photo.h"
#include "chrome/browser/ui/views/profiles/signin_view_controller_delegate_views.h"
#include "chrome/browser/ui/views/profiles/user_manager_view.h"
#include "chrome/browser/ui/views/sync/dice_signin_button_view.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/render_widget_host_view.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/clip_recorder.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// Helpers --------------------------------------------------------------------

constexpr int kButtonHeight = 32;
constexpr int kFixedAccountRemovalViewWidth = 280;
constexpr int kFixedMenuWidthPreDice = 240;
constexpr int kFixedMenuWidthDice = 288;
constexpr int kIconSize = 16;

// Spacing between the edge of the user menu and the top/bottom or left/right of
// the menu items.
constexpr int kMenuEdgeMargin = 16;

// If the bubble is too large to fit on the screen, it still needs to be at
// least this tall to show one row.
constexpr int kMinimumScrollableContentHeight = 40;

constexpr int kVerticalSpacing = 16;

// Number of times the Dice sign-in promo illustration should be shown.
constexpr int kDiceSigninPromoIllustrationShowCountMax = 10;

bool IsProfileChooser(profiles::BubbleViewMode mode) {
  return mode == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER;
}

// DEPRECATED: New user menu components should use views::BoxLayout instead.
// Creates a GridLayout with a single column. This ensures that all the child
// views added get auto-expanded to fill the full width of the bubble.
views::GridLayout* CreateSingleColumnLayout(views::View* view, int width) {
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>(view));

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     views::GridLayout::kFixedSize, views::GridLayout::FIXED,
                     width, width);
  return layout;
}

views::Link* CreateLink(const base::string16& link_text,
                        views::LinkListener* listener) {
  views::Link* link_button = new views::Link(link_text);
  link_button->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  link_button->SetUnderline(false);
  link_button->set_listener(listener);
  return link_button;
}

bool HasAuthError(Profile* profile) {
  const SigninErrorController* error =
      SigninErrorControllerFactory::GetForProfile(profile);
  return error && error->HasError();
}

std::string GetAuthErrorAccountId(Profile* profile) {
  const SigninErrorController* error =
      SigninErrorControllerFactory::GetForProfile(profile);
  if (!error)
    return std::string();

  return error->error_account_id();
}

views::ImageButton* CreateBackButton(views::ButtonListener* listener) {
  views::ImageButton* back_button = new views::ImageButton(listener);
  back_button->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                 views::ImageButton::ALIGN_MIDDLE);
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  back_button->SetImage(views::ImageButton::STATE_NORMAL,
                        rb->GetImageSkiaNamed(IDR_BACK));
  back_button->SetImage(views::ImageButton::STATE_HOVERED,
                        rb->GetImageSkiaNamed(IDR_BACK_H));
  back_button->SetImage(views::ImageButton::STATE_PRESSED,
                        rb->GetImageSkiaNamed(IDR_BACK_P));
  back_button->SetImage(views::ImageButton::STATE_DISABLED,
                        rb->GetImageSkiaNamed(IDR_BACK_D));
  back_button->SetFocusForPlatform();
  return back_button;
}

BadgedProfilePhoto::BadgeType GetProfileBadgeType(Profile* profile) {
  if (profile->IsSupervised()) {
    return profile->IsChild() ? BadgedProfilePhoto::BADGE_TYPE_CHILD
                              : BadgedProfilePhoto::BADGE_TYPE_SUPERVISOR;
  }
  // |Profile::IsSyncAllowed| is needed to check whether sync is allowed by GPO
  // policy.
  if (AccountConsistencyModeManager::IsDiceEnabledForProfile(profile) &&
      profile->IsSyncAllowed() &&
      SigninManagerFactory::GetForProfile(profile)->IsAuthenticated()) {
    return BadgedProfilePhoto::BADGE_TYPE_SYNC_COMPLETE;
  }
  return BadgedProfilePhoto::BADGE_TYPE_NONE;
}

std::vector<gfx::Image> GetImagesForAccounts(
    const std::vector<AccountInfo>& accounts,
    Profile* profile) {
  AccountTrackerService* tracker_service =
      AccountTrackerServiceFactory::GetForProfile(profile);
  std::vector<gfx::Image> images;
  for (auto account : accounts) {
    images.push_back(tracker_service->GetAccountImage(account.account_id));
  }
  return images;
}

}  // namespace

// A title card with one back button left aligned and one label center aligned.
class TitleCard : public views::View {
 public:
  TitleCard(const base::string16& message, views::ButtonListener* listener,
            views::ImageButton** back_button) {
    back_button_ = CreateBackButton(listener);
    *back_button = back_button_;

    title_label_ =
        new views::Label(message, views::style::CONTEXT_DIALOG_TITLE);
    title_label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);

    AddChildView(back_button_);
    AddChildView(title_label_);
  }

  // Creates a new view that has the |title_card| with horizontal padding at the
  // top, an edge-to-edge separator below, and the specified |view| at the
  // bottom.
  static views::View* AddPaddedTitleCard(views::View* view,
                                         TitleCard* title_card,
                                         int width) {
    views::View* titled_view = new views::View();
    views::GridLayout* layout = titled_view->SetLayoutManager(
        std::make_unique<views::GridLayout>(titled_view));

    ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
    const gfx::Insets dialog_insets =
        provider->GetInsetsMetric(views::INSETS_DIALOG);
    // Column set 0 is a single column layout with horizontal padding at left
    // and right, and column set 1 is a single column layout with no padding.
    views::ColumnSet* columns = layout->AddColumnSet(0);
    columns->AddPaddingColumn(1.0, dialog_insets.left());
    int available_width = width - dialog_insets.width();
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                       views::GridLayout::kFixedSize, views::GridLayout::FIXED,
                       available_width, available_width);
    columns->AddPaddingColumn(1.0, dialog_insets.right());
    layout->AddColumnSet(1)->AddColumn(
        views::GridLayout::FILL, views::GridLayout::FILL,
        views::GridLayout::kFixedSize, views::GridLayout::FIXED, width, width);

    layout->StartRowWithPadding(1.0, 0, views::GridLayout::kFixedSize,
                                kVerticalSpacing);
    layout->AddView(title_card);
    layout->StartRowWithPadding(1.0, 1.0, views::GridLayout::kFixedSize,
                                kVerticalSpacing);
    layout->AddView(new views::Separator());

    layout->StartRow(1.0, 1.0);
    layout->AddView(view);

    return titled_view;
  }

 private:
  void Layout() override {
    // The back button is left-aligned.
    const int back_button_width = back_button_->GetPreferredSize().width();
    back_button_->SetBounds(0, views::GridLayout::kFixedSize, back_button_width,
                            height());

    // The title is in the same row as the button positioned with a minimum
    // amount of space between them.
    const int button_to_title_min_spacing =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            ChromeDistanceMetric::DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
    const int unavailable_leading_space =
        back_button_width + button_to_title_min_spacing;

    // Because the title is centered, the unavailable space to the left is also
    // unavailable to right of the title.
    const int unavailable_space = 2 * unavailable_leading_space;
    const int label_width = width() - unavailable_space;
    DCHECK_GT(label_width, 0);
    title_label_->SetBounds(unavailable_leading_space,
                            views::GridLayout::kFixedSize, label_width,
                            height());
  }

  gfx::Size CalculatePreferredSize() const override {
    int height = std::max(title_label_->GetPreferredSize().height(),
        back_button_->GetPreferredSize().height());
    return gfx::Size(width(), height);
  }

  views::ImageButton* back_button_;
  views::Label* title_label_;

  DISALLOW_COPY_AND_ASSIGN(TitleCard);
};

// ProfileChooserView ---------------------------------------------------------

// static
ProfileChooserView* ProfileChooserView::profile_bubble_ = nullptr;
bool ProfileChooserView::close_on_deactivate_for_testing_ = true;

// static
void ProfileChooserView::ShowBubble(
    profiles::BubbleViewMode view_mode,
    const signin::ManageAccountsParams& manage_accounts_params,
    signin_metrics::AccessPoint access_point,
    views::Button* anchor_button,
    gfx::NativeView parent_window,
    const gfx::Rect& anchor_rect,
    Browser* browser,
    bool is_source_keyboard) {
  if (IsShowing())
    return;

  profile_bubble_ =
      new ProfileChooserView(anchor_button, browser, view_mode,
                             manage_accounts_params.service_type, access_point);
  if (anchor_button) {
    anchor_button->AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);
  } else {
    DCHECK(parent_window);
    profile_bubble_->SetAnchorRect(anchor_rect);
    profile_bubble_->set_parent_window(parent_window);
  }

  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(profile_bubble_);
  widget->Show();
  base::RecordAction(base::UserMetricsAction("ProfileChooser_Show"));
  if (is_source_keyboard)
    profile_bubble_->FocusFirstProfileButton();
}

// static
bool ProfileChooserView::IsShowing() {
  return profile_bubble_ != NULL;
}

// static
void ProfileChooserView::Hide() {
  if (IsShowing())
    profile_bubble_->GetWidget()->Close();
}

ProfileChooserView::ProfileChooserView(views::Button* anchor_button,
                                       Browser* browser,
                                       profiles::BubbleViewMode view_mode,
                                       signin::GAIAServiceType service_type,
                                       signin_metrics::AccessPoint access_point)
    : BubbleDialogDelegateView(anchor_button, views::BubbleBorder::TOP_RIGHT),
      browser_(browser),
      anchor_button_(anchor_button),
      view_mode_(view_mode),
      gaia_service_type_(service_type),
      access_point_(access_point),
      close_bubble_helper_(this, browser),
      dice_enabled_(AccountConsistencyModeManager::IsDiceEnabledForProfile(
          browser->profile())),
      menu_width_(dice_enabled_ ? kFixedMenuWidthDice
                                : kFixedMenuWidthPreDice) {
  // The sign in webview will be clipped on the bottom corners without these
  // margins, see related bug <http://crbug.com/593203>.
  set_margins(gfx::Insets(0, views::GridLayout::kFixedSize, 2, 0));
  ResetView();
  chrome::RecordDialogCreation(chrome::DialogIdentifier::PROFILE_CHOOSER);
}

ProfileChooserView::~ProfileChooserView() {
  ProfileOAuth2TokenService* oauth2_token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile());
  if (oauth2_token_service)
    oauth2_token_service->RemoveObserver(this);
}

void ProfileChooserView::ResetView() {
  open_other_profile_indexes_map_.clear();
  delete_account_button_map_.clear();
  reauth_account_button_map_.clear();
  sync_error_button_ = nullptr;
  manage_accounts_link_ = nullptr;
  manage_accounts_button_ = nullptr;
  signin_current_profile_button_ = nullptr;
  signin_with_gaia_account_button_ = nullptr;
  current_profile_card_ = nullptr;
  first_profile_button_ = nullptr;
  guest_profile_button_ = nullptr;
  users_button_ = nullptr;
  go_incognito_button_ = nullptr;
  lock_button_ = nullptr;
  close_all_windows_button_ = nullptr;
  add_account_link_ = nullptr;
  gaia_signin_cancel_button_ = nullptr;
  remove_account_button_ = nullptr;
  account_removal_cancel_button_ = nullptr;
  sync_to_another_account_button_ = nullptr;
  dice_signin_button_view_ = nullptr;
  passwords_button_ = nullptr;
  credit_cards_button_ = nullptr;
  addresses_button_ = nullptr;
  signout_button_ = nullptr;
}

void ProfileChooserView::Init() {
  set_close_on_deactivate(close_on_deactivate_for_testing_);

  avatar_menu_.reset(new AvatarMenu(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage(),
      this, browser_));
  avatar_menu_->RebuildMenu();

  Profile* profile = browser_->profile();
  ProfileOAuth2TokenService* oauth2_token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  if (oauth2_token_service)
    oauth2_token_service->AddObserver(this);

  // If view mode is PROFILE_CHOOSER but there is an auth error, force
  // ACCOUNT_MANAGEMENT mode.
  if (IsProfileChooser(view_mode_) && HasAuthError(profile) &&
      AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile) &&
      avatar_menu_->GetItemAt(avatar_menu_->GetActiveProfileIndex())
          .signed_in) {
    view_mode_ = profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT;
  }

  // The arrow keys can be used to tab between items.
  AddAccelerator(ui::Accelerator(ui::VKEY_DOWN, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_UP, ui::EF_NONE));

  ShowViewFromMode(view_mode_);
}

void ProfileChooserView::OnNativeThemeChanged(
    const ui::NativeTheme* native_theme) {
  views::BubbleDialogDelegateView::OnNativeThemeChanged(native_theme);
  SetBackground(views::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground)));
}

void ProfileChooserView::OnAvatarMenuChanged(
    AvatarMenu* avatar_menu) {
  if (IsProfileChooser(view_mode_) ||
      view_mode_ == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT) {
    // Refresh the view with the new menu. We can't just update the local copy
    // as this may have been triggered by a sign out action, in which case
    // the view is being destroyed.
    ShowView(view_mode_, avatar_menu);
  }
}

void ProfileChooserView::OnRefreshTokenAvailable(
    const std::string& account_id) {
  if (view_mode_ == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT ||
      view_mode_ == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
      view_mode_ == profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH) {
    // The account management UI is only available through the
    // --account-consistency=mirror flag.
    ShowViewFromMode(AccountConsistencyModeManager::IsMirrorEnabledForProfile(
                         browser_->profile())
                         ? profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT
                         : profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER);
  }
}

void ProfileChooserView::OnRefreshTokenRevoked(const std::string& account_id) {
  // Refresh the account management view when an account is removed from the
  // profile.
  if (view_mode_ == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT)
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT);
}

void ProfileChooserView::ShowView(profiles::BubbleViewMode view_to_display,
                                  AvatarMenu* avatar_menu) {
  // The account management view should only be displayed if the active profile
  // is signed in.
  if (view_to_display == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT) {
    DCHECK(AccountConsistencyModeManager::IsMirrorEnabledForProfile(
        browser_->profile()));
    const AvatarMenu::Item& active_item = avatar_menu->GetItemAt(
        avatar_menu->GetActiveProfileIndex());
    if (!active_item.signed_in) {
      // This is the case when the user selects the sign out option in the user
      // menu upon encountering unrecoverable errors. Afterwards, the profile
      // chooser view is shown instead of the account management view.
      view_to_display = profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER;
    }
  }

  if (browser_->profile()->IsSupervised() &&
      (view_to_display == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
       view_to_display == profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL)) {
    LOG(WARNING) << "Supervised user attempted to add/remove account";
    return;
  }

  ResetView();
  RemoveAllChildViews(true);
  view_mode_ = view_to_display;

  views::GridLayout* layout = nullptr;
  views::View* sub_view = nullptr;
  views::View* view_to_focus = nullptr;
  switch (view_mode_) {
    case profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN:
    case profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT:
    case profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH:
      // The modal sign-in view is shown in for bubble view modes.
      // See |SigninViewController::ShouldShowSigninForMode|.
      NOTREACHED();
      break;
    case profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL:
      layout = CreateSingleColumnLayout(this, kFixedAccountRemovalViewWidth);
      sub_view = CreateAccountRemovalView();
      break;
    case profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT:
    case profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER:
      layout = CreateSingleColumnLayout(this, menu_width_);
      sub_view = CreateProfileChooserView(avatar_menu);
      break;
  }

  views::ScrollView* scroll_view = new views::ScrollView;
  scroll_view->set_hide_horizontal_scrollbar(true);
  // TODO(https://crbug.com/871762): it's a workaround for the crash.
  scroll_view->set_draw_overflow_indicator(false);
  scroll_view->ClipHeightTo(0, GetMaxHeight());
  scroll_view->SetContents(sub_view);

  layout->StartRow(1.0, 0);
  layout->AddView(scroll_view);
  if (GetBubbleFrameView()) {
    SizeToContents();
    // SizeToContents() will perform a layout, but only if the size changed.
    Layout();
  }
  if (view_to_focus)
    view_to_focus->RequestFocus();
}

void ProfileChooserView::ShowViewFromMode(profiles::BubbleViewMode mode) {
  if (SigninViewController::ShouldShowSigninForMode(mode)) {
    // Hides the user menu if it is currently shown. The user menu automatically
    // closes when it loses focus; however, on Windows, the signin modals do not
    // take away focus, thus we need to manually close the bubble.
    Hide();
    browser_->signin_view_controller()->ShowSignin(mode, browser_,
                                                   access_point_);
  } else {
    ShowView(mode, avatar_menu_.get());
  }
}

void ProfileChooserView::FocusFirstProfileButton() {
  if (first_profile_button_)
    first_profile_button_->RequestFocus();
}

void ProfileChooserView::WindowClosing() {
  DCHECK_EQ(profile_bubble_, this);
  if (anchor_button_)
    anchor_button_->AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
  profile_bubble_ = NULL;
}

bool ProfileChooserView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() != ui::VKEY_DOWN &&
      accelerator.key_code() != ui::VKEY_UP)
    return BubbleDialogDelegateView::AcceleratorPressed(accelerator);

  // Move the focus up or down.
  GetFocusManager()->AdvanceFocus(accelerator.key_code() != ui::VKEY_DOWN);
  return true;
}

views::View* ProfileChooserView::GetInitiallyFocusedView() {
#if defined(OS_MACOSX)
  // On Mac, buttons are not focusable when full keyboard access is turned off,
  // causing views::Widget to fall back to focusing the first focusable View.
  // This behavior is not desired in the |ProfileChooserView| because of its
  // menu-like design using |HoverButtons|. Avoid this by returning null when
  // full keyboard access is off.
  if (!GetFocusManager() || !GetFocusManager()->keyboard_accessible())
    return nullptr;
#endif
  return signin_current_profile_button_;
}

int ProfileChooserView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

bool ProfileChooserView::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Suppresses the context menu because some features, such as inspecting
  // elements, are not appropriate in a bubble.
  return true;
}

void ProfileChooserView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == passwords_button_) {
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_PasswordsClicked"));
    chrome::ShowSettingsSubPage(browser_, chrome::kPasswordManagerSubPage);
  } else if (sender == credit_cards_button_) {
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_PaymentsClicked"));
    chrome::ShowSettingsSubPage(browser_, chrome::kPaymentsSubPage);
  } else if (sender == addresses_button_) {
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_AddressesClicked"));
    chrome::ShowSettingsSubPage(browser_, chrome::kAutofillSubPage);
  } else if (sender == guest_profile_button_) {
    PrefService* service = g_browser_process->local_state();
    DCHECK(service);
    DCHECK(service->GetBoolean(prefs::kBrowserGuestModeEnabled));
    profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());
    base::RecordAction(base::UserMetricsAction("ProfileChooser_GuestClicked"));
  } else if (sender == users_button_) {
    // If this is a guest session, close all the guest browser windows.
    if (browser_->profile()->IsGuestSession()) {
      profiles::CloseGuestProfileWindows();
    } else {
      base::RecordAction(
          base::UserMetricsAction("ProfileChooser_ManageClicked"));
      UserManager::Show(base::FilePath(),
                        profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
    }
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_OPEN_USER_MANAGER);
  } else if (sender == go_incognito_button_) {
    DCHECK(ShouldShowGoIncognito());
    chrome::NewIncognitoWindow(browser_->profile());
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_GO_INCOGNITO);
  } else if (sender == lock_button_) {
    profiles::LockProfile(browser_->profile());
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_LOCK);
  } else if (sender == close_all_windows_button_) {
    profiles::CloseProfileWindows(browser_->profile());
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_CloseAllClicked"));
  } else if (sender == sync_error_button_) {
    sync_ui_util::AvatarSyncErrorType error =
        static_cast<sync_ui_util::AvatarSyncErrorType>(sender->id());
    switch (error) {
      case sync_ui_util::MANAGED_USER_UNRECOVERABLE_ERROR:
        chrome::ShowSettingsSubPage(browser_, chrome::kSignOutSubPage);
        break;
      case sync_ui_util::UNRECOVERABLE_ERROR:
        if (ProfileSyncServiceFactory::GetForProfile(browser_->profile())) {
          browser_sync::ProfileSyncService::SyncEvent(
              browser_sync::ProfileSyncService::STOP_FROM_OPTIONS);
        }
        SigninManagerFactory::GetForProfile(browser_->profile())
            ->SignOut(signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS,
                      signin_metrics::SignoutDelete::IGNORE_METRIC);
        ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN);
        break;
      case sync_ui_util::SUPERVISED_USER_AUTH_ERROR:
        NOTREACHED();
        break;
      case sync_ui_util::AUTH_ERROR:
        ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH);
        break;
      case sync_ui_util::UPGRADE_CLIENT_ERROR:
        chrome::OpenUpdateChromeDialog(browser_);
        break;
      case sync_ui_util::PASSPHRASE_ERROR:
      case sync_ui_util::SETTINGS_UNCONFIRMED_ERROR:
        chrome::ShowSettingsSubPage(browser_, chrome::kSyncSetupSubPage);
        break;
      case sync_ui_util::NO_SYNC_ERROR:
        NOTREACHED();
        break;
    }
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_SignInAgainClicked"));
  } else if (sender == remove_account_button_) {
    RemoveAccount();
  } else if (sender == account_removal_cancel_button_) {
    account_id_to_remove_.clear();
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT);
  } else if (sender == gaia_signin_cancel_button_) {
    Profile* profile = browser_->profile();
    // The account management view is only available with the
    // --account-consistency=mirror flag.
    bool account_management_available =
        SigninManagerFactory::GetForProfile(profile)->IsAuthenticated() &&
        AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile);
    ShowViewFromMode(account_management_available ?
        profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT :
        profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER);
  } else if (sender == current_profile_card_) {
    if (dice_enabled_ &&
        SigninManagerFactory::GetForProfile(browser_->profile())
            ->IsAuthenticated()) {
      chrome::ShowSettingsSubPage(browser_, chrome::kPeopleSubPage);
    } else {
      // Open settings to edit profile name and image. The profile doesn't need
      // to be authenticated to open this.
      avatar_menu_->EditProfile(avatar_menu_->GetActiveProfileIndex());
      PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_IMAGE);
      PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_NAME);
    }
  } else if (sender == manage_accounts_button_) {
    // This button can either mean show/hide the account management view,
    // depending on which view it is displayed.
    ShowViewFromMode(view_mode_ == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT
                         ? profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER
                         : profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT);
  } else if (sender == signin_current_profile_button_) {
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN);
  } else if (sender == signin_with_gaia_account_button_) {
    DCHECK(dice_signin_button_view_->account());
    Hide();
    signin_ui_util::EnableSyncFromPromo(
        browser_, dice_signin_button_view_->account().value(), access_point_,
        true /* is_default_promo_account */);
  } else if (sender == sync_to_another_account_button_) {
    // Extract the promo accounts for the submenu, i.e. remove the first
    // one from the list because it is already shown in a separate button.
    std::vector<AccountInfo> accounts(dice_sync_promo_accounts_.begin() + 1,
                                      dice_sync_promo_accounts_.end());
    // Display the submenu to list |accounts|.
    // Using base::Unretained(this) is safe here because |dice_accounts_menu_|
    // is owned by |ProfileChooserView|, i.e. |this|.
    dice_accounts_menu_ = std::make_unique<DiceAccountsMenu>(
        accounts, GetImagesForAccounts(accounts, browser_->profile()),
        base::BindOnce(&ProfileChooserView::EnableSync,
                       base::Unretained(this)));
    // Add sign-out button.
    dice_accounts_menu_->SetSignOutButtonCallback(base::BindOnce(
        &ProfileChooserView::SignOutAllWebAccounts, base::Unretained(this)));
    dice_accounts_menu_->Show(sender, sync_to_another_account_button_);
  } else if (sender == signout_button_) {
    SignOutAllWebAccounts();
    base::RecordAction(base::UserMetricsAction("Signin_Signout_FromUserMenu"));
  } else {
    // Either one of the "other profiles", or one of the profile accounts
    // buttons was pressed.
    ButtonIndexes::const_iterator profile_match =
        open_other_profile_indexes_map_.find(sender);
    if (profile_match != open_other_profile_indexes_map_.end()) {
      avatar_menu_->SwitchToProfile(
          profile_match->second, ui::DispositionFromEventFlags(event.flags()) ==
                                     WindowOpenDisposition::NEW_WINDOW,
          ProfileMetrics::SWITCH_PROFILE_ICON);
      base::RecordAction(
          base::UserMetricsAction("ProfileChooser_ProfileClicked"));
      Hide();
    } else {
      // This was a profile accounts button.
      AccountButtonIndexes::const_iterator account_match =
          delete_account_button_map_.find(sender);
      if (account_match != delete_account_button_map_.end()) {
        account_id_to_remove_ = account_match->second;
        ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL);
      } else {
        account_match = reauth_account_button_map_.find(sender);
        DCHECK(account_match != reauth_account_button_map_.end());
        ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH);
      }
    }
  }
}

void ProfileChooserView::RemoveAccount() {
  DCHECK(!account_id_to_remove_.empty());
  ProfileOAuth2TokenService* oauth2_token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile());
  if (oauth2_token_service) {
    oauth2_token_service->RevokeCredentials(account_id_to_remove_);
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_REMOVE_ACCT);
  }
  account_id_to_remove_.clear();

  ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT);
}

void ProfileChooserView::LinkClicked(views::Link* sender, int event_flags) {
  if (sender == manage_accounts_link_) {
    // This link can either mean show/hide the account management view,
    // depending on which view it is displayed. ShowView() will DCHECK if
    // the account management view is displayed for non signed-in users.
    ShowViewFromMode(
        view_mode_ == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT ?
            profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER :
            profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT);
  } else if (sender == add_account_link_) {
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT);
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_ADD_ACCT);
  }
}

void ProfileChooserView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                const gfx::Range& range,
                                                int event_flags) {
  chrome::ShowSettings(browser_);
}

views::View* ProfileChooserView::CreateProfileChooserView(
    AvatarMenu* avatar_menu) {
  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(view, menu_width_);
  // Separate items into active and alternatives.
  Indexes other_profiles;
  views::View* sync_error_view = nullptr;
  views::View* current_profile_view = nullptr;
  views::View* current_profile_accounts = nullptr;
  views::View* option_buttons_view = nullptr;
  views::View* autofill_home_view = nullptr;
  bool current_profile_signed_in = false;
  for (size_t i = 0; i < avatar_menu->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& item = avatar_menu->GetItemAt(i);
    if (item.active) {
      option_buttons_view = CreateOptionsView(
          item.signed_in && profiles::IsLockAvailable(browser_->profile()),
          avatar_menu);
      current_profile_view = CreateCurrentProfileView(item, false);
      autofill_home_view = CreateAutofillHomeView();
      current_profile_signed_in = item.signed_in;
      if (!IsProfileChooser(view_mode_))
        current_profile_accounts = CreateCurrentProfileAccountsView(item);
      sync_error_view = CreateSyncErrorViewIfNeeded(item);
    } else {
      other_profiles.push_back(i);
    }
  }

  if (sync_error_view) {
    layout->StartRow(1.0, 0);
    layout->AddView(sync_error_view);
    layout->StartRow(views::GridLayout::kFixedSize, 0);
    layout->AddView(new views::Separator());
  }

  if (!current_profile_view) {
    // Guest windows don't have an active profile.
    current_profile_view = CreateGuestProfileView();
    option_buttons_view = CreateOptionsView(false, avatar_menu);
  }

  if (!(dice_enabled_ && sync_error_view)) {
    layout->StartRow(1.0, 0);
    layout->AddView(current_profile_view);
  }

  if (!IsProfileChooser(view_mode_)) {
    DCHECK(current_profile_accounts);
    layout->StartRow(views::GridLayout::kFixedSize, 0);
    layout->AddView(new views::Separator());
    layout->StartRow(1.0, 0);
    layout->AddView(current_profile_accounts);
  }

  if (browser_->profile()->IsSupervised()) {
    layout->StartRow(1.0, 0);
    layout->AddView(CreateSupervisedUserDisclaimerView());
  }

  if (autofill_home_view) {
    const int content_list_vert_spacing =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
    if (!current_profile_signed_in) {
      // If the user is signed in then the autofill data is a part of the
      // account logically. Otherwise, use a separator.
      layout->StartRow(0, 0);
      layout->AddView(new views::Separator());
      layout->AddPaddingRow(1.0, content_list_vert_spacing);
    }
    layout->StartRow(0, 0);
    layout->AddView(autofill_home_view);
    layout->AddPaddingRow(1.0, content_list_vert_spacing);
  }

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(new views::Separator());

  if (option_buttons_view) {
    layout->StartRow(views::GridLayout::kFixedSize, 0);
    layout->AddView(option_buttons_view);
  }
  return view;
}

views::View* ProfileChooserView::CreateSyncErrorViewIfNeeded(
    const AvatarMenu::Item& avatar_item) {
  int content_string_id, button_string_id;
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(browser_->profile());
  sync_ui_util::AvatarSyncErrorType error =
      sync_ui_util::GetMessagesForAvatarSyncError(
          browser_->profile(), *signin_manager, &content_string_id,
          &button_string_id);
  if (error == sync_ui_util::NO_SYNC_ERROR)
    return nullptr;

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  if (error != sync_ui_util::SUPERVISED_USER_AUTH_ERROR && dice_enabled_)
    return CreateDiceSyncErrorView(avatar_item, error, button_string_id);

  // Sets an overall horizontal layout.
  views::View* view = new views::View();
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(kMenuEdgeMargin),
      provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  view->SetLayoutManager(std::move(layout));

  // Adds the sync problem icon.
  views::ImageView* sync_problem_icon = new views::ImageView();
  sync_problem_icon->SetImage(
      gfx::CreateVectorIcon(kSyncProblemIcon, kIconSize, gfx::kGoogleRed700));
  view->AddChildView(sync_problem_icon);

  // Adds a vertical view to organize the error title, message, and button.
  views::View* vertical_view = new views::View();
  const int small_vertical_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
  auto vertical_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(), small_vertical_spacing);
  vertical_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  vertical_view->SetLayoutManager(std::move(vertical_layout));

  // Adds the title.
  views::Label* title_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_SYNC_ERROR_USER_MENU_TITLE));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetEnabledColor(gfx::kGoogleRed700);
  vertical_view->AddChildView(title_label);

  // Adds body content.
  views::Label* content_label =
      new views::Label(l10n_util::GetStringUTF16(content_string_id));
  content_label->SetMultiLine(true);
  content_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  vertical_view->AddChildView(content_label);

  // Adds an action button if an action exists.
  if (button_string_id) {
    // Adds a padding row between error title/content and the button.
    auto* padding = new views::View;
    padding->SetPreferredSize(gfx::Size(
        0,
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
    vertical_view->AddChildView(padding);

    sync_error_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
        this, l10n_util::GetStringUTF16(button_string_id));
    // Track the error type so that the correct action can be taken in
    // ButtonPressed().
    sync_error_button_->set_id(error);
    vertical_view->AddChildView(sync_error_button_);
    view->SetBorder(views::CreateEmptyBorder(0, 0, small_vertical_spacing, 0));
  }

  view->AddChildView(vertical_view);
  return view;
}

views::View* ProfileChooserView::CreateDiceSyncErrorView(
    const AvatarMenu::Item& avatar_item,
    sync_ui_util::AvatarSyncErrorType error,
    int button_string_id) {
  // Creates a view containing an error hover button displaying the current
  // profile (only selectable when sync is paused or disabled) and when sync is
  // not disabled there is a blue button to resolve the error.
  views::View* view = new views::View();
  const int current_profile_vertical_margin =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING);
  view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical,
      gfx::Insets(current_profile_vertical_margin, 0),
      current_profile_vertical_margin));

  const bool show_sync_paused_ui = error == sync_ui_util::AUTH_ERROR;
  const bool sync_disabled = !browser_->profile()->IsSyncAllowed();
  // Add profile hover button.
  auto current_profile_photo = std::make_unique<BadgedProfilePhoto>(
      show_sync_paused_ui
          ? BadgedProfilePhoto::BADGE_TYPE_SYNC_PAUSED
          : sync_disabled ? BadgedProfilePhoto::BADGE_TYPE_SYNC_DISABLED
                          : BadgedProfilePhoto::BADGE_TYPE_SYNC_ERROR,
      avatar_item.icon);
  HoverButton* current_profile = new HoverButton(
      this, std::move(current_profile_photo),
      l10n_util::GetStringUTF16(
          show_sync_paused_ui
              ? IDS_PROFILES_DICE_SYNC_PAUSED_TITLE
              : sync_disabled ? IDS_PROFILES_DICE_SYNC_DISABLED_TITLE
                              : IDS_SYNC_ERROR_USER_MENU_TITLE),
      avatar_item.username);

  if (!show_sync_paused_ui && !sync_disabled) {
    current_profile->SetStyle(HoverButton::STYLE_ERROR);
    current_profile->SetEnabled(false);
  }

  view->AddChildView(current_profile);
  current_profile_card_ = current_profile;

  if (sync_disabled)
    return view;

  // Add blue button.
  sync_error_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
      this, l10n_util::GetStringUTF16(button_string_id));
  sync_error_button_->set_id(error);
  base::RecordAction(
      base::UserMetricsAction("ProfileChooser_SignInAgainDisplayed"));
  // Add horizontal and bottom margin to blue button.
  views::View* padded_view = new views::View();
  padded_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  padded_view->SetBorder(views::CreateEmptyBorder(
      0, kMenuEdgeMargin, kMenuEdgeMargin - current_profile_vertical_margin,
      kMenuEdgeMargin));
  padded_view->AddChildView(sync_error_button_);
  view->AddChildView(padded_view);
  return view;
}

views::View* ProfileChooserView::CreateCurrentProfileView(
    const AvatarMenu::Item& avatar_item,
    bool is_guest) {
  Profile* profile = browser_->profile();
  const bool sync_disabled = !profile->IsSyncAllowed();
  if (!is_guest && sync_disabled)
    return CreateDiceSyncErrorView(avatar_item, sync_ui_util::NO_SYNC_ERROR, 0);

  if (!avatar_item.signed_in && dice_enabled_ &&
      SyncPromoUI::ShouldShowSyncPromo(profile)) {
    return CreateDiceSigninView();
  }

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  views::View* view = new views::View();
  bool mirror_enabled =
      AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile);
  int content_list_vert_spacing =
      mirror_enabled
          ? provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI)
          : provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_SINGLE);
  view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(content_list_vert_spacing, 0),
      0));

  auto current_profile_photo = std::make_unique<BadgedProfilePhoto>(
      GetProfileBadgeType(profile), avatar_item.icon);
  const base::string16 profile_name =
      profiles::GetAvatarNameForProfile(profile->GetPath());

  // Show the profile name by itself if not signed in or account consistency is
  // disabled. Otherwise, show the email attached to the profile.
  bool show_email = !is_guest && avatar_item.signed_in && !mirror_enabled;
  const base::string16 hover_button_title =
      dice_enabled_ && profile->IsSyncAllowed() && show_email
          ? l10n_util::GetStringUTF16(IDS_PROFILES_SYNC_COMPLETE_TITLE)
          : profile_name;
  HoverButton* profile_card = new HoverButton(
      this, std::move(current_profile_photo), hover_button_title,
      show_email ? avatar_item.username : base::string16());
  // TODO(crbug.com/815047): Sometimes, |avatar_item.username| is empty when
  // |show_email| is true, which should never happen. This causes a crash when
  // setting the elision behavior, so until this bug is fixed, avoid the crash
  // by checking that the username is not empty.
  if (show_email && !avatar_item.username.empty())
    profile_card->SetSubtitleElideBehavior(gfx::ELIDE_EMAIL);
  current_profile_card_ = profile_card;
  view->AddChildView(current_profile_card_);

  if (is_guest) {
    current_profile_card_->SetEnabled(false);
    return view;
  }

  // The available links depend on the type of profile that is active.
  if (avatar_item.signed_in) {
    if (mirror_enabled) {
      base::string16 button_text = l10n_util::GetStringUTF16(
          IsProfileChooser(view_mode_)
              ? IDS_PROFILES_PROFILE_MANAGE_ACCOUNTS_BUTTON
              : IDS_PROFILES_PROFILE_HIDE_MANAGE_ACCOUNTS_BUTTON);
      manage_accounts_button_ = new HoverButton(this, button_text);
      view->AddChildView(manage_accounts_button_);
    }

    current_profile_card_->SetAccessibleName(
        l10n_util::GetStringFUTF16(
            IDS_PROFILES_EDIT_SIGNED_IN_PROFILE_ACCESSIBLE_NAME,
            profile_name,
            avatar_item.username));
    return view;
  }

  if (!dice_enabled_ &&
      SigninManagerFactory::GetForProfile(profile)->IsSigninAllowed()) {
    views::View* extra_links_view = new views::View();
    extra_links_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical,
        gfx::Insets(provider->GetDistanceMetric(
                        views::DISTANCE_RELATED_CONTROL_VERTICAL),
                    kMenuEdgeMargin),
        kMenuEdgeMargin));
    views::Label* promo =
        new views::Label(l10n_util::GetStringUTF16(IDS_PROFILES_SIGNIN_PROMO));
    promo->SetMultiLine(true);
    promo->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    // Provide a hint to the layout manager by giving the promo text a maximum
    // width. This ensures it has the correct number of lines when determining
    // the initial Widget size.
    promo->SetMaximumWidth(menu_width_);
    extra_links_view->AddChildView(promo);

    signin_current_profile_button_ =
        views::MdTextButton::CreateSecondaryUiBlueButton(
            this, l10n_util::GetStringFUTF16(
                      IDS_SYNC_START_SYNC_BUTTON_LABEL,
                      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)));
    extra_links_view->AddChildView(signin_current_profile_button_);
    signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
        signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
    extra_links_view->SetBorder(views::CreateEmptyBorder(
        0, 0,
        provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL),
        0));
    view->AddChildView(extra_links_view);
  }

  current_profile_card_->SetAccessibleName(
      l10n_util::GetStringFUTF16(
          IDS_PROFILES_EDIT_PROFILE_ACCESSIBLE_NAME, profile_name));
  return view;
}

views::View* ProfileChooserView::CreateDiceSigninView() {
  IncrementDiceSigninPromoShowCount();
  // Fetch signed in GAIA web accounts.
  dice_sync_promo_accounts_ =
      signin_ui_util::GetAccountsForDicePromos(browser_->profile());

  // Create a view that holds an illustration, a promo text and a button to turn
  // on Sync. The promo illustration is only shown the first 10 times per
  // profile.
  int promotext_top_spacing = 16;
  const int additional_bottom_spacing =
      dice_sync_promo_accounts_.empty() ? 0 : 8;
  views::View* view = new views::View();
  view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical,
      gfx::Insets(0, 0, additional_bottom_spacing, 0)));

  const bool promo_account_available = !dice_sync_promo_accounts_.empty();

  // Log sign-in impressions user metrics.
  signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
  signin_metrics::RecordSigninImpressionWithAccountUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
      promo_account_available);

  if (!promo_account_available) {
    // Show promo illustration+text when there is no promo account.
    if (GetDiceSigninPromoShowCount() <=
        kDiceSigninPromoIllustrationShowCountMax) {
      // Add the illustration.
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      views::ImageView* illustration = new NonAccessibleImageView();
      illustration->SetImage(
          *rb.GetNativeImageNamed(IDR_PROFILES_DICE_TURN_ON_SYNC)
               .ToImageSkia());
      view->AddChildView(illustration);
      // Adjust the spacing between illustration and promo text.
      promotext_top_spacing = 24;
    }
    // Add the promo text.
    views::Label* promo = new views::Label(
        l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SYNC_PROMO));
    promo->SetMultiLine(true);
    promo->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    promo->SetMaximumWidth(menu_width_ - 2 * kMenuEdgeMargin);
    promo->SetBorder(views::CreateEmptyBorder(
        promotext_top_spacing, kMenuEdgeMargin, 0, kMenuEdgeMargin));
    view->AddChildView(promo);

    // Create a sign-in button without account information.
    dice_signin_button_view_ = new DiceSigninButtonView(this);
    dice_signin_button_view_->SetBorder(
        views::CreateEmptyBorder(gfx::Insets(kMenuEdgeMargin)));
    view->AddChildView(dice_signin_button_view_);
    signin_current_profile_button_ = dice_signin_button_view_->signin_button();
    return view;
  }
  // Create a button to sign in the first account of
  // |dice_sync_promo_accounts_|.
  AccountInfo dice_promo_default_account = dice_sync_promo_accounts_[0];
  gfx::Image account_icon =
      AccountTrackerServiceFactory::GetForProfile(browser_->profile())
          ->GetAccountImage(dice_promo_default_account.account_id);
  if (account_icon.IsEmpty()) {
    account_icon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }
  dice_signin_button_view_ =
      new DiceSigninButtonView(dice_promo_default_account, account_icon, this,
                               /*show_drop_down_arrow=*/false);
  signin_with_gaia_account_button_ = dice_signin_button_view_->signin_button();

  views::View* promo_button_container = new views::View();
  const int content_list_vert_spacing =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
  const int bottom_spacing = kMenuEdgeMargin - content_list_vert_spacing;
  promo_button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical,
      gfx::Insets(kMenuEdgeMargin, kMenuEdgeMargin, bottom_spacing,
                  kMenuEdgeMargin),
      content_list_vert_spacing));
  promo_button_container->AddChildView(dice_signin_button_view_);

  // Add sign out button.
  signout_button_ = views::MdTextButton::Create(
      this, l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_SIGN_OUT),
      views::style::CONTEXT_BUTTON);
  promo_button_container->AddChildView(signout_button_);

  view->AddChildView(promo_button_container);

  return view;
}

views::View* ProfileChooserView::CreateGuestProfileView() {
  gfx::Image guest_icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          profiles::GetPlaceholderAvatarIconResourceID());
  AvatarMenu::Item guest_avatar_item(0, base::FilePath(), guest_icon);
  guest_avatar_item.active = true;
  guest_avatar_item.name = l10n_util::GetStringUTF16(
      IDS_PROFILES_GUEST_PROFILE_NAME);
  guest_avatar_item.signed_in = false;

  return CreateCurrentProfileView(guest_avatar_item, true);
}

views::View* ProfileChooserView::CreateOptionsView(bool display_lock,
                                                   AvatarMenu* avatar_menu) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int content_list_vert_spacing =
      provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);

  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(view, menu_width_);

  const bool is_guest = browser_->profile()->IsGuestSession();
  // Add the user switching buttons.
  // Order them such that the active user profile comes first (for Dice).
  layout->StartRowWithPadding(1.0, 0, views::GridLayout::kFixedSize,
                              content_list_vert_spacing);
  std::vector<size_t> ordered_item_indices;
  for (size_t i = 0; i < avatar_menu->GetNumberOfItems(); ++i) {
    if (avatar_menu->GetItemAt(i).active)
      ordered_item_indices.insert(ordered_item_indices.begin(), i);
    else
      ordered_item_indices.push_back(i);
  }
  for (size_t i : ordered_item_indices) {
    const AvatarMenu::Item& item = avatar_menu->GetItemAt(i);
    if (!item.active) {
      gfx::Image image = profiles::GetSizedAvatarIcon(
          item.icon, true, kIconSize, kIconSize, profiles::SHAPE_CIRCLE);
      views::LabelButton* button =
          new HoverButton(this, *image.ToImageSkia(),
                          profiles::GetProfileSwitcherTextForItem(item));
      open_other_profile_indexes_map_[button] = i;

      if (!first_profile_button_)
        first_profile_button_ = button;
      layout->StartRow(1.0, 0);
      layout->AddView(button);
    }
  }

  UMA_HISTOGRAM_BOOLEAN("ProfileChooser.HasProfilesShown",
                        first_profile_button_);

  // Add the "Guest" button for browsing as guest
  if (!is_guest && !browser_->profile()->IsSupervised()) {
    PrefService* service = g_browser_process->local_state();
    DCHECK(service);
    if (service->GetBoolean(prefs::kBrowserGuestModeEnabled)) {
      guest_profile_button_ = new HoverButton(
          this,
          gfx::CreateVectorIcon(kUserMenuGuestIcon, kIconSize,
                                gfx::kChromeIconGrey),
          l10n_util::GetStringUTF16(IDS_PROFILES_OPEN_GUEST_PROFILE_BUTTON));
      layout->StartRow(1.0, 0);
      layout->AddView(guest_profile_button_);
    }
  }

  base::string16 text = l10n_util::GetStringUTF16(
      is_guest ? IDS_PROFILES_EXIT_GUEST : IDS_PROFILES_MANAGE_USERS_BUTTON);
  const gfx::VectorIcon& settings_icon =
      is_guest ? kCloseAllIcon : kSettingsIcon;
  users_button_ = new HoverButton(
      this,
      gfx::CreateVectorIcon(settings_icon, kIconSize, gfx::kChromeIconGrey),
      text);

  layout->StartRow(1.0, 0);
  layout->AddView(users_button_);

  if (display_lock) {
    lock_button_ = new HoverButton(
        this,
        gfx::CreateVectorIcon(vector_icons::kLockIcon, kIconSize,
                              gfx::kChromeIconGrey),
        l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_SIGNOUT_BUTTON));
    layout->StartRow(1.0, 0);
    layout->AddView(lock_button_);
  } else if (!is_guest) {
    AvatarMenu::Item active_avatar_item =
        avatar_menu->GetItemAt(ordered_item_indices[0]);
    close_all_windows_button_ = new HoverButton(
        this,
        gfx::CreateVectorIcon(kCloseAllIcon, kIconSize, gfx::kChromeIconGrey),
        avatar_menu->GetNumberOfItems() >= 2
            ? l10n_util::GetStringFUTF16(IDS_PROFILES_EXIT_PROFILE_BUTTON,
                                         active_avatar_item.name)
            : l10n_util::GetStringUTF16(IDS_PROFILES_CLOSE_ALL_WINDOWS_BUTTON));
    layout->StartRow(1.0, 0);
    layout->AddView(close_all_windows_button_);
  }

  layout->AddPaddingRow(views::GridLayout::kFixedSize,
                        content_list_vert_spacing);
  return view;
}

views::View* ProfileChooserView::CreateSupervisedUserDisclaimerView() {
  views::View* view = new views::View();
  int horizontal_margin = kMenuEdgeMargin;
  views::GridLayout* layout =
      CreateSingleColumnLayout(view, menu_width_ - 2 * horizontal_margin);
  view->SetBorder(views::CreateEmptyBorder(0, horizontal_margin,
                                           kMenuEdgeMargin, horizontal_margin));

  views::Label* disclaimer = new views::Label(
      avatar_menu_->GetSupervisedUserInformation(), CONTEXT_BODY_TEXT_SMALL);
  disclaimer->SetMultiLine(true);
  disclaimer->SetAllowCharacterBreak(true);
  disclaimer->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->StartRow(1.0, 0);
  layout->AddView(disclaimer);

  return view;
}

views::View* ProfileChooserView::CreateAutofillHomeView() {
  if (browser_->profile()->IsGuestSession())
    return nullptr;

  views::View* view = new views::View();
  view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));

  // Passwords.
  passwords_button_ = new HoverButton(
      this, gfx::CreateVectorIcon(kKeyIcon, kIconSize, gfx::kChromeIconGrey),
      l10n_util::GetStringUTF16(IDS_PROFILES_PASSWORDS_LINK));
  view->AddChildView(passwords_button_);

  // Credit cards.
  credit_cards_button_ = new HoverButton(
      this,
      gfx::CreateVectorIcon(kCreditCardIcon, kIconSize, gfx::kChromeIconGrey),
      l10n_util::GetStringUTF16(IDS_PROFILES_CREDIT_CARDS_LINK));
  view->AddChildView(credit_cards_button_);

  // Addresses.
  addresses_button_ =
      new HoverButton(this,
                      gfx::CreateVectorIcon(vector_icons::kLocationOnIcon,
                                            kIconSize, gfx::kChromeIconGrey),
                      l10n_util::GetStringUTF16(IDS_PROFILES_ADDRESSES_LINK));
  view->AddChildView(addresses_button_);
  return view;
}

views::View* ProfileChooserView::CreateCurrentProfileAccountsView(
    const AvatarMenu::Item& avatar_item) {
  DCHECK(avatar_item.signed_in);
  views::View* view = new views::View();
  view->SetBackground(views::CreateSolidBackground(
      profiles::kAvatarBubbleAccountsBackgroundColor));
  views::GridLayout* layout = CreateSingleColumnLayout(view, menu_width_);

  // Get state of authentication error, if any.
  Profile* profile = browser_->profile();
  std::string error_account_id = GetAuthErrorAccountId(profile);

  // The primary account should always be listed first.
  // TODO(rogerta): we still need to further differentiate the primary account
  // from the others in the UI, so more work is likely required here:
  // crbug.com/311124.
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  DCHECK(identity_manager->HasPrimaryAccount());
  AccountInfo primary_account = identity_manager->GetPrimaryAccountInfo();

  CreateAccountButton(layout, primary_account.account_id, true,
                      error_account_id == primary_account.account_id,
                      menu_width_);
  for (const AccountInfo& account :
       profiles::GetSecondaryAccountsForSignedInProfile(profile))
    CreateAccountButton(layout, account.account_id, false,
                        error_account_id == account.account_id, menu_width_);

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  if (!profile->IsSupervised()) {
    layout->AddPaddingRow(views::GridLayout::kFixedSize, vertical_spacing);

    add_account_link_ = CreateLink(l10n_util::GetStringFUTF16(
        IDS_PROFILES_PROFILE_ADD_ACCOUNT_BUTTON, avatar_item.name), this);
    add_account_link_->SetBorder(views::CreateEmptyBorder(
        0, provider->GetInsetsMetric(views::INSETS_DIALOG).left(),
        vertical_spacing, 0));
    layout->StartRow(1.0, 0);
    layout->AddView(add_account_link_);
  }

  return view;
}

void ProfileChooserView::CreateAccountButton(views::GridLayout* layout,
                                             const std::string& account_id,
                                             bool is_primary_account,
                                             bool reauth_required,
                                             int width) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  std::string email = signin_ui_util::GetDisplayEmail(browser_->profile(),
                                                      account_id);
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  const gfx::ImageSkia* delete_default_image =
      rb->GetImageNamed(IDR_CLOSE_1).ToImageSkia();
  const int kDeleteButtonWidth = delete_default_image->width();
  gfx::ImageSkia warning_default_image;
  int warning_button_width = 0;
  if (reauth_required) {
    const int kIconSize = 18;
    warning_default_image = gfx::CreateVectorIcon(
        vector_icons::kWarningIcon, kIconSize, gfx::kChromeIconGrey);
    warning_button_width =
        kIconSize +
        provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  }

  const gfx::Insets dialog_insets =
      provider->GetInsetsMetric(views::INSETS_DIALOG);

  int available_width =
      width - dialog_insets.width() - kDeleteButtonWidth - warning_button_width;
  HoverButton* email_button =
      new HoverButton(this, warning_default_image, base::UTF8ToUTF16(email));
  email_button->SetEnabled(reauth_required);
  email_button->SetSubtitleElideBehavior(gfx::ELIDE_EMAIL);
  email_button->SetMinSize(gfx::Size(0, kButtonHeight));
  email_button->SetMaxSize(gfx::Size(available_width, kButtonHeight));
  layout->StartRow(1.0, 0);
  layout->AddView(email_button);

  if (reauth_required)
    reauth_account_button_map_[email_button] = account_id;

  // Delete button.
  if (!browser_->profile()->IsSupervised()) {
    views::ImageButton* delete_button = new views::ImageButton(this);
    delete_button->SetImageAlignment(views::ImageButton::ALIGN_RIGHT,
                                     views::ImageButton::ALIGN_MIDDLE);
    delete_button->SetImage(views::ImageButton::STATE_NORMAL,
                            delete_default_image);
    delete_button->SetImage(views::ImageButton::STATE_HOVERED,
                            rb->GetImageSkiaNamed(IDR_CLOSE_1_H));
    delete_button->SetImage(views::ImageButton::STATE_PRESSED,
                            rb->GetImageSkiaNamed(IDR_CLOSE_1_P));
    delete_button->SetBounds(
        width - provider->GetInsetsMetric(views::INSETS_DIALOG).right() -
            kDeleteButtonWidth,
        views::GridLayout::kFixedSize, kDeleteButtonWidth, kButtonHeight);

    email_button->set_notify_enter_exit_on_child(true);
    email_button->AddChildView(delete_button);

    // Save the original email address, as the button text could be elided.
    delete_account_button_map_[delete_button] = account_id;
  }
}

views::View* ProfileChooserView::CreateAccountRemovalView() {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  const gfx::Insets dialog_insets =
      provider->GetInsetsMetric(views::INSETS_DIALOG);

  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(
      view, kFixedAccountRemovalViewWidth - dialog_insets.width());

  view->SetBorder(
      views::CreateEmptyBorder(0, dialog_insets.left(),
                               dialog_insets.bottom(), dialog_insets.right()));

  const std::string& primary_account = SigninManagerFactory::GetForProfile(
      browser_->profile())->GetAuthenticatedAccountId();
  bool is_primary_account = primary_account == account_id_to_remove_;

  const int unrelated_vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL);

  // Adds main text.
  layout->StartRowWithPadding(1.0, views::GridLayout::kFixedSize,
                              views::GridLayout::kFixedSize,
                              unrelated_vertical_spacing);

  if (is_primary_account) {
    std::string email = signin_ui_util::GetDisplayEmail(browser_->profile(),
                                                        account_id_to_remove_);
    std::vector<size_t> offsets;
    const base::string16 settings_text =
        l10n_util::GetStringUTF16(IDS_PROFILES_SETTINGS_LINK);
    const base::string16 primary_account_removal_text =
        l10n_util::GetStringFUTF16(IDS_PROFILES_PRIMARY_ACCOUNT_REMOVAL_TEXT,
            base::UTF8ToUTF16(email), settings_text, &offsets);
    views::StyledLabel* primary_account_removal_label =
        new views::StyledLabel(primary_account_removal_text, this);
    primary_account_removal_label->AddStyleRange(
        gfx::Range(offsets[1], offsets[1] + settings_text.size()),
        views::StyledLabel::RangeStyleInfo::CreateForLink());
    primary_account_removal_label->SetTextContext(CONTEXT_BODY_TEXT_SMALL);
    layout->AddView(primary_account_removal_label);
  } else {
    views::Label* content_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_PROFILES_ACCOUNT_REMOVAL_TEXT),
        CONTEXT_BODY_TEXT_SMALL);
    content_label->SetMultiLine(true);
    content_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->AddView(content_label);
  }

  // Adds button.
  if (!is_primary_account) {
    remove_account_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
        this, l10n_util::GetStringUTF16(IDS_PROFILES_ACCOUNT_REMOVAL_BUTTON));
    remove_account_button_->SetHorizontalAlignment(
        gfx::ALIGN_CENTER);
    layout->StartRowWithPadding(1.0, views::GridLayout::kFixedSize,
                                views::GridLayout::kFixedSize,
                                unrelated_vertical_spacing);
    layout->AddView(remove_account_button_);
  } else {
    layout->AddPaddingRow(views::GridLayout::kFixedSize,
                          unrelated_vertical_spacing);
  }

  TitleCard* title_card = new TitleCard(
      l10n_util::GetStringUTF16(IDS_PROFILES_ACCOUNT_REMOVAL_TITLE),
      this, &account_removal_cancel_button_);
  return TitleCard::AddPaddedTitleCard(view, title_card,
      kFixedAccountRemovalViewWidth);
}

bool ProfileChooserView::ShouldShowGoIncognito() const {
  bool incognito_available =
      IncognitoModePrefs::GetAvailability(browser_->profile()->GetPrefs()) !=
          IncognitoModePrefs::DISABLED;
  return incognito_available && !browser_->profile()->IsGuestSession();
}

int ProfileChooserView::GetMaxHeight() const {
  gfx::Rect anchor_rect = GetAnchorRect();
  gfx::Rect screen_space =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(anchor_rect.CenterPoint())
          .work_area();
  int available_space = screen_space.bottom() - anchor_rect.bottom();
#if defined(OS_WIN)
  // On Windows the bubble can also be show to the top of the anchor.
  available_space =
      std::max(available_space, anchor_rect.y() - screen_space.y());
#endif
  return std::max(kMinimumScrollableContentHeight, available_space);
}

void ProfileChooserView::PostActionPerformed(
    ProfileMetrics::ProfileDesktopMenu action_performed) {
  ProfileMetrics::LogProfileDesktopMenu(action_performed, gaia_service_type_);
  gaia_service_type_ = signin::GAIA_SERVICE_TYPE_NONE;
}

void ProfileChooserView::EnableSync(
    const base::Optional<AccountInfo>& account) {
  Hide();
  if (account)
    signin_ui_util::EnableSyncFromPromo(browser_, account.value(),
                                        access_point_,
                                        false /* is_default_promo_account */);
  else
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN);
}

void ProfileChooserView::SignOutAllWebAccounts() {
  Hide();
  ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile())
      ->RevokeAllCredentials();
}

int ProfileChooserView::GetDiceSigninPromoShowCount() const {
  return browser_->profile()->GetPrefs()->GetInteger(
      prefs::kDiceSigninUserMenuPromoCount);
}

void ProfileChooserView::IncrementDiceSigninPromoShowCount() {
  browser_->profile()->GetPrefs()->SetInteger(
      prefs::kDiceSigninUserMenuPromoCount, GetDiceSigninPromoShowCount() + 1);
}
