// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARD_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARD_H_

#include <string>

#include "base/time/time.h"

namespace password_manager {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Needs to stay in sync with the
// PasswordManagerNotificationCard enum in enums.xml.
// LINT.IfChange(NotificationCardType)
enum class NotificationCardType {
  // Password Checkup promo bubble.
  kCheckup = 0,
  // Password on the web promo bubble.
  kWebPasswordManager = 1,
  // Add shortcut promo bubble.
  kAddShortcut = 2,
  // Access passwords on iOS/Android promo bubble.
  kAccessOnAnyDevice = 3,
  // Relaunch Chrome to fix the keychain issue.
  kRelaunchChrome = 4,
  // Move passwords stored only on this device to the account.
  kMovePasswords = 5,
  // kScreenlockReauth = 6, Obsolete
  kMaxValue = kMovePasswords,
};
// LINT.ThenChange(//chrome/browser/resources/password_manager/notification_cards/notification_card.ts:NotificationCardMetricId)

enum class NotificationSeverity {
  kCritical = 0,
  kPromo = 1,
};

struct NotificationCardPrefState {
  bool was_dismissed = false;
  int number_of_times_shown = 0;
  base::Time last_time_shown;
};

// This is the base class for all password manager notification cards. Each
// subclass must override GetCardID() and the content to be displayed.
class PasswordNotificationCardBase {
 public:
  PasswordNotificationCardBase(const PasswordNotificationCardBase&) = delete;
  PasswordNotificationCardBase& operator=(const PasswordNotificationCardBase&) =
      delete;

  virtual ~PasswordNotificationCardBase();

  // The upper limit on how many times Chrome will show the notification card.
  static constexpr int kPromoDisplayLimit = 3;

  // Unique ID for a notification card. This is also used by the WebUI to
  // display banner image.
  virtual std::string GetCardID() const = 0;

  // Used to distinguish notification cards.
  virtual NotificationCardType GetNotificationCardType() const = 0;

  // Used to distinguish notification tiers (critical alerts vs standard
  // promos).
  virtual NotificationSeverity GetNotificationSeverity() const;

  // Whether promo can be shown given its preference state.
  virtual bool ShouldShowCard(
      const NotificationCardPrefState& pref_state) const = 0;

  // Whether the close ('X') button should be shown for this card.
  virtual bool IsDismissible() const;

  // Title of the notification card to be shown in the WebUI.
  virtual std::u16string GetTitle() const = 0;

  // Description of the notification card to be shown in the WebUI.
  virtual std::u16string GetDescription() const = 0;

  // Text for an actionable button if one exists. Returns empty string by
  // default.
  virtual std::u16string GetActionButtonText() const;

 protected:
  PasswordNotificationCardBase();
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARD_H_
