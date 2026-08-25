// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_SERVICE_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class Profile;

namespace omnibox_everywhere {
class OmniboxEverywhereController;
class OmniboxEverywhereUIManager;
}

class OmniboxEverywhereService : public KeyedService {
 public:
  explicit OmniboxEverywhereService(Profile* profile);
  OmniboxEverywhereService(const OmniboxEverywhereService&) = delete;
  OmniboxEverywhereService& operator=(const OmniboxEverywhereService&) = delete;
  ~OmniboxEverywhereService() override;

  virtual void HidePopup();
  virtual bool IsPopupVisible() const;
  virtual void ShowProfilePicker();
  virtual void OnDrivePickerOpened();
  virtual void OnDrivePickerClosed();
  void OnScreensharePickerOpened();
  void OnScreensharePickerClosed();
  virtual void OpenUrl(const GURL& url,
                       WindowOpenDisposition disposition,
                       ui::PageTransition transition);

  // KeyedService:
  void Shutdown() override;

 private:
  omnibox_everywhere::OmniboxEverywhereController* controller() const;
  omnibox_everywhere::OmniboxEverywhereUIManager* ui_manager() const;

  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<OmniboxEverywhereService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_SERVICE_H_
