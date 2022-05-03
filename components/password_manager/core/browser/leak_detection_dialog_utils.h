// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DIALOG_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DIALOG_UTILS_H_

#include <string>
#include <type_traits>

#include "base/types/strong_alias.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace password_manager {

// Defines possible scenarios for leaked credentials.
enum CredentialLeakFlags {
  // Password is saved for current site.
  kPasswordSaved = 1 << 0,
  // Password is reused on other sites.
  kPasswordUsedOnOtherSites = 1 << 1,
  // User is syncing passwords with normal encryption.
  kSyncingPasswordsNormally = 1 << 2,
  // There is an automatic password change script available for this credential.
  kAutomaticPasswordChangeScriptAvailable = 1 << 3,
};

enum class PasswordCheckupReferrer {
  // Corresponds to the leak detection dialog shown on Desktop and Mobile.
  kLeakDetectionDialog = 0,
  // Corresponds to Chrome's password check page on Desktop.
  kPasswordCheck = 1,
};

// Contains combination of CredentialLeakFlags values.
using CredentialLeakType = std::underlying_type_t<CredentialLeakFlags>;

using IsSaved = base::StrongAlias<class IsSavedTag, bool>;
using IsReused = base::StrongAlias<class IsReusedTag, bool>;
using IsSyncing = base::StrongAlias<class IsSyncingTag, bool>;
using HasChangeScript = base::StrongAlias<class HasChangeScriptTag, bool>;
// Creates CredentialLeakType from strong booleans.
CredentialLeakType CreateLeakType(IsSaved is_saved,
                                  IsReused is_reused,
                                  IsSyncing is_syncing,
                                  HasChangeScript has_change_script);

// Checks whether password is saved in chrome.
bool IsPasswordSaved(CredentialLeakType leak_type);

// Checks whether password is reused on other sites.
bool IsPasswordUsedOnOtherSites(CredentialLeakType leak_type);

// Checks whether user is syncing passwords with normal encryption.
bool IsSyncingPasswordsNormally(CredentialLeakType leak_type);

// Checks whether an automatic password change script is available for the
// credential.
bool IsAutomaticPasswordChangeScriptAvailable(CredentialLeakType leak_type);

// Returns the label for the leak dialog accept button.
std::u16string GetAcceptButtonLabel(CredentialLeakType leak_type);

// Returns the label for the leak dialog cancel button.
std::u16string GetCancelButtonLabel(CredentialLeakType leak_type);

// Returns the leak dialog message based on leak type.
std::u16string GetDescription(CredentialLeakType leak_type);

// Returns the leak dialog title based on leak type.
std::u16string GetTitle(CredentialLeakType leak_type);

// Returns the leak dialog tooltip shown on (?) click.
std::u16string GetLeakDetectionTooltip();

// Checks whether the leak dialog should prompt user to password checkup.
bool ShouldCheckPasswords(CredentialLeakType leak_type);

// Checks whether the leak dialog should show automatic change password button.
bool ShouldShowAutomaticChangePasswordButton(CredentialLeakType leak_type);

// Checks whether the leak dialog should show cancel button.
bool ShouldShowCancelButton(CredentialLeakType leak_type);

// Returns the LeakDialogType corresponding to |leak_type|.
metrics_util::LeakDialogType GetLeakDialogType(CredentialLeakType leak_type);

// Returns the URL used to launch the password checkup.
GURL GetPasswordCheckupURL(PasswordCheckupReferrer referrer =
                               PasswordCheckupReferrer::kLeakDetectionDialog);

// Captures common traits needed for a leak dialog.
class LeakDialogTraits {
 public:
  explicit LeakDialogTraits(CredentialLeakType leak_type);

  virtual ~LeakDialogTraits() = default;

  // Returns the label for the accept button.
  virtual std::u16string GetAcceptButtonLabel() const = 0;

  // Returns the label for the cancel button.
  virtual std::u16string GetCancelButtonLabel() const = 0;

