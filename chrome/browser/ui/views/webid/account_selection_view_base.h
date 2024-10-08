// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_VIEW_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_VIEW_BASE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/i18n/case_conversion.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_observer.h"
#include "chrome/browser/picture_in_picture/scoped_picture_in_picture_occlusion_observation.h"
#include "chrome/browser/ui/monogram_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/widget/widget_observer.h"

using IdentityProviderDataPtr = scoped_refptr<content::IdentityProviderData>;
using IdentityRequestAccountPtr =
    scoped_refptr<content::IdentityRequestAccount>;
using TokenError = content::IdentityCredentialTokenError;

namespace content {
class IdentityRequestAccount;
}  // namespace content

// The radius used for the corner of the "Continue as" button.
inline constexpr int kButtonRadius = 16;
// The fixed, total width of the bubble.
inline constexpr int kBubbleWidth = 375;
// The desired size of the avatars of user accounts.
inline constexpr int kDesiredAvatarSize = 30;
// The desired size of the IDP icon used as badge for the user account avatar
// when there are multiple IDPs.
inline constexpr int kLargeAvatarBadgeSize = 16;
// The size of the icon of the identity provider in the bubble.
inline constexpr int kBubbleIdpIconSize = 20;
// The desired size of the icon for a "login to IDP" secondary view.
inline constexpr int kIdpLoginIconSize = 20;
// The desired size of the icon for the "Choose an account" button or the "sign
// in to IDP" button in the multi IDP UI.
inline constexpr int kMultiIdpIconSize = 20;
// The left margin of a multi IDP icon button.
inline constexpr int kMultiIdpIconLeftMargin = 8;
// The right margin of a multi IDP icon button.
inline constexpr int kMultiIdpIconRightMargin = 10;
// The size of the padding used at the top and bottom of the bubble.
inline constexpr int kTopBottomPadding = 4;
// The size of the horizontal padding between the bubble content and the edge of
// the bubble, as well as the horizontal padding between icons and text.
inline constexpr int kLeftRightPadding = 12;
// The size of the vertical padding for most elements in the bubble.
inline constexpr int kVerticalSpacing = 8;
// Vertical spacing for buttons in multi IDP.
inline constexpr int kMultiIdpVerticalSpacing = 4;
// The height of the progress bar shown when showing "Verifying...".
inline constexpr int kProgressBarHeight = 2;
// The size of the space between the right boundary of the WebContents and the
// right boundary of the bubble.
inline constexpr int kRightMargin = 40;
// The size of the space between the top boundary of the WebContents and the top
// boundary of the bubble.
inline constexpr int kTopMargin = 16;
// The size of the icon of the identity provider in the modal.
inline constexpr int kModalIdpIconSize = 32;
// The size of the icons when they are combined i.e. IDP icon + arrow icon + RP
// icon is shown at the same time in the modal.
inline constexpr int kModalCombinedIconSize = 20;
// The size of avatars in the modal dialog.
inline constexpr int kModalAvatarSize = 36;
// The size of the horizontal padding for most elements in the modal.
inline constexpr int kModalHorizontalSpacing = 8;
// Size of the IDP icon offset when badging the IDP icon in the account button.
inline constexpr int kIdpBadgeOffset = 8;
// The size of the arrow icon.
inline constexpr int kArrowIconSize = 8;
// The size of the arrow icon.
inline constexpr int kModalIconSpinnerSize = 28;

inline constexpr char kImageFetcherUmaClient[] = "FedCMAccountChooser";

class BrandIconImageView : public views::ImageView {
  METADATA_HEADER(BrandIconImageView, views::ImageView)

 public:
  BrandIconImageView(
      base::OnceCallback<void(const GURL&, const gfx::ImageSkia&)> add_image,
      int image_size,
      bool should_circle_crop,
      std::optional<SkColor> background_color = std::nullopt,
      base::RepeatingClosure on_image_set = base::DoNothing());
  BrandIconImageView(const BrandIconImageView&) = delete;
  BrandIconImageView& operator=(const BrandIconImageView&) = delete;
  ~BrandIconImageView() override;

  // Fetch image and set it on BrandIconImageView.
  void FetchImage(const GURL& icon_url,
                  image_fetcher::ImageFetcher& image_fetcher);

  void CropAndSetImage(const gfx::ImageSkia& original_image);

  // If this image uses a background circle, updates its color.
  void OnBackgroundColorUpdated(const SkColor& background_color);

