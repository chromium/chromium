// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller_impl.h"

#include <array>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/user_selectable_type.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace autofill {

namespace {

// CanvasImageSource that combines a background image with user's avatar,
// the avatar is positioned and resized in terms of the background image DIPs,
// it also is cropped in a circle.
class MigrationHeaderImageSource : public gfx::CanvasImageSource {
 public:
  MigrationHeaderImageSource(const ui::ImageModel& image,
                             const ui::ImageModel& avatar,
                             const gfx::Point& avatar_position,
                             size_t avatar_size)
      : gfx::CanvasImageSource(image.Size()),
        image_(image),
        avatar_(avatar),
        avatar_position_(avatar_position),
        avatar_size_(avatar_size) {}

  MigrationHeaderImageSource(const MigrationHeaderImageSource&) = delete;
  MigrationHeaderImageSource& operator=(const MigrationHeaderImageSource&) =
      delete;

  ~MigrationHeaderImageSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;

 private:
  const ui::ImageModel image_;
  const ui::ImageModel avatar_;
  const gfx::Point avatar_position_;
  const size_t avatar_size_;
};

void MigrationHeaderImageSource::Draw(gfx::Canvas* canvas) {
  // Draw the background image first.
  gfx::ImageSkia image = image_.GetImage().AsImageSkia();
  canvas->DrawImageInt(image, 0, 0);

  // Setting a clippath makes subsequent avatar drawing cropped in a circle.
  SkPath avatar_bound = SkPath().addOval(
      SkRect::MakeXYWH(avatar_position_.x(), avatar_position_.y(),
                       /*w=*/avatar_size_, /*h=*/avatar_size_));
  canvas->ClipPath(avatar_bound, /*do_anti_alias=*/true);

  // Finally draw the avatar, above the background and cropped.
  gfx::ImageSkia avatar = gfx::ImageSkiaOperations::CreateResizedImage(
      avatar_.GetImage().AsImageSkia(),
      skia::ImageOperations::ResizeMethod::RESIZE_BEST,
      gfx::Size(avatar_size_, avatar_size_));
  canvas->DrawImageInt(avatar, avatar_position_.x(), avatar_position_.y());
}

ui::ImageModel EmbedAvatar(int background_id,
                           const ui::ImageModel& avatar,
                           const gfx::Point& position,
                           size_t size) {
  return ui::ImageModel::FromImageSkia(
      gfx::CanvasImageSource::MakeImageSkia<MigrationHeaderImageSource>(
          ui::ImageModel::FromResourceId(background_id), avatar, position,
          size));
}

}  // namespace

SaveUpdateAddressProfileBubbleControllerImpl::
    SaveUpdateAddressProfileBubbleControllerImpl(
        content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<
          SaveUpdateAddressProfileBubbleControllerImpl>(*web_contents),
      app_locale_(g_browser_process->GetApplicationLocale()) {}

SaveUpdateAddressProfileBubbleControllerImpl::
    ~SaveUpdateAddressProfileBubbleControllerImpl() {
  // `address_profile_save_prompt_callback_` must have been invoked before
  // destroying the controller to inform the backend of the output of the
  // save/update flow. It's either invoked upon user action when accepting
  // or rejecting the flow, or in cases when users ignore it, it's invoked
  // when the web contents are destroyed.
  DCHECK(address_profile_save_prompt_callback_.is_null());
}

void SaveUpdateAddressProfileBubbleControllerImpl::OfferSave(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    AutofillClient::SaveAddressProfilePromptOptions options,
    AutofillClient::AddressProfileSavePromptCallback
        address_profile_save_prompt_callback) {
  // Don't show the bubble if it's already visible, and inform the backend.
  if (bubble_view()) {
    std::move(address_profile_save_prompt_callback)
        .Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAutoDeclined,
             profile);
    return;
  }
  // If the user closed the bubble of the previous import process using the
  // "Close" button without making a decision to "Accept" or "Deny" the prompt,
  // a fallback icon is shown, so the user can get back to the prompt. In this
  // specific scenario the import process is considered in progress (since the
  // backend didn't hear back via the callback yet), but hidden. When a second
  // prompt arrives, we finish the previous import process as "Ignored", before
  // showing the 2nd prompt.
  if (address_profile_save_prompt_callback_) {
    std::move(address_profile_save_prompt_callback_)
        .Run(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
             address_profile_);
  }

  address_profile_ = profile;
  original_profile_ = base::OptionalFromPtr(original_profile);
  address_profile_save_prompt_callback_ =
      std::move(address_profile_save_prompt_callback);
  shown_by_user_gesture_ = false;
  is_migration_to_account_ = options.is_migration_to_account;
  if (options.show_prompt)
    Show();
}

