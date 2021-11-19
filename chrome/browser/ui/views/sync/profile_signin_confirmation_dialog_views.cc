// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/profile_signin_confirmation_dialog_views.h"

#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"

constexpr int kBusinessIconSize = 20;
constexpr int kPreferredWidth = 440;

ProfileSigninConfirmationDialogViews::ProfileSigninConfirmationDialogViews(
    Browser* browser,
    const std::string& username,
    std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate,
    bool prompt_for_new_profile)
    : browser_(browser),
      username_(username),
      delegate_(std::move(delegate)),
      prompt_for_new_profile_(prompt_for_new_profile),
      use_work_profile_wording_(base::FeatureList::IsEnabled(
          features::kSyncConfirmationUpdatedText)) {
  using Delegate = ui::ProfileSigninConfirmationDelegate;
  using DelegateNotifyFn = void (Delegate::*)();
  auto notify_delegate = [](ProfileSigninConfirmationDialogViews* dialog,
                            DelegateNotifyFn fn) {
    if (dialog->delegate_) {
      (dialog->delegate_.get()->*fn)();
      dialog->delegate_.reset();
    }
  };

  auto builder =
      views::Builder<ProfileSigninConfirmationDialogViews>(this)
          .SetButtonLabel(
              ui::DIALOG_BUTTON_CANCEL,
              l10n_util::GetStringUTF16(IDS_ENTERPRISE_SIGNIN_CANCEL))
          .SetModalType(ui::MODAL_TYPE_WINDOW)
          .SetAcceptCallback(base::BindOnce(
              notify_delegate, base::Unretained(this),
              prompt_for_new_profile_ ? &Delegate::OnSigninWithNewProfile
                                      : &Delegate::OnContinueSignin))
          .SetCancelCallback(base::BindOnce(notify_delegate,
                                            base::Unretained(this),
                                            &Delegate::OnCancelSignin));

  if (use_work_profile_wording_) {
    builder
        .SetTitle(IDS_ENTERPRISE_SIGNIN_WORK_PROFILE_TITLE)
        // Create business icon.
        .SetIcon(gfx::CreateVectorIcon(
            gfx::IconDescription(vector_icons::kBusinessIcon, kBusinessIconSize,
                                 gfx::kChromeIconGrey)))
        .SetShowIcon(true)
        .SetDefaultButton(ui::DIALOG_BUTTON_OK)
        .SetButtonLabel(ui::DIALOG_BUTTON_OK,
                        l10n_util::GetStringUTF16(
                            prompt_for_new_profile_
                                ? IDS_ENTERPRISE_SIGNIN_CREATE_NEW_WORK_PROFILE
                                : IDS_ENTERPRISE_SIGNIN_CONTINUE));
  } else {
    builder.SetShowCloseButton(false)
        .SetTitle(IDS_ENTERPRISE_SIGNIN_TITLE)
        .SetDefaultButton(ui::DIALOG_BUTTON_NONE)
        .SetButtonLabel(ui::DIALOG_BUTTON_OK,
                        l10n_util::GetStringUTF16(
                            prompt_for_new_profile_
                                ? IDS_ENTERPRISE_SIGNIN_CREATE_NEW_PROFILE
                                : IDS_ENTERPRISE_SIGNIN_CONTINUE));
    if (prompt_for_new_profile) {
      builder.SetExtraView(views::Builder<views::MdTextButton>()
                               .SetCallback(base::BindRepeating(
                                   &ProfileSigninConfirmationDialogViews::
                                       ContinueSigninButtonPressed,
                                   base::Unretained(this)))
                               .SetText(l10n_util::GetStringUTF16(
                                   IDS_ENTERPRISE_SIGNIN_CONTINUE)));
    }
  }

  std::move(builder).BuildChildren();

  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::PROFILE_SIGNIN_CONFIRMATION);
}

ProfileSigninConfirmationDialogViews::~ProfileSigninConfirmationDialogViews() {}

// static
void ProfileSigninConfirmationDialogViews::Show(
    Browser* browser,
    const std::string& username,
    std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate,
    bool prompt_for_new_profile) {
  auto dialog = std::make_unique<ProfileSigninConfirmationDialogViews>(
      browser, username, std::move(delegate), prompt_for_new_profile);
  constrained_window::CreateBrowserModalDialogViews(
      dialog.release(), browser->window()->GetNativeWindow())
      ->Show();
}

void ProfileSigninConfirmationDialogViews::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  views::DialogDelegateView::ViewHierarchyChanged(details);
  if (!details.is_add || details.child != this)
    return;

  if (use_work_profile_wording_)
    BuildWorkProfileView();
  else
    BuildDefaultView();
}

