// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DEVICE_SIGNALS_CONSENT_CONSENT_DIALOG_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_DEVICE_SIGNALS_CONSENT_CONSENT_DIALOG_COORDINATOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"

namespace ui {
class DialogModel;
}

namespace views {
class Widget;
}

class Browser;
class Profile;

using RequestConsentCallback = base::RepeatingCallback<void()>;

// Abstract base class that requests the collection of user consent for
// sharing device signals.
class ConsentRequester {
 public:
  virtual ~ConsentRequester() = default;

  ConsentRequester(const ConsentRequester&) = delete;
  ConsentRequester& operator=(const ConsentRequester&) = delete;

  static std::unique_ptr<ConsentRequester> CreateConsentRequester(
      Profile* profile);

  static void SetConsentRequesterForTest(
      std::unique_ptr<ConsentRequester> consent_requester);

  virtual void RequestConsent(RequestConsentCallback callback) = 0;

 protected:
  ConsentRequester() = default;
};

// Concrete implementation that manages the device signals consent dialog.
class ConsentDialogCoordinator : public ConsentRequester {
 public:
  ConsentDialogCoordinator(Browser* browser, Profile* profile);
  ~ConsentDialogCoordinator() override;

  ConsentDialogCoordinator(const ConsentDialogCoordinator&) = delete;
  ConsentDialogCoordinator& operator=(const ConsentDialogCoordinator&) = delete;

  void RequestConsent(RequestConsentCallback callback) override;

  // Retrieves the domain managing current profile, and formats the body text of
  // the consent dialog accordingly. If no domain is pulled, default body text
  // will be used.
  std::u16string GetDialogBodyText();

 private:
  std::unique_ptr<ui::DialogModel> CreateDeviceSignalsConsentDialogModel();

  void Show();

  void OnConsentDialogAccept();
  void OnConsentDialogCancel();
  void OnConsentDialogClose();

  void OnConsentPreferenceUpdated(RequestConsentCallback callback);

  raw_ptr<Browser> browser_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<views::Widget> dialog_widget_ = nullptr;
  PrefChangeRegistrar pref_observer_;

  base::WeakPtrFactory<ConsentDialogCoordinator> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DEVICE_SIGNALS_CONSENT_CONSENT_DIALOG_COORDINATOR_H_