  // Returns the dialog message based on credential leak type.
  virtual std::u16string GetDescription() const = 0;

  // Returns the dialog title based on credential leak type.
  virtual std::u16string GetTitle() const = 0;

  // Checks whether the dialog should prompt user to password checkup.
  virtual bool ShouldCheckPasswords() const = 0;

  // Checks whether the dialog should show cancel button.
  virtual bool ShouldShowCancelButton() const = 0;

 protected:
  bool uses_password_manager_updated_naming() const {
    return uses_password_manager_updated_naming_;
  }

  bool uses_password_manager_google_branding() const {
    return uses_password_manager_google_branding_;
  }

 private:
  // Set iff Unified Password Manager / Updated branding strings are used.
  const bool uses_password_manager_updated_naming_;
  // Set iff Google Chrome Branding strings are used.
  const bool uses_password_manager_google_branding_;
};

// Creates a dialog traits object.
std::unique_ptr<LeakDialogTraits> CreateDialogTraits(
    CredentialLeakType leak_type);

template <metrics_util::LeakDialogType kDialogType>
class LeakDialogTraitsImp : public LeakDialogTraits {
 public:
  explicit LeakDialogTraitsImp(CredentialLeakType leak_type)
      : LeakDialogTraits(leak_type) {}
  LeakDialogTraitsImp(const LeakDialogTraitsImp&) = delete;
  LeakDialogTraitsImp& operator=(const LeakDialogTraitsImp&) = delete;

  std::u16string GetAcceptButtonLabel() const override;
  std::u16string GetCancelButtonLabel() const override;
  std::u16string GetDescription() const override;
  std::u16string GetTitle() const override;
  bool ShouldCheckPasswords() const override;
  bool ShouldShowCancelButton() const override;
};

// Implementation of a leak checkup dialog.
template <>
class LeakDialogTraitsImp<metrics_util::LeakDialogType::kCheckup>
    : public LeakDialogTraits {
 public:
  explicit LeakDialogTraitsImp(CredentialLeakType leak_type)
      : LeakDialogTraits(leak_type) {}
  LeakDialogTraitsImp(const LeakDialogTraitsImp&) = delete;
  LeakDialogTraitsImp& operator=(const LeakDialogTraitsImp&) = delete;

  std::u16string GetAcceptButtonLabel() const override {
    return l10n_util::GetStringUTF16(IDS_LEAK_CHECK_CREDENTIALS);
  }

  std::u16string GetCancelButtonLabel() const override {
    return l10n_util::GetStringUTF16(IDS_CLOSE);
  }

  std::u16string GetDescription() const override {
    if (uses_password_manager_updated_naming()) {
      return l10n_util::GetStringUTF16(
          uses_password_manager_google_branding()
              ? IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE_GPM_BRANDED
              : IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE_GPM_NON_BRANDED);
    } else {
      return l10n_util::GetStringUTF16(
          IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE);
    }
  }

  std::u16string GetTitle() const override {
    return l10n_util::GetStringUTF16(uses_password_manager_updated_naming()
                                         ? IDS_CREDENTIAL_LEAK_TITLE_CHECK_GPM
                                         : IDS_CREDENTIAL_LEAK_TITLE_CHECK);
  }

  bool ShouldCheckPasswords() const override { return true; }

  bool ShouldShowCancelButton() const override { return true; };
};

