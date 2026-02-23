// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_INTERNAL_SKILLS_DOWNLOADER_H_
#define COMPONENTS_SKILLS_INTERNAL_SKILLS_DOWNLOADER_H_
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "components/skills/proto/skill.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace skills {

inline constexpr char kSkillsDownloaderGstaticUrl[] =
    "https://www.gstatic.com/chrome/skills/first_party_skills_binary";

// SkillsDownloader downloads a list of 1P agent prompts that are accessible to
// users via chrome://skills/discover-skills.
class SkillsDownloader {
 public:
  // Map of id to skill.
  using SkillsMap = absl::flat_hash_map<std::string, skills::proto::Skill>;

  explicit SkillsDownloader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~SkillsDownloader();

  using OnFetchCompleteCallback =
      base::OnceCallback<void(std::unique_ptr<SkillsMap>)>;

  // Initiates async download process that calls callback on fetch complete.
  // Called on chrome://skills/discover-skills page load or attempt to save a
  // skill.
  void FetchDiscoverySkills(OnFetchCompleteCallback callback);

 private:
  // Parses string response into the correct format for OnFetchCompleteCallback
  // on download complete. Runs callback with nullptr in case of download or
  // parse errors.
  void OnUrlDownloadComplete(OnFetchCompleteCallback callback,
                             std::optional<std::string> response_body);

  // Stores information about the last time the file being fetched was modified.
  // Prevents refetching if no modification.
  std::string last_modified_header_;
  std::unique_ptr<network::SimpleURLLoader> pending_request_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<SkillsDownloader> self_ptr_factory_{this};
};

}  // namespace skills

#endif  // COMPONENTS_SKILLS_INTERNAL_SKILLS_DOWNLOADER_H_
