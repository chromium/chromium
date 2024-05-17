// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_MEDIA_APP_CONTENT_MANAGER_H_
#define CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_MEDIA_APP_CONTENT_MANAGER_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/unguessable_token.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_events_proxy.h"
#include "chromeos/components/mahi/public/cpp/mahi_util.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"

namespace chromeos {
using GetMediaAppContentCallback =
    base::OnceCallback<void(crosapi::mojom::MahiPageContentPtr)>;

// Interface that defines the central class that serves as media app PDF content
// provider for mahi feature.
class COMPONENT_EXPORT(MAHI_PUBLIC_CPP) MahiMediaAppContentManager {
 public:
  MahiMediaAppContentManager(const MahiMediaAppContentManager&) = delete;
  MahiMediaAppContentManager& operator=(const MahiMediaAppContentManager&) =
      delete;
  virtual ~MahiMediaAppContentManager();

  static MahiMediaAppContentManager* Get();

  base::UnguessableToken active_client_id() { return active_client_id_; }

  // Requests attributes / contents of current loaded PDF file in the media app.
  virtual std::u16string GetFileName(
      const base::UnguessableToken client_id) = 0;
  virtual void GetContent(const base::UnguessableToken client_id,
                          GetMediaAppContentCallback callback) = 0;
  // Forwards click of mahi context menu shown on the media app surface to mahi
  // manager, to show the pop up UI and request manta service accordingly.
  virtual void OnMahiContextMenuClicked(int64_t display_id,
                                        chromeos::mahi::ButtonType button_type,
                                        const std::u16string& question) = 0;

  // TODO(b/335741382): we need add/remove client functions for client
  // management.

 protected:
  MahiMediaAppContentManager();

  // Keeps track of the current active client (i.e. media app window).
  base::UnguessableToken active_client_id_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_MEDIA_APP_CONTENT_MANAGER_H_
