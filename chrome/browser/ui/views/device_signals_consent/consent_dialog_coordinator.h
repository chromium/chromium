// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DEVICE_SIGNALS_CONSENT_CONSENT_DIALOG_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_DEVICE_SIGNALS_CONSENT_CONSENT_DIALOG_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/device_signals_consent/consent_requester.h"
#include "components/prefs/pref_change_registrar.h"

namespace ui {
class DialogModel;
}

namespace views {
class Widget;
}

class Browser;
class Profile;

// View class for managing the device signals consent dialog widget.
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
