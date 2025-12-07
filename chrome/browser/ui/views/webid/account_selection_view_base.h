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
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace webid {

class FedCmAccountSelectionView;

// The radius used for the corner of the "Continue as" button.
inline constexpr int kButtonRadius = 16;
// The fixed, total width of the bubble.
inline constexpr int kBubbleWidth = 375;
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
// The size of the horizontal padding for most elements in the modal.
inline constexpr int kModalHorizontalSpacing = 8;
// The size of the arrow icon.
inline constexpr int kArrowIconSize = 8;
// The size of the spinner used in place of the IDP icon while it is being
// fetched.
inline constexpr int kModalIconSpinnerSize = 28;
// The size of the spinner used in a button when the user clicks on an account
// row, continue button or use other account button.
inline constexpr int kModalButtonSpinnerSize = 20;

inline constexpr char kImageFetcherUmaClient[] = "FedCMAccountChooser";

using AccountSelectionCallback =
    base::RepeatingCallback<bool(const ui::Event&)>;

class BrandIconImageView : public views::ImageView {
  METADATA_HEADER(BrandIconImageView, views::ImageView)

 public:
  explicit BrandIconImageView(int image_size);
  BrandIconImageView(const BrandIconImageView&) = delete;
  BrandIconImageView& operator=(const BrandIconImageView&) = delete;
  ~BrandIconImageView() override;

  // This method will crop the given `image` if `should_circle_crop` and will
  // attempt to set it into the BrandIconImageView. Returns whether the image
  // was successfully set or not.
  bool SetBrandIconImage(const gfx::Image& image, bool should_circle_crop);

 private:
  int image_size_;
};

class AccountHoverButton : public HoverButton {
 public:
  AccountHoverButton(AccountSelectionCallback callback,
                     std::unique_ptr<views::View> icon_view,
                     const std::u16string& title,
                     const std::u16string& subtitle,
                     std::unique_ptr<views::View> secondary_view,
                     bool add_vertical_label_spacing,
                     const std::u16string& footer,
                     int button_position);
  AccountHoverButton(const AccountHoverButton&) = delete;
  AccountHoverButton& operator=(const AccountHoverButton&) = delete;
  ~AccountHoverButton() override;

  // HoverButton
  void StateChanged(ButtonState old_state) override;

  void OnPressed(const ui::Event& event);
  bool HasBeenClicked();

  // Changes the opacity of elements in the button to appear disabled. Used when
  // the button is disabled in the verifying sheet.
  void SetDisabledOpacity();
  bool HasDisabledOpacity();

  // Should only be invoked when the button has a secondary view.
  void ReplaceSecondaryViewWithSpinner();

  // Used for testing.
  void SetCallbackForTesting(AccountSelectionCallback callback);

 private:
  AccountSelectionCallback callback_;
  // The order of this account button relative to other account buttons in
  // the dialog (e.g. 0 is the topmost account, 1 the one below it, etc.). Used
  // to record a metric when the button is clicked.
  int button_position_;
  bool has_spinner_{false};
  bool is_appear_disabled_{false};
  bool has_been_clicked_{false};
};

class AccountHoverButtonSecondaryView : public views::View {
 public:
  AccountHoverButtonSecondaryView();
  AccountHoverButtonSecondaryView(const AccountHoverButtonSecondaryView&) =
      delete;
  AccountHoverButtonSecondaryView& operator=(
      const AccountHoverButtonSecondaryView&) = delete;
  ~AccountHoverButtonSecondaryView() override = default;

  void ReplaceWithSpinner();
  void SetDisabledOpacity();

 private:
  raw_ptr<views::ImageView> arrow_image_view_{nullptr};
};

// Base class for interacting with FedCM account selection dialog.
class AccountSelectionViewBase {
 public:
  AccountSelectionViewBase(
      FedCmAccountSelectionView* owner,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const content::RelyingPartyData& rp_data,
      float device_scale_factor);
  virtual ~AccountSelectionViewBase();

  // Updates the FedCM dialog to show the "account picker" sheet.
  // `rp_icon` is the RP icon to be displayed on the header of the dialog when
  // there are multiple IdPs to select from.
  virtual void ShowMultiAccountPicker(
      const std::vector<IdentityRequestAccountPtr>& accounts,
      const std::vector<IdentityProviderDataPtr>& idp_list,
      const gfx::Image& rp_icon,
      bool show_back_button) = 0;

  // Updates the FedCM dialog to show the "verifying" sheet.
  virtual void ShowVerifyingSheet(const IdentityRequestAccountPtr& account,
                                  const std::u16string& title) = 0;

  // Updates to show a single account. On widget mode, used when showing the
  // account confirmation dialog after the user picks one of multiple accounts.
  // On button mode, used for the user to pick the single account.
  virtual void ShowSingleAccountConfirmDialog(
      const IdentityRequestAccountPtr& account,
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
      const IdentityRequestAccountPtr& account) = 0;

  // Gets the title of the dialog.
  virtual std::string GetDialogTitle() const = 0;

  // Gets the subtitle of the dialog, if any.
  virtual std::optional<std::string> GetDialogSubtitle() const = 0;

  virtual void UpdateTitleAndSubtitle(const content::RelyingPartyData& rp_data);

 protected:
  void SetLabelProperties(views::Label* label);

  // Returns a View containing information about an account: the picture for
  // the account on the left, and information about the account on the right.
  // |clickable_position| contains an int if and only if the account is a
  // HoverButton, and in that case the number is the 0-based position of that
  // account in the overall dialog. |used_string| is set if this is a returning
  // account in a multi IDP dialog.
  std::unique_ptr<views::View> CreateAccountRow(
      const IdentityRequestAccountPtr& account,
      std::optional<int> clickable_position,
      bool should_include_idp,
      bool is_modal_dialog = false,
      int additional_vertical_padding = 0,
      std::optional<std::u16string> used_string = std::nullopt);

  // Returns a StyledLabel containing a disclosure label. The label links to
  // privacy policy and terms of service URLs, if available.
  std::unique_ptr<views::StyledLabel> CreateDisclosureLabel(
      const IdentityRequestAccountPtr& account);

  // Gets the summary and description string of the error.
  std::pair<std::u16string, std::u16string> GetErrorDialogText(
      const std::optional<TokenError>& error,
      const std::u16string& idp_for_display);

  // Observes events on AccountSelectionBubbleView.
  // Dangling when running Chromedriver's run_py_tests.py test suite.
  // TODO(https://crbug.com/377803489): This is a real dangling pointer in
  // production code. The subclasses of AccountSelectionViewBase also inherit
  // (indirectly) from views::DialogDelegate, with owned_by_widget = true. This
  // means that this class is owned by the widget, which in turn is owned by
  // FedCmAccountSelectionView. The problem is that the widget uses
  // NATIVE_WIDGET_OWNS_WIDGET ownership semantics and is closed via
  // Widget::Close() which is asynchronous. ~FedCmAccountSelectionView() calls
  // into FedCmAccountSelectionView::Close() which asynchronously closes the
  // Widget. When the Widget is eventually destroyed, this class is destroyed,
  // but that's after FedCmAccountSelectionView is destroyed.
  raw_ptr<FedCmAccountSelectionView, DanglingUntriaged> owner_{nullptr};

  // Relying party data to customize the dialog.
  content::RelyingPartyData rp_data_;

  // The device's scale factor.
  float device_scale_factor_;

  // Used to ensure that callbacks are not run if the AccountSelectionViewBase
  // is destroyed.
  base::WeakPtrFactory<AccountSelectionViewBase> weak_ptr_factory_{this};
};

}  // namespace webid

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_VIEW_BASE_H_