std::u16string SaveUpdateAddressProfileBubbleControllerImpl::GetWindowTitle()
    const {
  if (IsSaveBubble()) {
    return l10n_util::GetStringUTF16(
        is_migration_to_account_
            ? IDS_AUTOFILL_ACCOUNT_MIGRATE_ADDRESS_PROMPT_TITLE
            : IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
  }

  return l10n_util::GetStringUTF16(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE);
}

absl::optional<SaveUpdateAddressProfileBubbleController::HeaderImages>
SaveUpdateAddressProfileBubbleControllerImpl::GetHeaderImages() const {
  if (is_migration_to_account_) {
    absl::optional<AccountInfo> account =
        GetPrimaryAccountInfoFromBrowserContext(
            web_contents()->GetBrowserContext());
    if (account) {
      ui::ImageModel avatar = ui::ImageModel::FromImage(account->account_image);
      // The position and size must match the implied one in the image,
      // so these numbers are exclusively for ..._AVATAR50_X135_Y54.
      static constexpr gfx::Point kAvatarPosition{135, 54};
      static constexpr size_t kAvatarSize{50};
      return HeaderImages{
          .light = EmbedAvatar(IDR_MIGRATE_ADDRESS_AVATAR50_X135_Y54, avatar,
                               kAvatarPosition, kAvatarSize),
          .dark = EmbedAvatar(IDR_MIGRATE_ADDRESS_AVATAR50_X135_Y54_DARK,
                              avatar, kAvatarPosition, kAvatarSize)};
    }
  } else if (IsSaveBubble()) {
    return HeaderImages{
        .light = ui::ImageModel::FromResourceId(IDR_SAVE_ADDRESS),
        .dark = ui::ImageModel::FromResourceId(IDR_SAVE_ADDRESS_DARK)};
  }

  return absl::nullopt;
}

std::u16string SaveUpdateAddressProfileBubbleControllerImpl::GetBodyText()
    const {
  if (is_migration_to_account_) {
    PersonalDataManager* pdm =
        ContentAutofillClient::FromWebContents(web_contents())
            ->GetPersonalDataManager();

    absl::optional<AccountInfo> account =
        GetPrimaryAccountInfoFromBrowserContext(
            web_contents()->GetBrowserContext());

    int string_id = pdm->IsSyncFeatureEnabledForAutofill()
                        ? IDS_AUTOFILL_SYNCABLE_PROFILE_MIGRATION_PROMPT_NOTICE
                        : IDS_AUTOFILL_LOCAL_PROFILE_MIGRATION_PROMPT_NOTICE;

    return l10n_util::GetStringFUTF16(string_id,
                                      base::UTF8ToUTF16(account->email));
  }

  return {};
}

std::u16string SaveUpdateAddressProfileBubbleControllerImpl::GetAddressSummary()
    const {
  if (!IsSaveBubble()) {
    return {};
  }
  // Use a shorter version of the address summary for migration, it has
  // a fixed set of fields and doesn't depend on libaddressinput.
  if (is_migration_to_account_) {
    static constexpr std::array fields = {
        ServerFieldType::NAME_FULL_WITH_HONORIFIC_PREFIX,
        ServerFieldType::ADDRESS_HOME_LINE1, ServerFieldType::EMAIL_ADDRESS,
        ServerFieldType::PHONE_HOME_WHOLE_NUMBER};
    std::vector<std::u16string> values;
    for (ServerFieldType field : fields) {
      std::u16string value = address_profile_.GetInfo(field, app_locale_);
      if (!value.empty()) {
        values.push_back(value);
      }
    }
    if (values.empty()) {
      return {};
    }
    return base::JoinString(values, u"\n");
  }

  return GetEnvelopeStyleAddress(address_profile_, app_locale_, true, true);
}

std::u16string SaveUpdateAddressProfileBubbleControllerImpl::GetProfileEmail()
    const {
  // Email is not shown as a separate field in the migration flow,
  // it is included in the address summary, see GetAddressSummary().
  if (is_migration_to_account_ || !IsSaveBubble()) {
    return {};
  }

  return address_profile_.GetInfo(EMAIL_ADDRESS, app_locale_);
}

std::u16string SaveUpdateAddressProfileBubbleControllerImpl::GetProfilePhone()
    const {
  // Phone is not shown as a separate field in the migration flow,
  // it is included in the address summary, see GetAddressSummary().
  if (is_migration_to_account_ || !IsSaveBubble()) {
    return {};
  }

  return address_profile_.GetInfo(PHONE_HOME_WHOLE_NUMBER, app_locale_);
}

std::u16string SaveUpdateAddressProfileBubbleControllerImpl::GetOkButtonLabel()
    const {
  return l10n_util::GetStringUTF16(
      is_migration_to_account_
          ? IDS_AUTOFILL_MIGRATE_ADDRESS_DIALOG_OK_BUTTON_LABEL_SAVE
          : IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_OK_BUTTON_LABEL_SAVE);
}

AutofillClient::SaveAddressProfileOfferUserDecision
SaveUpdateAddressProfileBubbleControllerImpl::GetCancelCallbackValue() const {
  // The migration prompt should not be shown again if the user explicitly
  // rejects it (for a particular address, due to legal and privacy
  // requirements). In other cases it is acceptable to show it a few times more.
  return is_migration_to_account_
             ? AutofillClient::SaveAddressProfileOfferUserDecision::kNever
             : AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined;
}

std::u16string SaveUpdateAddressProfileBubbleControllerImpl::GetFooterMessage()
    const {
  if (address_profile_.source() == AutofillProfile::Source::kAccount) {
    absl::optional<AccountInfo> account =
        GetPrimaryAccountInfoFromBrowserContext(
            web_contents()->GetBrowserContext());

    int string_id =
        IsSaveBubble()
            ? IDS_AUTOFILL_SAVE_IN_ACCOUNT_PROMPT_ADDRESS_SOURCE_NOTICE
            : IDS_AUTOFILL_UPDATE_PROMPT_ACCOUNT_ADDRESS_SOURCE_NOTICE;
    return l10n_util::GetStringFUTF16(string_id,
                                      base::UTF8ToUTF16(account->email));
  }

  return {};
}

const AutofillProfile&
SaveUpdateAddressProfileBubbleControllerImpl::GetProfileToSave() const {
  return address_profile_;
}

const AutofillProfile*
SaveUpdateAddressProfileBubbleControllerImpl::GetOriginalProfile() const {
  return base::OptionalToPtr(original_profile_);
}

void SaveUpdateAddressProfileBubbleControllerImpl::OnUserDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision,
    AutofillProfile profile) {
  if (address_profile_save_prompt_callback_) {
    std::move(address_profile_save_prompt_callback_).Run(decision, profile);
  }
}