  std::optional<SkColor> background_color_for_testing() const {
    return background_color_;
  }

 private:
  void OnImageFetched(const GURL& image_url,
                      const gfx::Image& image,
                      const image_fetcher::RequestMetadata& metadata);

  base::OnceCallback<void(const GURL&, const gfx::ImageSkia&)> add_image_;
  int image_size_;
  bool should_circle_crop_;
  // The color of a background circle used to encapsulate the brand icon. Set
  // when this object is used as a badge for an account icon. When set, this
  // should be the background color of the dialog.
  std::optional<SkColor> background_color_;
  gfx::ImageSkia cropped_idp_image_;
  base::RepeatingClosure on_image_set_;

  base::WeakPtrFactory<BrandIconImageView> weak_ptr_factory_{this};
};

// Base class for interacting with FedCM account selection dialog.
class AccountSelectionViewBase : public PictureInPictureOcclusionObserver {
 public:
  // Used to observe changes to the account selection dialog.
  class Observer {
   public:
    // Called when a user either selects the account from the multi-account
    // chooser or clicks the "continue" button.
    // Takes `account` as well as `idp_data` since passing `account_id`
    // is insufficient in the multiple IDP case. The caller should pass a cref,
    // as these objects are owned by the observer.
    virtual void OnAccountSelected(
        const content::IdentityRequestAccount& account,
        const content::IdentityProviderData& idp_data,
        const ui::Event& event) = 0;

    // Called when the user clicks "privacy policy" or "terms of service" link.
    virtual void OnLinkClicked(
        content::IdentityRequestDialogController::LinkType link_type,
        const GURL& url,
        const ui::Event& event) = 0;

    // Called when the user clicks "back" button.
    virtual void OnBackButtonClicked() = 0;

    // Called when the user clicks "close" button.
    virtual void OnCloseButtonClicked(const ui::Event& event) = 0;

    // Called when the user clicks the "continue" button on the sign-in
    // failure dialog or wants to sign in to another account.
    virtual void OnLoginToIdP(const GURL& idp_config_url,
                              const GURL& idp_login_url,
                              const ui::Event& event) = 0;

    // Called when the user clicks "got it" button.
    virtual void OnGotIt(const ui::Event& event) = 0;

    // Called when the user clicks the "more details" button on the error
    // dialog.
    virtual void OnMoreDetails(const ui::Event& event) = 0;

    // Called when the accounts UI is displayed.
    virtual void OnAccountsDisplayed() = 0;

    // Called when the user clicks on the 'Choose an account' button
    virtual void OnChooseAnAccountClicked() = 0;
  };

  AccountSelectionViewBase(
      content::WebContents* web_contents,
      AccountSelectionViewBase::Observer* observer,
      views::WidgetObserver* widget_observer,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::u16string rp_for_display);
  AccountSelectionViewBase();
  ~AccountSelectionViewBase() override;

  // PictureInPictureOcclusionObserver:
  void OnOcclusionStateChanged(bool occluded) override;

  // Creates and sets the appropriate dialog widget, depending on whether the
  // dialog is bubble or modal.
  virtual void InitDialogWidget() = 0;

  // Updates the FedCM dialog to show the "account picker" sheet.
  // `is_choose_an_account` is true if the dialog must change its title to
  // 'Choose an account'. This is currently only used on widget mode, when
  // clicking on the 'Choose an account' button.
  virtual void ShowMultiAccountPicker(
      const std::vector<IdentityRequestAccountPtr>& accounts,
      const std::vector<IdentityProviderDataPtr>& idp_list,
      bool show_back_button,
      bool is_choose_an_account) = 0;

  // Updates the FedCM dialog to show the "verifying" sheet.
  virtual void ShowVerifyingSheet(
      const content::IdentityRequestAccount& account,
      const std::u16string& title) = 0;

  // Updates to show a single account. On widget mode, used when showing the
  // account confirmation dialog after the user picks one of multiple accounts.
  // On button mode, used for the user to pick the single account.
  virtual void ShowSingleAccountConfirmDialog(
      const content::IdentityRequestAccount& account,
      bool show_back_button) = 0;

  // Updates the FedCM dialog to show the "failure" sheet.
  virtual void ShowFailureDialog(
      const std::u16string& idp_for_display,
      const content::IdentityProviderMetadata& idp_metadata) = 0;

