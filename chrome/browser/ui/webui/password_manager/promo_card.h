// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARD_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARD_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;
class Profile;

namespace extensions {
class PasswordsPrivateDelegate;
}  // namespace extensions

namespace syncer {
class SyncService;
}

namespace password_manager {

// Interface for the promo cards. It has a basic implementation to read/write
// to PrefService as well as basic properties needed for each promo card. Each
// subclass must override GetPromoID() and the content to be displayed.
class PromoCardInterface {
 public:
  static std::vector<std::unique_ptr<PromoCardInterface>>
  GetAllPromoCardsForProfile(Profile* profile);

  PromoCardInterface(const PromoCardInterface&) = delete;
  PromoCardInterface& operator=(const PromoCardInterface&) = delete;

  virtual ~PromoCardInterface();

  // Unique ID for a promo card. This is also used by the WebUI to display
  // banner image.
  virtual std::string GetPromoID() const = 0;

  // Whether promo can be shown. For most of the promos once it's dismissed it
  // can't be shown again.
  virtual bool ShouldShowPromo() const = 0;

  // Title of the promo card to be shown in the WebUI.
  virtual std::u16string GetTitle() const = 0;

  // Description of the promo card to be shown in the WebUI.
  virtual std::u16string GetDescription() const = 0;

  // Text for an actionable button if one exists. Returns empty string by
  // default.
  virtual std::u16string GetActionButtonText() const;

  void OnPromoCardDismissed();
  void OnPromoCardShown();

  base::Time last_time_shown() const { return last_time_shown_; }

 protected:
  PromoCardInterface(const std::string& id, PrefService* prefs);

  int number_of_times_shown_ = 0;
  base::Time last_time_shown_;
  bool was_dismissed_ = false;

 private:
  raw_ptr<PrefService> prefs_;
};

// Password checkup promo card. Despite other promo cards this one should be
// shown regularly but not more often than kPasswordCheckupPromoPeriod.
class PasswordCheckupPromo : public PromoCardInterface {
 public:
  PasswordCheckupPromo(PrefService* prefs,
                       extensions::PasswordsPrivateDelegate* delegate);
  ~PasswordCheckupPromo() override;

 private:
  // PromoCardInterface implementation.
  std::string GetPromoID() const override;
  bool ShouldShowPromo() const override;
  std::u16string GetTitle() const override;
  std::u16string GetDescription() const override;
  std::u16string GetActionButtonText() const override;

  raw_ptr<extensions::PasswordsPrivateDelegate> delegate_ = nullptr;
};

// Promoting web version of Password Manager. Has a link to the website in the
// description.
class WebPasswordManagerPromo : public PromoCardInterface {
 public:
  WebPasswordManagerPromo(PrefService* prefs,
                          const syncer::SyncService* sync_service);

 private:
  // PromoCardInterface implementation.
  std::string GetPromoID() const override;
  bool ShouldShowPromo() const override;
  std::u16string GetTitle() const override;
  std::u16string GetDescription() const override;

  bool sync_enabled_ = false;
};

// Promo card to create shortcut to the Password Manager.
class PasswordManagerShortcutPromo : public PromoCardInterface {
 public:
  explicit PasswordManagerShortcutPromo(Profile* profile);

 private:
  // PromoCardInterface implementation.
  std::string GetPromoID() const override;
  bool ShouldShowPromo() const override;
  std::u16string GetTitle() const override;
  std::u16string GetDescription() const override;
  std::u16string GetActionButtonText() const override;

  bool is_shortcut_installed_ = false;
};

// Promo card to communicate how to use Password Manager on Android and iOS.
class AccessOnAnyDevicePromo : public PromoCardInterface {
 public:
  explicit AccessOnAnyDevicePromo(PrefService* prefs);

 private:
  // PromoCardInterface implementation.
  std::string GetPromoID() const override;
  bool ShouldShowPromo() const override;
  std::u16string GetTitle() const override;
  std::u16string GetDescription() const override;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARD_H_