void SaveUpdateAddressProfileBubbleControllerImpl::OnUserCanceledEditing() {
  shown_by_user_gesture_ = false;
  Show();
}

void SaveUpdateAddressProfileBubbleControllerImpl::OnEditButtonClicked() {
  EditAddressProfileDialogControllerImpl::CreateForWebContents(web_contents());
  EditAddressProfileDialogControllerImpl* controller =
      EditAddressProfileDialogControllerImpl::FromWebContents(web_contents());
  controller->OfferEdit(
      address_profile_, GetOriginalProfile(), GetEditorFooterMessage(),
      base::BindOnce(&SaveUpdateAddressProfileBubbleController::OnUserDecision,
                     GetWeakPtr()),
      base::BindOnce(
          &SaveUpdateAddressProfileBubbleController::OnUserCanceledEditing,
          GetWeakPtr()),
      is_migration_to_account_);
  HideBubble();
}

void SaveUpdateAddressProfileBubbleControllerImpl::OnBubbleClosed() {
  set_bubble_view(nullptr);
  UpdatePageActionIcon();
}

void SaveUpdateAddressProfileBubbleControllerImpl::OnPageActionIconClicked() {
  // Don't show the bubble if it's already visible.
  if (bubble_view())
    return;
  shown_by_user_gesture_ = true;
  Show();
}

bool SaveUpdateAddressProfileBubbleControllerImpl::IsBubbleActive() const {
  return !address_profile_save_prompt_callback_.is_null();
}

std::u16string
SaveUpdateAddressProfileBubbleControllerImpl::GetPageActionIconTootip() const {
  return GetWindowTitle();
}

AutofillBubbleBase*
SaveUpdateAddressProfileBubbleControllerImpl::GetBubbleView() const {
  return bubble_view();
}

bool SaveUpdateAddressProfileBubbleControllerImpl::IsSaveBubble() const {
  return !original_profile_;
}

void SaveUpdateAddressProfileBubbleControllerImpl::WebContentsDestroyed() {
  AutofillBubbleControllerBase::WebContentsDestroyed();

  OnUserDecision(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
                 address_profile_);
}

PageActionIconType
SaveUpdateAddressProfileBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kSaveAutofillAddress;
}

void SaveUpdateAddressProfileBubbleControllerImpl::DoShowBubble() {
  DCHECK(!bubble_view());
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (IsSaveBubble()) {
    set_bubble_view(browser->window()
                        ->GetAutofillBubbleHandler()
                        ->ShowSaveAddressProfileBubble(web_contents(), this,
                                                       shown_by_user_gesture_));
  } else {
    // This is an update prompt.
    set_bubble_view(browser->window()
                        ->GetAutofillBubbleHandler()
                        ->ShowUpdateAddressProfileBubble(
                            web_contents(), this, shown_by_user_gesture_));
  }
  DCHECK(bubble_view());
}

std::u16string
SaveUpdateAddressProfileBubbleControllerImpl::GetEditorFooterMessage() const {
  if (is_migration_to_account_) {
    absl::optional<AccountInfo> account =
        GetPrimaryAccountInfoFromBrowserContext(
            web_contents()->GetBrowserContext());
    return l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_SAVE_IN_ACCOUNT_PROMPT_ADDRESS_SOURCE_NOTICE,
        base::UTF8ToUTF16(account->email));
  }

  return GetFooterMessage();
}

base::WeakPtr<SaveUpdateAddressProfileBubbleController>
SaveUpdateAddressProfileBubbleControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SaveUpdateAddressProfileBubbleControllerImpl);

}  // namespace autofill
