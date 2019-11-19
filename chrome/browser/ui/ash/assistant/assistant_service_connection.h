// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_SERVICE_CONNECTION_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_SERVICE_CONNECTION_H_

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "chromeos/services/assistant/service.h"

class Profile;

// AssistantServiceConnection exposes a Mojo interface connection to a Profile's
// own instance of the in-process Assistant service.
class AssistantServiceConnection : public base::SupportsUserData::Data,
                                   public ProfileObserver {
 public:
  explicit AssistantServiceConnection(Profile* profile);
  ~AssistantServiceConnection() override;

  static AssistantServiceConnection* GetForProfile(Profile* profile);

  chromeos::assistant::mojom::AssistantService* service() const {
    return remote_.get();
  }

 private:
  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  mojo::Remote<chromeos::assistant::mojom::AssistantService> remote_;
  chromeos::assistant::Service service_;
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(AssistantServiceConnection);
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_SERVICE_CONNECTION_H_