  // Updates the FedCM dialog to show the "error" sheet.
  virtual void ShowErrorDialog(
      const std::u16string& idp_for_display,
      const content::IdentityProviderMetadata& idp_metadata,
      const std::optional<TokenError>& error) = 0;

  // Updates the FedCM dialog to show the "request permission" sheet.
  virtual void ShowRequestPermissionDialog(
      const content::IdentityRequestAccount& account,
      const content::IdentityProviderData& idp_data) = 0;

  // Updates to show a single account along with a button to show all options.
  // Currently used when there are multiple IDPs and exactly one returning
  // account.
  virtual void ShowSingleReturningAccountDialog(
      const std::vector<IdentityRequestAccountPtr>& accounts,
      const std::vector<IdentityProviderDataPtr>& idp_list) = 0;

  // Updates the FedCM dialog to show the "loading" sheet.
  virtual void ShowLoadingDialog() = 0;

  // Closes the dialog, without dismissing the FedCM API.
  virtual void CloseDialog() = 0;

  // Gets the title of the dialog.
  virtual std::string GetDialogTitle() const = 0;

  // Retrieves the dialog widget used to control the dialog, if available. This
  // method is virtual for testing purposes.
  virtual base::WeakPtr<views::Widget> GetDialogWidget();

  // Populates `brand_icon_images_` when an IDP image has been fetched.
  void AddIdpImage(const GURL& image_url, const gfx::ImageSkia& idp_image);

  // Returns the network traffic annotation tag for FedCM.
  static net::NetworkTrafficAnnotationTag GetTrafficAnnotation();

  // Updates the position of the dialog. Used when the contents of the dialog
  // has changed or when the widget which the dialog is anchored on has been
  // resized.
  virtual void UpdateDialogPosition() = 0;

  // Whether the dialog can fit in the web contents at its preferred size.
  // Virtual for testing purposes.
  virtual bool CanFitInWebContents();

  bool IsOccluded() const { return is_occluded_; }

 protected:
  void SetLabelProperties(views::Label* label);

  // Returns a View containing information about an account: the picture for
  // the account on the left, and information about the account on the right.
  // |clickable_position| contains an int if and only if the account is a
  // HoverButton, and in that case the number is the 0-based position of that
  // account in the overall dialog.
  std::unique_ptr<views::View> CreateAccountRow(
      const content::IdentityRequestAccount& account,
      std::optional<int> clickable_position,
      bool should_include_idp,
      bool is_modal_dialog = false,
      int additional_vertical_padding = 0,
      std::optional<std::u16string> last_used_string = std::nullopt);

  // Returns a StyledLabel containing a disclosure label. The label links to
  // privacy policy and terms of service URLs, if available.
  std::unique_ptr<views::StyledLabel> CreateDisclosureLabel(
      const content::IdentityProviderData& idp_data);

  // Sets the brand views::ImageView visibility and image. Initiates the
  // download of the brand icon if necessary.
  void ConfigureBrandImageView(BrandIconImageView* image_view,
                               const GURL& brand_icon_url);

  // The ImageFetcher used to fetch the account pictures for FedCM.
  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;

  // Web contents which the dialog is rendered on.
  base::WeakPtr<content::WebContents> web_contents_;

  // The images for the brand icons. Stored so that they can be reused upon
  // pressing the back button after choosing an account.
  base::flat_map<GURL, gfx::ImageSkia> brand_icon_images_;

  // Widget to control the dialog i.e. hide, show, add observer etc.
  base::WeakPtr<views::Widget> dialog_widget_;

  // Observes events on `dialog_widget_`.
  // Dangling when running Chromedriver's run_py_tests.py test suite.
  raw_ptr<views::WidgetObserver, DanglingUntriaged> widget_observer_{nullptr};

  // Observes events on AccountSelectionBubbleView.
  // Dangling when running Chromedriver's run_py_tests.py test suite.
  raw_ptr<Observer, DanglingUntriaged> observer_{nullptr};

  // The description of the RP to be used in the dialog.
  std::u16string rp_for_display_;

  // Whether the widget is occluded (and therefore we should ignore inputs.
  bool is_occluded_{false};

  ScopedPictureInPictureOcclusionObservation occlusion_observation_{this};

  // Used to ensure that callbacks are not run if the AccountSelectionViewBase
  // is destroyed.
  base::WeakPtrFactory<AccountSelectionViewBase> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_VIEW_BASE_H_
