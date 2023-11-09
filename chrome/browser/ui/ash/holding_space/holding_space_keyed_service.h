// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_client_impl.h"
#include "chrome/browser/ui/ash/thumbnail_loader.h"
#include "chromeos/crosapi/mojom/holding_space_service.mojom.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "url/gurl.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace ash {

class HoldingSpaceDownloadsDelegate;
class HoldingSpaceKeyedServiceDelegate;

// Browser context keyed service that:
// *   Manages the temporary holding space per-profile data model.
// *   Serves as an entry point to add holding space items from Chrome.
class HoldingSpaceKeyedService : public crosapi::mojom::HoldingSpaceService,
                                 public KeyedService,
                                 public ProfileManagerObserver,
                                 public chromeos::PowerManagerClient::Observer {
 public:
  HoldingSpaceKeyedService(Profile* profile, const AccountId& account_id);
  HoldingSpaceKeyedService(const HoldingSpaceKeyedService& other) = delete;
  HoldingSpaceKeyedService& operator=(const HoldingSpaceKeyedService& other) =
      delete;
  ~HoldingSpaceKeyedService() override;

  // Registers profile preferences for holding space.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Binds the holding space service to the specified pending `receiver`.
  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::HoldingSpaceService> receiver);

  // crosapi::mojom::HoldingSpaceKeyedService:
  // NOTE: No-op if the service has not been initialized.
  // TODO(http://b/274477308): Remove one-off API.
  void AddPrintedPdf(const base::FilePath& printed_pdf_path,
                     bool from_incognito_profile) override;

  // Adds multiple pinned file items identified by the provided file system
  // URLs. NOTE: No-op if the service has not been initialized.
  void AddPinnedFiles(
      const std::vector<storage::FileSystemURL>& file_system_urls);

  // Removes multiple pinned file items identified by the provided file system
  // URLs. NOTE: No-ops if:
  // 1. The specified files are not present in the holding space; OR
  // 2. The service has not been initialized.
  void RemovePinnedFiles(
      const std::vector<storage::FileSystemURL>& file_system_urls);

  // Returns whether the holding space contains a pinned file identified by a
  // file system URL.
  bool ContainsPinnedFile(const storage::FileSystemURL& file_system_url) const;

  // Returns the list of pinned files in the holding space. It returns the files
  // files system URLs as GURLs.
  std::vector<GURL> GetPinnedFiles() const;

  // Replaces the existing suggestions with `suggestions`. The order among
  // `suggestions` is respected, which means that if a suggestion A is in front
  // of a suggestion B in the given array, after calling this function, the
  // suggestion view of A is in front of the view of B. `suggestions` can be
  // empty. In this case, all the existing suggestions are cleared.
  // NOTE: No-op if the service has not been initialized.
  void SetSuggestions(
      const std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>&
          suggestions);

  // Adds the specified `item` to the holding space model. Returns the id of the
  // added holding space item. Returns an empty string if the item is not added
  // for either of the following reasons:
  // 1. The `item` is a duplicate.
  // 2. The service has not been initialized.
  const std::string& AddItem(std::unique_ptr<HoldingSpaceItem> item);

  // Adds an item of the specified `type` backed by the provided absolute
  // `file_path` to the holding space model. Returns the id of the added
  // holding space item. Returns an empty string if the item is not added
  // for either of the following reasons:
  // 1. The item to add is a duplicate.
  // 2. The service has not been initialized.
  const std::string& AddItemOfType(
      HoldingSpaceItem::Type type,
      const base::FilePath& file_path,
      const HoldingSpaceProgress& progress = HoldingSpaceProgress(),
      HoldingSpaceImage::PlaceholderImageSkiaResolver
          placeholder_image_skia_resolver = base::NullCallback());

  // Returns an object which, upon its destruction, performs an atomic update to
  // the holding space item associated with the specified `id`. Returns
  // `nullptr` if the service has not been initialized.
  std::unique_ptr<HoldingSpaceModel::ScopedItemUpdate> UpdateItem(
      const std::string& id);

  // Removes all holding space items directly from the model.
  // NOTE: No-op if the service has not been initialized.
  void RemoveAll();

  // Removes the holding space item with the specified `id` from the model.
  // NOTE: No-op if the service has not been initialized.
  void RemoveItem(const std::string& id);

  // Attempts to mark the specified holding space `item` to open when complete.
  // Returns `absl::nullopt` on success or the reason if the attempt was not
  // successful.
  absl::optional<holding_space_metrics::ItemFailureToLaunchReason>
  OpenItemWhenComplete(const HoldingSpaceItem* item);

  // Returns the `profile_` associated with this service.
  Profile* profile() { return profile_; }

  HoldingSpaceClient* client() { return &holding_space_client_; }

  const HoldingSpaceModel* model_for_testing() const {
    return &holding_space_model_;
  }

  ThumbnailLoader* thumbnail_loader_for_testing() { return &thumbnail_loader_; }

 private:
  // KeyedService:
  void Shutdown() override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // PowerManagerClient::Observer
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // Adds multiple `items` to the holding space model. `allow_duplicates`
  // indicates whether an item should be added to the model if it is duplicate
  // to an existing item. Returns the ids of the added holding space items or
  // empty strings where items were not added due to de-duplication checks.
  // NOTE: This function can be called during service initialization.
  std::vector<std::reference_wrapper<const std::string>> AddItems(
      std::vector<std::unique_ptr<HoldingSpaceItem>> items,
      bool allow_duplicates);

  // Invoked when the associated profile is ready.
  void OnProfileReady();

  // Creates and initializes holding space delegates. Called when the associated
  // profile finishes initialization, or when device suspend ends (the delegates
  // are shutdown during suspend).
  void InitializeDelegates();

  // Shuts down and destroys existing holding space delegates. Called on
  // profile shutdown, or when device suspend starts.
  void ShutdownDelegates();

  // Invoked when holding space persistence has been restored. Adds
  // `restored_items` to the holding space model and notifies delegates.
  void OnPersistenceRestored(
      std::vector<std::unique_ptr<HoldingSpaceItem>> restored_items);

  // Pin a drive file for offline access.
  void MakeDriveItemAvailableOffline(
      const storage::FileSystemURL& file_system_url);

  // Returns whether the service has been initialized. The service is considered
  // initialized if:
  // 1. All delegates have been created.
  // 2. Persistence restoration has been completed.
  bool IsInitialized() const;

  // Creates an item of the specified `type` backed by the provided absolute
  // `file_path`. Returns an empty unique pointer if the file url cannot be
  // resolved.
  std::unique_ptr<HoldingSpaceItem> CreateItemOfType(
      HoldingSpaceItem::Type type,
      const base::FilePath& file_path,
      const HoldingSpaceProgress& progress,
      HoldingSpaceImage::PlaceholderImageSkiaResolver
          placeholder_image_skia_resolver);

  const raw_ptr<Profile, ExperimentalAsh> profile_;
  const AccountId account_id_;

  HoldingSpaceClientImpl holding_space_client_;
  HoldingSpaceModel holding_space_model_;

  ThumbnailLoader thumbnail_loader_;

  // The `HoldingSpaceKeyedService` owns a collection of `delegates_` which are
  // each tasked with an independent area of responsibility on behalf of the
  // service. They operate autonomously of one another.
  std::vector<std::unique_ptr<HoldingSpaceKeyedServiceDelegate>> delegates_;

  // The delegate, owned by `delegates_`, responsible for downloads.
  raw_ptr<HoldingSpaceDownloadsDelegate, ExperimentalAsh> downloads_delegate_ =
      nullptr;

  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<crosapi::mojom::HoldingSpaceService> receivers_;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observer_{this};

  base::WeakPtrFactory<HoldingSpaceKeyedService> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_H_
