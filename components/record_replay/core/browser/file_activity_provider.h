// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_FILE_ACTIVITY_PROVIDER_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_FILE_ACTIVITY_PROVIDER_H_

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/record_replay/core/browser/activity_provider.h"
#include "url/gurl.h"

namespace record_replay {

class FileActivityProvider : public ActivityProvider {
 public:
  explicit FileActivityProvider(const base::FilePath& file_path);
  ~FileActivityProvider() override;

  // ActivityProvider:
  void ShouldOfferActivity(
      const GURL& url,
      base::OnceCallback<
          void(std::optional<ActivityDiscoveryService::AutomationMetadata>)>
          callback) override;

 private:
  void OnFileLoaded(std::optional<std::string> file_content);
  void OnJsonParsed(std::vector<base::Value> values);
  std::optional<ActivityDiscoveryService::AutomationMetadata> GetMetadata(
      const GURL& url) const;

  base::flat_map<GURL, ActivityDiscoveryService::AutomationMetadata>
      metadata_map_;
  bool is_ready_ = false;
  std::vector<std::pair<
      GURL,
      base::OnceCallback<void(
          std::optional<ActivityDiscoveryService::AutomationMetadata>)>>>
      pending_queries_;

  base::WeakPtrFactory<FileActivityProvider> weak_ptr_factory_{this};
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_FILE_ACTIVITY_PROVIDER_H_
