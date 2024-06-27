// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARD_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARD_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

class PrefService;

namespace password_manager {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Needs to stay in sync with the
// PasswordManagerPromoCard enum in enums.xml.
enum class PromoCardType {
  // Password Checkup promo bubble.
  kCheckup = 0,
  // Password on the web promo bubble.
  kWebPasswordManager = 1,
  // Add shortcut promo bubble.
  kAddShortcut = 2,
  // Access passwords on iOS/Android promo bubble.
  kAccessOnAnyDevice = 3,
  // Relaunch Chrome to fix the keychain issue.
  kRelauchChrome = 4,
  // Move passwords stored only on this device to the account.
  kMovePasswords = 5,
  // Require reauth before accessing saved passwords.
  kScreenlockReauth = 6,
  kMaxValue = kScreenlockReauth,
};

// This is the base class for all password manager promo cards. It has a basic
// implementation to read/write to PrefService as well as basic properties
// needed for each promo card. Each subclass must override GetPromoID() and the
// content to be displayed.
class PasswordPromoCardBase {
 public:
  PasswordPromoCardBase(const PasswordPromoCardBase&) = delete;
  PasswordPromoCardBase& operator=(const PasswordPromoCardBase&) = delete;

  virtual ~PasswordPromoCardBase();

  // The upper limit on how many times Chrome will show the promo card.
  static constexpr int kPromoDisplayLimit = 3;

  // Unique ID for a promo card. This is also used by the WebUI to display
  // banner image.
  virtual std::string GetPromoID() const = 0;

  // Used to distinguish promo cards.
  virtual PromoCardType GetPromoCardType() const = 0;

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
  PasswordPromoCardBase(const std::string& id, PrefService* prefs);

  int number_of_times_shown_ = 0;
  base::Time last_time_shown_;
  bool was_dismissed_ = false;
  raw_ptr<PrefService> prefs_;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARD_H_