void ProfileSigninConfirmationDialogViews::BuildDefaultView() {
  DCHECK(!use_work_profile_wording_);

  size_t offset;
  std::vector<size_t> offsets;
  const std::u16string domain =
      base::ASCIIToUTF16(gaia::ExtractDomainName(username_));
  const std::u16string username = base::ASCIIToUTF16(username_);
  const std::u16string prompt_text =
      l10n_util::GetStringFUTF16(IDS_ENTERPRISE_SIGNIN_ALERT, domain, &offset);
  views::StyledLabel::RangeStyleInfo bold_style;
  bold_style.text_style = STYLE_EMPHASIZED;
  const std::u16string learn_more_text =
      l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  const std::u16string signin_explanation_text = l10n_util::GetStringFUTF16(
      prompt_for_new_profile_
          ? IDS_ENTERPRISE_SIGNIN_EXPLANATION_WITH_PROFILE_CREATION
          : IDS_ENTERPRISE_SIGNIN_EXPLANATION_WITHOUT_PROFILE_CREATION,
      username, learn_more_text, &offsets);
  const SkColor kPromptBarBackgroundColor =
      ui::GetSigninConfirmationPromptBarColor(GetColorProvider(), 0x0A);
  const gfx::Insets content_insets =
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kControl, views::DialogContentType::kText);

  views::Builder<ProfileSigninConfirmationDialogViews>(this)
      .SetBorder(views::CreateEmptyBorder(content_insets.top(), 0,
                                          content_insets.bottom(), 0))
      // Layout the components.
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          content_insets.top()))
      .AddChildren(
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetBetweenChildSpacing(
                  ChromeLayoutProvider::Get()->GetDistanceMetric(
                      views::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING))
              .SetBorder(views::CreateEmptyBorder(
                  ChromeLayoutProvider::Get()->GetInsetsMetric(
                      views::INSETS_DIALOG_SUBSECTION)))
              .SetBackground(
                  views::CreateSolidBackground(kPromptBarBackgroundColor))
              .AddChildren(
                  views::Builder<views::ImageView>().SetImage(
                      gfx::CreateVectorIcon(gfx::IconDescription(
                          vector_icons::kBusinessIcon, kBusinessIconSize,
                          gfx::kChromeIconGrey))),
                  views::Builder<views::StyledLabel>()
                      .SetDisplayedOnBackgroundColor(kPromptBarBackgroundColor)
                      .SetText(prompt_text)
                      .AddStyleRange(gfx::Range(offset, offset + domain.size()),
                                     bold_style)),
          views::Builder<views::StyledLabel>()
              .SetText(signin_explanation_text)
              .AddStyleRange(
                  gfx::Range(offsets[1], offsets[1] + learn_more_text.size()),
                  views::StyledLabel::RangeStyleInfo::CreateForLink(
                      base::BindRepeating(
                          &ProfileSigninConfirmationDialogViews::
                              LearnMoreClicked,
                          base::Unretained(this))))
              .SizeToFit(kPreferredWidth)
              .SetProperty(views::kMarginsKey,
                           gfx::Insets(0, content_insets.left(), 0,
                                       content_insets.right())))
      .BuildChildren();
}

void ProfileSigninConfirmationDialogViews::BuildWorkProfileView() {
  DCHECK(use_work_profile_wording_);

  // Define the explanation label text.
  size_t learn_more_offset;
  const std::u16string learn_more_text =
      l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  const std::u16string signin_explanation_text =
      l10n_util::GetStringFUTF16(IDS_ENTERPRISE_SIGNIN_WORK_PROFILE_EXPLANATION,
                                 learn_more_text, &learn_more_offset);
  const gfx::Insets content_insets =
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kControl, views::DialogContentType::kText);
  const gfx::Insets control_insets =
      gfx::Insets(0, content_insets.left(), 0, content_insets.right());

  views::Builder<ProfileSigninConfirmationDialogViews>(this)
      // The prompt bar needs to go to the edge of the dialog, so remove
      // horizontal insets.
      .SetBorder(views::CreateEmptyBorder(content_insets.top(), 0,
                                          content_insets.bottom(), 0))
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets(content_insets.top(), 0, 0, 0), 10))
      .AddChildren(
          // Create the explanation label first row.
          views::Builder<views::Label>()
              .SetText(l10n_util::GetStringUTF16(
                  IDS_ENTERPRISE_SIGNIN_WORK_PROFILE_CREATION))
              .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
              .SetMultiLine(true)
              .SizeToFit(kPreferredWidth)
              .SetProperty(views::kMarginsKey, control_insets),
          views::Builder<views::Label>()
              .SetText(l10n_util::GetStringUTF16(
                  IDS_ENTERPRISE_SIGNIN_WORK_PROFILE_ISOLATION_NOTICE))
              .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
              .SetProperty(views::kMarginsKey, control_insets),
          // Create the explanation label.
          views::Builder<views::StyledLabel>()
              .SetText(signin_explanation_text)
              .AddStyleRange(
                  gfx::Range(learn_more_offset,
                             learn_more_offset + learn_more_text.size()),
                  views::StyledLabel::RangeStyleInfo::CreateForLink(
                      base::BindRepeating(
                          &ProfileSigninConfirmationDialogViews::
                              LearnMoreClicked,
                          base::Unretained(this))))
              .SizeToFit(kPreferredWidth)
              .SetProperty(views::kMarginsKey, control_insets))
      .BuildChildren();
}

void ProfileSigninConfirmationDialogViews::ContinueSigninButtonPressed() {
  DCHECK(prompt_for_new_profile_);
  if (delegate_) {
    delegate_->OnContinueSignin();
    delegate_ = nullptr;
  }
  GetWidget()->Close();
}

void ProfileSigninConfirmationDialogViews::LearnMoreClicked(
    const ui::Event& event) {
  NavigateParams params(
      browser_, GURL("https://support.google.com/chromebook/answer/1331549"),
      ui::PAGE_TRANSITION_LINK);
  params.disposition = ui::DispositionFromEventFlags(
      event.flags(), WindowOpenDisposition::NEW_POPUP);
  params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&params);
}

BEGIN_METADATA(ProfileSigninConfirmationDialogViews, views::DialogDelegateView)
END_METADATA