// Implementation of a leak change dialog.
template <>
class LeakDialogTraitsImp<metrics_util::LeakDialogType::kChange>
    : public LeakDialogTraits {
 public:
  explicit LeakDialogTraitsImp(CredentialLeakType leak_type)
      : LeakDialogTraits(leak_type) {}
  LeakDialogTraitsImp(const LeakDialogTraitsImp&) = delete;
  LeakDialogTraitsImp& operator=(const LeakDialogTraitsImp&) = delete;

  std::u16string GetAcceptButtonLabel() const override {
    return l10n_util::GetStringUTF16(IDS_OK);
  }

  std::u16string GetCancelButtonLabel() const override {
    return l10n_util::GetStringUTF16(IDS_CLOSE);
  }

  std::u16string GetDescription() const override {
    if (uses_password_manager_updated_naming()) {
      return l10n_util::GetStringUTF16(
          uses_password_manager_google_branding()
              ? IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE_GPM_BRANDED
              : IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE_GPM_NON_BRANDED);
    } else {
      return l10n_util::GetStringUTF16(
          IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE);
    }
  }

  std::u16string GetTitle() const override {
    return l10n_util::GetStringUTF16(IDS_CREDENTIAL_LEAK_TITLE_CHANGE);
  }

  bool ShouldCheckPasswords() const override { return false; }

  bool ShouldShowCancelButton() const override { return false; };
};

// Implementation of a leak checkup and change dialog.
template <>
class LeakDialogTraitsImp<metrics_util::LeakDialogType::kCheckupAndChange>
    : public LeakDialogTraits {
 public:
  explicit LeakDialogTraitsImp(CredentialLeakType leak_type)
      : LeakDialogTraits(leak_type) {}
  LeakDialogTraitsImp(const LeakDialogTraitsImp&) = delete;
  LeakDialogTraitsImp& operator=(const LeakDialogTraitsImp&) = delete;

  std::u16string GetAcceptButtonLabel() const override {
    return l10n_util::GetStringUTF16(IDS_LEAK_CHECK_CREDENTIALS);
  }

  std::u16string GetCancelButtonLabel() const override {
    return l10n_util::GetStringUTF16(IDS_CLOSE);
  }

  std::u16string GetDescription() const override {
    if (uses_password_manager_updated_naming())
      return l10n_util::GetStringUTF16(
          uses_password_manager_google_branding()
              ? IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE_GPM_BRANDED
              : IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE_GPM_NON_BRANDED);
    else
      return l10n_util::GetStringUTF16(
          IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE);
  }

  std::u16string GetTitle() const override {
    return l10n_util::GetStringUTF16(uses_password_manager_updated_naming()
                                         ? IDS_CREDENTIAL_LEAK_TITLE_CHECK_GPM
                                         : IDS_CREDENTIAL_LEAK_TITLE_CHECK);
  }

  bool ShouldCheckPasswords() const override { return true; }

  bool ShouldShowCancelButton() const override { return true; };
};

// Implementation of a leak automatic change dialog.
template <>
class LeakDialogTraitsImp<metrics_util::LeakDialogType::kChangeAutomatically>
    : public LeakDialogTraits {
 public:
  explicit LeakDialogTraitsImp(CredentialLeakType leak_type)
      : LeakDialogTraits(leak_type) {}
  LeakDialogTraitsImp(const LeakDialogTraitsImp&) = delete;
  LeakDialogTraitsImp& operator=(const LeakDialogTraitsImp&) = delete;

  std::u16string GetAcceptButtonLabel() const override {
    return l10n_util::GetStringUTF16(IDS_CREDENTIAL_LEAK_CHANGE_AUTOMATICALLY);
  }

  std::u16string GetCancelButtonLabel() const override {
    return l10n_util::GetStringUTF16(IDS_CLOSE);
  }

  std::u16string GetDescription() const override {
    return l10n_util::GetStringUTF16(
        uses_password_manager_updated_naming()
            ? IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_AUTOMATICALLY_MESSAGE_GPM
            : IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_AUTOMATICALLY_MESSAGE);
  }

  std::u16string GetTitle() const override {
    return l10n_util::GetStringUTF16(
        IDS_CREDENTIAL_LEAK_TITLE_CHANGE_AUTOMATICALLY);
  }

  bool ShouldCheckPasswords() const override { return false; }

  bool ShouldShowCancelButton() const override { return true; };
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DIALOG_UTILS_H_
