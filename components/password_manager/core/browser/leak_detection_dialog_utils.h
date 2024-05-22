// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DIALOG_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DIALOG_UTILS_H_

#include <string>
#include <type_traits>

#include "base/types/strong_alias.h"
#include "build/branding_buildflags.h"
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
  // Password is synced to a remote store (either syncing profile store or
  // account store).
  kPasswordSynced = 1 << 2,
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
// Creates CredentialLeakType from strong booleans.
CredentialLeakType CreateLeakType(IsSaved is_saved,
                                  IsReused is_reused,
                                  IsSyncing is_syncing);

// Checks whether the password is saved in Chrome.
bool IsPasswordSaved(CredentialLeakType leak_type);

// Checks whether the password is reused on other sites.
bool IsPasswordUsedOnOtherSites(CredentialLeakType leak_type);

// Checks whether the password is synced to a remote store (profile or account).
bool IsPasswordSynced(CredentialLeakType leak_type);

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

// Checks whether the leak dialog should show cancel button.
bool ShouldShowCancelButton(CredentialLeakType leak_type);

// Returns the LeakDialogType corresponding to |leak_type|.
metrics_util::LeakDialogType GetLeakDialogType(CredentialLeakType leak_type);

// Returns the URL used to launch the password checkup.
GURL GetPasswordCheckupURL(PasswordCheckupReferrer referrer =
                               PasswordCheckupReferrer::kLeakDetectionDialog);

// Returns whether to use Google Chrome branded strings.
constexpr bool UsesPasswordManagerGoogleBranding() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return false;
#endif
}

// Captures common traits needed for a leak dialog.
class LeakDialogTraits {
 public:
  LeakDialogTraits() = default;

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
};

// Creates a dialog traits object.
std::unique_ptr<LeakDialogTraits> CreateDialogTraits(
    CredentialLeakType leak_type);

template <metrics_util::LeakDialogType kDialogType>
class LeakDialogTraitsImp : public LeakDialogTraits {
 public:
  LeakDialogTraitsImp() = default;
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
  LeakDialogTraitsImp() = default;
  LeakDialogTraitsImp(const LeakDialogTraitsImp&) = delete;
  LeakDialogTraitsImp& operator=(const LeakDialogTraitsImp&) = delete;

  std::u16string GetAcceptButtonLabel() const override {
    return l10n_util::GetStringUTF16(IDS_LEAK_CHECK_CREDENTIALS);
  }

  std::u16string GetCancelButtonLabel() const override {
    return l10n_util::GetStringUTF16(IDS_CLOSE);
  }

  std::u16string GetDescription() const override {
    return l10n_util::GetStringUTF16(
        UsesPasswordManagerGoogleBranding()
            ? IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE_GPM_BRANDED
            : IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE_GPM_NON_BRANDED);
  }

  std::u16string GetTitle() const override {
    return l10n_util::GetStringUTF16(IDS_CREDENTIAL_LEAK_TITLE_CHECK_GPM);
  }

  bool ShouldCheckPasswords() const override { return true; }

  bool ShouldShowCancelButton() const override { return true; }
};

// Implementation of a leak change dialog.
template <>
class LeakDialogTraitsImp<metrics_util::LeakDialogType::kChange>
    : public LeakDialogTraits {
 public:
  LeakDialogTraitsImp() = default;
  LeakDialogTraitsImp(const LeakDialogTraitsImp&) = delete;
  LeakDialogTraitsImp& operator=(const LeakDialogTraitsImp&) = delete;

  std::u16string GetAcceptButtonLabel() const override {
    return l10n_util::GetStringUTF16(IDS_OK);
  }

  std::u16string GetCancelButtonLabel() const override {
    return l10n_util::GetStringUTF16(IDS_CLOSE);
  }

  std::u16string GetDescription() const override {
    return l10n_util::GetStringUTF16(
        UsesPasswordManagerGoogleBranding()
            ? IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE_GPM_BRANDED
            : IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE_GPM_NON_BRANDED);
  }

  std::u16string GetTitle() const override {
    return l10n_util::GetStringUTF16(IDS_CREDENTIAL_LEAK_TITLE_CHANGE);
  }

  bool ShouldCheckPasswords() const override { return false; }

  bool ShouldShowCancelButton() const override { return false; }
};

// Implementation of a leak checkup and change dialog.
template <>
class LeakDialogTraitsImp<metrics_util::LeakDialogType::kCheckupAndChange>
    : public LeakDialogTraits {
 public:
  LeakDialogTraitsImp() = default;
  LeakDialogTraitsImp(const LeakDialogTraitsImp&) = delete;
  LeakDialogTraitsImp& operator=(const LeakDialogTraitsImp&) = delete;

  std::u16string GetAcceptButtonLabel() const override {
    return l10n_util::GetStringUTF16(IDS_LEAK_CHECK_CREDENTIALS);
  }

  std::u16string GetCancelButtonLabel() const override {
    return l10n_util::GetStringUTF16(IDS_CLOSE);
  }

  std::u16string GetDescription() const override {
    return l10n_util::GetStringUTF16(
        UsesPasswordManagerGoogleBranding()
            ? IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE_GPM_BRANDED
            : IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE_GPM_NON_BRANDED);
  }

  std::u16string GetTitle() const override {
    return l10n_util::GetStringUTF16(IDS_CREDENTIAL_LEAK_TITLE_CHECK_GPM);
  }

  bool ShouldCheckPasswords() const override { return true; }

  bool ShouldShowCancelButton() const override { return true; }
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DIALOG_UTILS_H_
