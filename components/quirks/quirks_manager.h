// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUIRKS_QUIRKS_MANAGER_H_
#define COMPONENTS_QUIRKS_QUIRKS_MANAGER_H_

#include <memory>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/quirks/quirks_export.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class TaskRunner;
}

namespace quirks {

class QuirksClient;

// Callback when Quirks path request is complete.
// First parameter - path found, or empty if no file.
// Second parameter - true if file was just downloaded.
using RequestFinishedCallback =
    base::OnceCallback<void(const base::FilePath&, bool)>;

// Format int as hex string for filename.
QUIRKS_EXPORT std::string IdToHexString(int64_t product_id);

// Append ".icc" to hex string in filename.
QUIRKS_EXPORT std::string IdToFileName(int64_t product_id);

// Manages downloads of and requests for hardware calibration and configuration
// files ("Quirks").  The manager presents an external Quirks API, handles
// needed components from browser (local preferences, url context getter,
// blocking pool, etc), and owns clients and manages their life cycles.
class QUIRKS_EXPORT QuirksManager {
 public:
  static void Initialize(
      std::string api_key,
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  static void Shutdown();
  static QuirksManager* Get();
  static bool HasInstance();

  QuirksManager(const QuirksManager&) = delete;
  QuirksManager& operator=(const QuirksManager&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Signal to start queued downloads after login.
  void OnLoginCompleted();

  // Entry point into manager.  Finds or downloads icc file.
  void RequestIccProfilePath(int64_t product_id,
                             const std::string& display_name,
                             RequestFinishedCallback on_request_finished);

  void ClientFinished(QuirksClient* client);

  base::TaskRunner* task_runner() { return task_runner_.get(); }
  network::mojom::URLLoaderFactory* url_loader_factory() {
    return url_loader_factory_.get();
  }
  const base::FilePath& display_profile_path() const {
    return display_profile_path_;
  }

  void SetEnabled(bool enabled);

 protected:
  friend class QuirksBrowserTest;

  void SetURLLoaderFactoryForTests(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    url_loader_factory_ = std::move(url_loader_factory);
  }

 private:
  QuirksManager(
      std::string api_key,
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~QuirksManager();

  // Callback after checking for existing icc file; proceed if not found.
  void OnIccFilePathRequestCompleted(
      int64_t product_id,
      const std::string& display_name,
      RequestFinishedCallback on_request_finished,
      base::FilePath path);

  // Whether downloads allowed by cmd line flag and device policy.
  bool QuirksEnabled();

  // Records time of most recent server check.
  void SetLastServerCheck(int64_t product_id, const base::Time& last_check);

  // Set of active clients, each created to download a different Quirks file.
  std::set<std::unique_ptr<QuirksClient>, base::UniquePtrComparator> clients_;

  // Don't start downloads before first session login.
  bool waiting_for_login_;

  // Ensure this class runs on a single thread.
  base::ThreadChecker thread_checker_;

  // Path to the display_profiles directory.
  const base::FilePath display_profile_path_;

  // Google API key.
  const std::string api_key_;

  // These objects provide resources from the browser.
  scoped_refptr<base::TaskRunner> task_runner_;
  raw_ptr<PrefService> local_state_;  // For local prefs.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  bool enabled_ = true;

  // Factory for callbacks.
  base::WeakPtrFactory<QuirksManager> weak_ptr_factory_{this};
};

}  // namespace quirks

#endif  // COMPONENTS_QUIRKS_QUIRKS_MANAGER_H_
