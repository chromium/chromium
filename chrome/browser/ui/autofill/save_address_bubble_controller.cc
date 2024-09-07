// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_address_bubble_controller.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPath.h"
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
  // Note that some testing profiles do not have an avatar.
  if (!avatar_.IsEmpty()) {
    gfx::ImageSkia avatar = gfx::ImageSkiaOperations::CreateResizedImage(
        avatar_.GetImage().AsImageSkia(),
        skia::ImageOperations::ResizeMethod::RESIZE_BEST,
        gfx::Size(avatar_size_, avatar_size_));
    canvas->DrawImageInt(avatar, avatar_position_.x(), avatar_position_.y());
  }
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

SaveAddressBubbleController::SaveAddressBubbleController(
    base::WeakPtr<AddressBubbleControllerDelegate> delegate,
    content::WebContents* web_contents,
    const AutofillProfile& address_profile,
    bool is_migration_to_account)
    : content::WebContentsObserver(web_contents),
      delegate_(delegate),
      address_profile_(address_profile),
      is_migration_to_account_(is_migration_to_account) {}

SaveAddressBubbleController::~SaveAddressBubbleController() = default;

std::u16string SaveAddressBubbleController::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(
      is_migration_to_account_
          ? IDS_AUTOFILL_ACCOUNT_MIGRATE_ADDRESS_PROMPT_TITLE
          : IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
}

std::optional<SaveAddressBubbleController::HeaderImages>
SaveAddressBubbleController::GetHeaderImages() const {
  if (is_migration_to_account_ && web_contents()) {
    std::optional<AccountInfo> account =
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
  }

  return HeaderImages{
      .light = ui::ImageModel::FromResourceId(IDR_SAVE_ADDRESS),
      .dark = ui::ImageModel::FromResourceId(IDR_SAVE_ADDRESS_DARK)};
}

std::u16string SaveAddressBubbleController::GetBodyText() const {
  if (is_migration_to_account_ && web_contents()) {
    PersonalDataManager* pdm =
        ContentAutofillClient::FromWebContents(web_contents())
            ->GetPersonalDataManager();

    std::optional<AccountInfo> account =
        GetPrimaryAccountInfoFromBrowserContext(
            web_contents()->GetBrowserContext());

    int string_id =
        pdm->address_data_manager().IsSyncFeatureEnabledForAutofill()
            ? IDS_AUTOFILL_SYNCABLE_PROFILE_MIGRATION_PROMPT_NOTICE
            : IDS_AUTOFILL_LOCAL_PROFILE_MIGRATION_PROMPT_NOTICE;

    return l10n_util::GetStringFUTF16(string_id,
                                      base::UTF8ToUTF16(account->email));
  }

  return {};
}

std::u16string SaveAddressBubbleController::GetAddressSummary() const {
  // Use a shorter version of the address summary for migration, it has
  // a fixed set of fields and doesn't depend on libaddressinput.
  if (is_migration_to_account_) {
    static constexpr std::array fields = {
        NAME_FULL, ADDRESS_HOME_LINE1, EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER};
    std::vector<std::u16string> values;
    for (FieldType field : fields) {
      std::u16string value = address_profile_.GetInfo(
          field, g_browser_process->GetApplicationLocale());
      if (!value.empty()) {
        values.push_back(value);
      }
    }
    if (values.empty()) {
      return {};
    }
    return base::JoinString(values, u"\n");
  }

  return GetEnvelopeStyleAddress(
      address_profile_, g_browser_process->GetApplicationLocale(),
      /*include_recipient=*/true, /*include_country=*/true);
}

std::u16string SaveAddressBubbleController::GetProfileEmail() const {
  // Email is not shown as a separate field in the migration flow,
  // it is included in the address summary, see GetAddressSummary().
  if (is_migration_to_account_) {
    return {};
  }

  return address_profile_.GetInfo(EMAIL_ADDRESS,
                                  g_browser_process->GetApplicationLocale());
}

std::u16string SaveAddressBubbleController::GetProfilePhone() const {
  // Phone is not shown as a separate field in the migration flow,
  // it is included in the address summary, see GetAddressSummary().
  if (is_migration_to_account_) {
    return {};
  }

  return address_profile_.GetInfo(PHONE_HOME_WHOLE_NUMBER,
                                  g_browser_process->GetApplicationLocale());
}

std::u16string SaveAddressBubbleController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(
      is_migration_to_account_
          ? IDS_AUTOFILL_MIGRATE_ADDRESS_DIALOG_OK_BUTTON_LABEL_SAVE
          : IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_OK_BUTTON_LABEL_SAVE);
}

AutofillClient::AddressPromptUserDecision
SaveAddressBubbleController::GetCancelCallbackValue() const {
  // The migration prompt should not be shown again if the user explicitly
  // rejects it (for a particular address, due to legal and privacy
  // requirements). In other cases it is acceptable to show it a few times more.
  return is_migration_to_account_
             ? AutofillClient::AddressPromptUserDecision::kNever
             : AutofillClient::AddressPromptUserDecision::kDeclined;
}

std::u16string SaveAddressBubbleController::GetFooterMessage() const {
  if (address_profile_.IsAccountProfile() && web_contents()) {
    std::optional<AccountInfo> account =
        GetPrimaryAccountInfoFromBrowserContext(
            web_contents()->GetBrowserContext());

    return l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_SAVE_IN_ACCOUNT_PROMPT_ADDRESS_SOURCE_NOTICE,
        base::UTF8ToUTF16(account->email));
  }

  return {};
}

std::u16string SaveAddressBubbleController::GetEditorFooterMessage() const {
  if (is_migration_to_account_ && web_contents()) {
    std::optional<AccountInfo> account =
        GetPrimaryAccountInfoFromBrowserContext(
            web_contents()->GetBrowserContext());
    return l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_SAVE_IN_ACCOUNT_PROMPT_ADDRESS_SOURCE_NOTICE,
        base::UTF8ToUTF16(account->email));
  }

  return GetFooterMessage();
}

void SaveAddressBubbleController::OnUserDecision(
    AutofillClient::AddressPromptUserDecision decision,
    base::optional_ref<const AutofillProfile> profile) {
  if (delegate_) {
    delegate_->OnUserDecision(decision, profile);
  }
}

void SaveAddressBubbleController::OnEditButtonClicked() {
  if (delegate_) {
    delegate_->ShowEditor(address_profile_, /*title_override=*/u"",
                          GetEditorFooterMessage(),
                          /*is_editing_existing_address=*/false);
  }
}

void SaveAddressBubbleController::OnBubbleClosed() {
  if (delegate_) {
    delegate_->OnBubbleClosed();
  }
}

}  // namespace autofill
