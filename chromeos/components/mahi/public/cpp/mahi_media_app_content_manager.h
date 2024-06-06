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

namespace ash {
class MahiMediaAppClient;
}

namespace aura {
class Window;
}
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
  // Although MahiMediaAppContentManager keeps the current active client id, we
  // make these functions have a `client_id` as input to allow the possibility
  // of retrieving info from given client.
  virtual std::optional<std::string> GetFileName(
      const base::UnguessableToken client_id) = 0;
  virtual void GetContent(const base::UnguessableToken client_id,
                          GetMediaAppContentCallback callback) = 0;

  // Forwards click of mahi context menu shown on the media app surface to mahi
  // manager, to show the pop up UI and request manta service accordingly.
  virtual void OnMahiContextMenuClicked(int64_t display_id,
                                        chromeos::mahi::ButtonType button_type,
                                        const std::u16string& question,
                                        const gfx::Rect& mahi_menu_bounds) = 0;

  // Client registration/removal.
  virtual void AddClient(base::UnguessableToken client_id,
                         ash::MahiMediaAppClient* client) = 0;
  virtual void RemoveClient(base::UnguessableToken client_id) = 0;

  // Whether a Window* is observed by `MahiMediaAppContentManager`. Callers may
  // suppress focus events of this window (i.e. not report to Mahi system) to
  // avoid overriding the media app pdf focus events.
  virtual bool ObservingWindow(const aura::Window* window) const = 0;

  // Tries to activate the `client_id`'s associated window.
  virtual bool ActivateClientWindow(const base::UnguessableToken client_id) = 0;

 protected:
  MahiMediaAppContentManager();

  // Keeps track of the current active client (i.e. media app window).
  base::UnguessableToken active_client_id_;
};

// A scoped object that set the global instance of
// `chromeos::MahiMediaAppEventsProxy::Get()` to the provided object pointer.
// The real instance will be restored when this scoped object is destructed.
// This class is used in testing and mocking.
class COMPONENT_EXPORT(MAHI_PUBLIC_CPP) ScopedMahiMediaAppContentManagerSetter {
 public:
  explicit ScopedMahiMediaAppContentManagerSetter(
      MahiMediaAppContentManager* proxy);
  ScopedMahiMediaAppContentManagerSetter(
      const ScopedMahiMediaAppContentManagerSetter&) = delete;
  ScopedMahiMediaAppContentManagerSetter& operator=(
      const ScopedMahiMediaAppContentManagerSetter&) = delete;
  ~ScopedMahiMediaAppContentManagerSetter();

 private:
  static ScopedMahiMediaAppContentManagerSetter* instance_;

  raw_ptr<MahiMediaAppContentManager> real_content_manager_instance_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_MEDIA_APP_CONTENT_MANAGER_H_
