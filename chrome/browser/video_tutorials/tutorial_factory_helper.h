// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_TUTORIAL_FACTORY_HELPER_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_TUTORIAL_FACTORY_HELPER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class PrefService;

namespace video_tutorials {

class VideoTutorialService;

std::unique_ptr<VideoTutorialService> CreateVideoTutorialService(
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    const base::FilePath& storage_dir,
    const std::string& accepted_language,
    const std::string& country_code,
    const std::string& api_key,
    const std::string& client_version,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& default_server_url,
    PrefService* pref_service);

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_TUTORIAL_FACTORY_HELPER_H_
