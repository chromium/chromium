// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_notice_bubble.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

using PromptAction = PrivacySandboxService::PromptAction;

namespace {

class PrivacySandboxNoticeBubbleModelDelegate : public ui::DialogModelDelegate {
 public:
  explicit PrivacySandboxNoticeBubbleModelDelegate(Browser* browser)
      : browser_(browser) {
    // Saving a reference to the service as an attempt to fix crashes when
    // getting a service later on (crbug.com/1379327, crbug.com/1380342,
    // crbug.com/1370708). Crashes seem to be caused by browser_ reference not
    // being valid.
    privacy_sandbox_service_ =
        PrivacySandboxServiceFactory::GetForProfile(browser_->profile());

    if (privacy_sandbox_service_)
      privacy_sandbox_service_->PromptOpenedForBrowser(browser_);
    NotifyServiceAboutPromptAction(PromptAction::kNoticeShown);
  }

  void OnDialogDestroying() {
    if (!has_user_interacted_)
      NotifyServiceAboutPromptAction(PromptAction::kNoticeClosedNoInteraction);

    if (privacy_sandbox_service_)
      privacy_sandbox_service_->PromptClosedForBrowser(browser_);
  }

  void OnOkButtonPressed() {
    has_user_interacted_ = true;
    NotifyServiceAboutPromptAction(PromptAction::kNoticeAcknowledge);
  }

  void OnSettingsLinkPressed() {
    has_user_interacted_ = true;
    chrome::ShowPrivacySandboxSettings(browser_);
    NotifyServiceAboutPromptAction(PromptAction::kNoticeOpenSettings);
  }

  void OnLearnMoreLinkPressed() {
    has_user_interacted_ = true;
    chrome::ShowPrivacySandboxLearnMore(browser_);
    NotifyServiceAboutPromptAction(PromptAction::kNoticeLearnMore);
  }

  void OnDialogExplicitlyClosed() {
    if (!has_user_interacted_)
      NotifyServiceAboutPromptAction(PromptAction::kNoticeDismiss);
    has_user_interacted_ = true;
  }

  void NotifyServiceAboutPromptAction(PromptAction action) {
    if (privacy_sandbox_service_)
      privacy_sandbox_service_->PromptActionOccurred(action);
  }

 private:
  raw_ptr<Browser> browser_;
  raw_ptr<PrivacySandboxService> privacy_sandbox_service_;
  bool has_user_interacted_ = false;
};

}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kPrivacySandboxLearnMoreTextForTesting);

// static
void ShowPrivacySandboxNoticeBubble(Browser* browser) {
  auto bubble_delegate_unique =
      std::make_unique<PrivacySandboxNoticeBubbleModelDelegate>(browser);
  PrivacySandboxNoticeBubbleModelDelegate* bubble_delegate =
      bubble_delegate_unique.get();
  auto& bundle = ui::ResourceBundle::GetSharedInstance();
  auto dialog_model =
      ui::DialogModel::Builder(std::move(bubble_delegate_unique))
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_PRIVACY_SANDBOX_BUBBLE_NOTICE_TITLE))
          .SetInternalName(kPrivacySandboxNoticeBubbleName)
          .SetIsAlertDialog()
          .SetIcon(ui::ImageModel::FromImageSkia(*bundle.GetImageSkiaNamed(
                       IDR_PRIVACY_SANDBOX_CONFIRMATION_BANNER)),
                   ui::ImageModel::FromImageSkia(*bundle.GetImageSkiaNamed(
                       IDR_PRIVACY_SANDBOX_CONFIRMATION_BANNER_DARK)))
          .AddParagraph(
              ui::DialogModelLabel::CreateWithReplacement(
                  IDS_PRIVACY_SANDBOX_BUBBLE_NOTICE_DESCRIPTION,
                  ui::DialogModelLabel::CreateLink(
                      IDS_PRIVACY_SANDBOX_BUBBLE_NOTICE_DESCRIPTION_ESTIMATES_INTERESTS_LINK,
                      base::BindRepeating(
                          &PrivacySandboxNoticeBubbleModelDelegate::
                              OnLearnMoreLinkPressed,
                          base::Unretained(bubble_delegate)),
                      l10n_util::GetStringUTF16(
                          IDS_PRIVACY_SANDBOX_BUBBLE_NOTICE_DESCRIPTION_ESTIMATES_INTERESTS_LINK_A11Y_NAME))),
              std::u16string(), kPrivacySandboxLearnMoreTextForTesting)
          .AddOkButton(
              base::BindRepeating(
                  &PrivacySandboxNoticeBubbleModelDelegate::OnOkButtonPressed,
                  base::Unretained(bubble_delegate)),
              ui::DialogModelButton::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_PRIVACY_SANDBOX_DIALOG_NOTICE_ACKNOWLEDGE_BUTTON)))
          .AddExtraLink(ui::DialogModelLabel::CreateLink(
              IDS_PRIVACY_SANDBOX_BUBBLE_NOTICE_SETTINGS_LINK,
              base::BindRepeating(&PrivacySandboxNoticeBubbleModelDelegate::
                                      OnSettingsLinkPressed,
                                  base::Unretained(bubble_delegate))))
          .SetCloseActionCallback(
              base::BindOnce(&PrivacySandboxNoticeBubbleModelDelegate::
                                 OnDialogExplicitlyClosed,
                             base::Unretained(bubble_delegate)))
          .SetDialogDestroyingCallback(base::BindOnce(
              &PrivacySandboxNoticeBubbleModelDelegate::OnDialogDestroying,
              base::Unretained(bubble_delegate)))
          .Build();

  bubble_anchor_util::AnchorConfiguration configuration =
      bubble_anchor_util::GetAppMenuAnchorConfiguration(browser);
  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), configuration.anchor_view,
      configuration.bubble_arrow);
  views::Widget* const widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
  // The bubble isn't opened as a result of a user action. It shouldn't take the
  // focus away from the current task. Open the bubble starting as inactive
  // also avoids unintentionally closing it due to focus loss until the user has
  // interacted with it. After a user interaction, it may be closed on focus
  // loss.
  widget->ShowInactive();
  widget->GetRootView()->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(IDS_SHOW_BUBBLE_INACTIVE_DESCRIPTION));
}
